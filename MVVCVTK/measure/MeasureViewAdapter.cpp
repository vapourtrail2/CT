#include "measure/MeasureViewAdapter.h"

#include "measure/MeasurementGeometry.h"
#include "measure/MeasurementView.h"

#include <QVTKOpenGLNativeWidget.h>

#include <vtkCallbackCommand.h>
#include <vtkCamera.h>
#include <vtkCommand.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkImageData.h>
#include <vtkImageProperty.h>
#include <vtkImageSlice.h>
#include <vtkImageSliceMapper.h>
#include <vtkMatrix4x4.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>

#include <algorithm>
#include <cmath>
#include <utility>

namespace measure {

Point3 MeasureViewAdapter::GetMatrixPoint(
    vtkMatrix4x4* matrix,
    const Point3& point,
    double w)
{
    if (!matrix) {
        return point;
    }

    const double source[4] = { point[0], point[1], point[2], w };
    double target[4] = {};
    matrix->MultiplyPoint(source, target);
    if (w != 0.0 && std::abs(target[3]) > 1e-12) {
        return {
            target[0] / target[3],
            target[1] / target[3],
            target[2] / target[3]
        };
    }
    return { target[0], target[1], target[2] };
}

InteractionEventKind MeasureViewAdapter::GetEventKind(unsigned long eventId)
{
    switch (eventId) {
    case vtkCommand::LeftButtonPressEvent:
        return InteractionEventKind::PrimaryPress;
    case vtkCommand::LeftButtonReleaseEvent:
        return InteractionEventKind::PrimaryRelease;
    case vtkCommand::RightButtonPressEvent:
        return InteractionEventKind::SecondaryPress;
    case vtkCommand::RightButtonReleaseEvent:
        return InteractionEventKind::SecondaryRelease;
    case vtkCommand::MouseMoveEvent:
        return InteractionEventKind::PointerMove;
    case vtkCommand::MouseWheelForwardEvent:
        return InteractionEventKind::WheelForward;
    case vtkCommand::MouseWheelBackwardEvent:
        return InteractionEventKind::WheelBackward;
    case vtkCommand::KeyPressEvent:
        return InteractionEventKind::KeyPress;
    case vtkCommand::KeyReleaseEvent:
        return InteractionEventKind::KeyRelease;
    default:
        return InteractionEventKind::None;
    }
}

MeasureViewAdapter::MeasureViewAdapter() = default;

MeasureViewAdapter::~MeasureViewAdapter()
{
    Reset();
}

bool MeasureViewAdapter::Build(
    QVTKOpenGLNativeWidget* widget,
    TrustedImageSnapshot image,
    MeasureView view,
    const MeasurementViewInitState& initialState)
{
    Reset();
    if (!widget || !image || !image->image) {
        return false;
    }

    m_widget = widget;
    m_image = std::move(image);
    m_renderWindow = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
    m_renderer = vtkSmartPointer<vtkRenderer>::New();
    m_sliceMapper = vtkSmartPointer<vtkImageSliceMapper>::New();
    m_slice = vtkSmartPointer<vtkImageSlice>::New();
    m_modelToWorld = vtkSmartPointer<vtkMatrix4x4>::New();
    m_worldToModel = vtkSmartPointer<vtkMatrix4x4>::New();

    constexpr std::array<double, 16> identityMatrix = {
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        0.0, 0.0, 0.0, 1.0
    };
    const auto& matrix = initialState.modelMatrix
        ? *initialState.modelMatrix
        : identityMatrix;
    const bool isMatrixFinite = std::all_of(
        matrix.begin(),
        matrix.end(),
        [](double value) { return std::isfinite(value); });
    if (!isMatrixFinite) {
        Reset();
        return false;
    }
    m_modelToWorld->DeepCopy(matrix.data());
    if (std::abs(m_modelToWorld->Determinant()) <= 1e-12) {
        Reset();
        return false;
    }
    m_worldToModel->DeepCopy(m_modelToWorld);
    m_worldToModel->Invert();

    m_widget->setRenderWindow(m_renderWindow);
    m_renderWindow->AddRenderer(m_renderer);
    m_sliceMapper->SetInputData(m_image->image);
    m_slice->SetMapper(m_sliceMapper);
    m_slice->SetUserMatrix(m_modelToWorld);
    m_slice->GetProperty()->SetInterpolationTypeToLinear();
    m_renderer->AddViewProp(m_slice);

    const auto scalarRange = m_image->scalarRange;
    const double defaultWidth =
        std::max(0.01, scalarRange[1] - scalarRange[0]);
    const double defaultCenter =
        (scalarRange[0] + scalarRange[1]) * 0.5;
    m_windowLevel.windowWidth = initialState.windowLevel
        ? std::max(0.01, (*initialState.windowLevel)[0])
        : defaultWidth;
    m_windowLevel.windowCenter = initialState.windowLevel
        ? (*initialState.windowLevel)[1]
        : defaultCenter;
    m_slice->GetProperty()->SetColorWindow(m_windowLevel.windowWidth);
    m_slice->GetProperty()->SetColorLevel(m_windowLevel.windowCenter);

    const auto background = initialState.background.value_or(
        std::array<double, 3>{ 0.1, 0.1, 0.1 });
    m_renderer->SetBackground(
        background[0], background[1], background[2]);

    if (initialState.cursorWorld) {
        m_cursorWorld = *initialState.cursorWorld;
    }
    else {
        double bounds[6] = {};
        m_image->image->GetBounds(bounds);
        m_cursorWorld = GetWorldPoint({
            (bounds[0] + bounds[1]) * 0.5,
            (bounds[2] + bounds[3]) * 0.5,
            (bounds[4] + bounds[5]) * 0.5
        });
    }

    if (!SetSlice(view)) {
        Reset();
        return false;
    }

    if (!AttachInput()) {
        Reset();
        return false;
    }
    SendRender();
    return true;
}

void MeasureViewAdapter::Reset()
{
    DetachInput();
    m_inputCallback = {};

    if (m_renderer && m_slice) {
        m_renderer->RemoveViewProp(m_slice);
    }
    if (m_renderWindow && m_renderer) {
        m_renderWindow->RemoveRenderer(m_renderer);
    }

    m_slice = nullptr;
    m_sliceMapper = nullptr;
    m_renderer = nullptr;
    m_renderWindow = nullptr;
    m_modelToWorld = nullptr;
    m_worldToModel = nullptr;
    m_image.reset();
    m_widget.clear();
    m_cursorWorld = {};
    m_windowLevel = {};
    m_view = MeasureView::Axial;
}

bool MeasureViewAdapter::SetView(MeasureView view)
{
    if (!GetIsReady()) {
        return false;
    }
    return SetSlice(view);
}

void MeasureViewAdapter::SetInputCallback(InputCallback callback)
{
    m_inputCallback = std::move(callback);
}

void MeasureViewAdapter::SendRender()
{
    if (m_renderWindow) {
        m_renderWindow->Render();
    }
    if (m_widget) {
        m_widget->update();
    }
}

bool MeasureViewAdapter::GetIsReady() const noexcept
{
    return m_widget
        && m_image
        && m_image->image
        && m_renderWindow
        && m_renderer
        && m_sliceMapper
        && m_slice
        && m_modelToWorld
        && m_worldToModel;
}

TrustedImageSnapshot MeasureViewAdapter::GetImageSnapshot() const
{
    return m_image;
}

vtkRenderer* MeasureViewAdapter::GetRenderer() const noexcept
{
    return m_renderer;
}

std::array<double, 3> MeasureViewAdapter::GetCursorWorld() const noexcept
{
    return m_cursorWorld;
}

HostWindowLevelParams MeasureViewAdapter::GetWindowLevel() const noexcept
{
    return m_windowLevel;
}

Point3 MeasureViewAdapter::GetWorldPoint(const Point3& modelPoint) const
{
    return GetMatrixPoint(m_modelToWorld, modelPoint, 1.0);
}

Point3 MeasureViewAdapter::GetModelPoint(const Point3& worldPoint) const
{
    return GetMatrixPoint(m_worldToModel, worldPoint, 1.0);
}

Point3 MeasureViewAdapter::GetWorldVector(const Point3& modelVector) const
{
    const auto vector = GetMatrixPoint(m_modelToWorld, modelVector, 0.0);
    return geometry::Normalized(vector).value_or(modelVector);
}

std::optional<Point3> MeasureViewAdapter::GetDisplayModel(int x, int y) const
{
    if (!GetIsReady()) {
        return std::nullopt;
    }

    const auto getWorldPoint = [this, x, y](double z)
        -> std::optional<Point3> {
        m_renderer->SetDisplayPoint(
            static_cast<double>(x),
            static_cast<double>(y),
            z);
        m_renderer->DisplayToWorld();
        const double* world = m_renderer->GetWorldPoint();
        if (!world || std::abs(world[3]) <= 1e-12) {
            return std::nullopt;
        }
        return Point3{
            world[0] / world[3],
            world[1] / world[3],
            world[2] / world[3]
        };
    };

    const auto nearPoint = getWorldPoint(0.0);
    const auto farPoint = getWorldPoint(1.0);
    if (!nearPoint || !farPoint) {
        return std::nullopt;
    }

    const Point3 direction = geometry::Subtract(*farPoint, *nearPoint);
    const Point3 normal = GetWorldVector(
        GetSliceViewDescriptor(m_view).normal);
    const double denominator = geometry::Dot(normal, direction);
    if (std::abs(denominator) <= 1e-12) {
        return std::nullopt;
    }

    const Point3 planeOrigin{
        m_cursorWorld[0], m_cursorWorld[1], m_cursorWorld[2]
    };
    const double distance = geometry::Dot(
        normal,
        geometry::Subtract(planeOrigin, *nearPoint)) / denominator;
    return GetModelPoint(geometry::Add(
        *nearPoint,
        geometry::Scale(direction, distance)));
}

void MeasureViewAdapter::OnInput(
    vtkObject* caller,
    unsigned long eventId,
    void* clientData,
    void*)
{
    auto* adapter = static_cast<MeasureViewAdapter*>(clientData);
    if (adapter) {
        adapter->SendInput(caller, eventId);
    }
}

bool MeasureViewAdapter::SetSlice(MeasureView view)
{
    if (!GetIsReady()) {
        return false;
    }

    const auto& descriptor = GetSliceViewDescriptor(view);
    const Point3 cursorModel = GetModelPoint({
        m_cursorWorld[0], m_cursorWorld[1], m_cursorWorld[2]
    });
    double modelPoint[3] = {
        cursorModel[0], cursorModel[1], cursorModel[2]
    };
    double continuousIndex[3] = {};
    m_image->image->TransformPhysicalPointToContinuousIndex(
        modelPoint, continuousIndex);

    int extent[6] = {};
    m_image->image->GetExtent(extent);
    const int axis = descriptor.axis;
    const int sliceIndex = std::clamp(
        static_cast<int>(std::lround(continuousIndex[axis])),
        extent[axis * 2],
        extent[axis * 2 + 1]);
    continuousIndex[axis] = sliceIndex;

    double sliceModel[3] = {};
    m_image->image->TransformContinuousIndexToPhysicalPoint(
        continuousIndex, sliceModel);
    m_cursorWorld = GetWorldPoint({
        sliceModel[0], sliceModel[1], sliceModel[2]
    });

    m_view = view;
    m_sliceMapper->SetOrientation(axis);
    m_sliceMapper->SetSliceNumber(sliceIndex);
    SetCamera(view);
    return true;
}

void MeasureViewAdapter::SetCamera(MeasureView view)
{
    if (!m_renderer || !m_image || !m_image->image) {
        return;
    }

    const auto& descriptor = GetSliceViewDescriptor(view);
    const Point3 normal = GetWorldVector(descriptor.normal);
    const Point3 viewUp = GetWorldVector(descriptor.v);
    const Point3 focalPoint{
        m_cursorWorld[0], m_cursorWorld[1], m_cursorWorld[2]
    };

    double bounds[6] = {};
    m_image->image->GetBounds(bounds);
    const double diagonal = std::max(1.0, std::sqrt(
        std::pow(bounds[1] - bounds[0], 2.0)
        + std::pow(bounds[3] - bounds[2], 2.0)
        + std::pow(bounds[5] - bounds[4], 2.0)));
    const Point3 cameraPosition = geometry::Add(
        focalPoint,
        geometry::Scale(normal, diagonal * 2.0));

    auto* camera = m_renderer->GetActiveCamera();
    camera->ParallelProjectionOn();
    camera->SetFocalPoint(focalPoint.data());
    camera->SetPosition(cameraPosition.data());
    camera->SetViewUp(viewUp.data());
    m_renderer->ResetCamera();
    m_renderer->ResetCameraClippingRange();
}

bool MeasureViewAdapter::AttachInput()
{
    DetachInput();
    if (!m_renderWindow || !m_renderWindow->GetInteractor()) {
        return false;
    }

    m_inputCommand = vtkSmartPointer<vtkCallbackCommand>::New();
    m_inputCommand->SetClientData(this);
    m_inputCommand->SetCallback(&MeasureViewAdapter::OnInput);

    auto* interactor = m_renderWindow->GetInteractor();
    for (const auto eventId : {
        vtkCommand::LeftButtonPressEvent,
        vtkCommand::LeftButtonReleaseEvent,
        vtkCommand::RightButtonPressEvent,
        vtkCommand::RightButtonReleaseEvent,
        vtkCommand::MouseMoveEvent,
        vtkCommand::MouseWheelForwardEvent,
        vtkCommand::MouseWheelBackwardEvent,
        vtkCommand::KeyPressEvent,
        vtkCommand::KeyReleaseEvent }) {
        m_inputTags.push_back(interactor->AddObserver(
            eventId, m_inputCommand, 1.0));
    }
    return m_inputTags.size() == 9;
}

void MeasureViewAdapter::DetachInput()
{
    if (m_renderWindow && m_renderWindow->GetInteractor()) {
        auto* interactor = m_renderWindow->GetInteractor();
        for (const auto tag : m_inputTags) {
            interactor->RemoveObserver(tag);
        }
    }
    m_inputTags.clear();
    m_inputCommand = nullptr;
}

void MeasureViewAdapter::SendInput(vtkObject* caller, unsigned long eventId)
{
    auto* interactor = vtkRenderWindowInteractor::SafeDownCast(caller);
    if (!interactor || !m_inputCallback) {
        return;
    }

    InteractionEvent event;
    event.eventKind = GetEventKind(eventId);
    if (event.eventKind == InteractionEventKind::None) {
        return;
    }

    const int* position = interactor->GetEventPosition();
    event.x = position ? position[0] : 0;
    event.y = position ? position[1] : 0;
    event.isCtrlDown = interactor->GetControlKey() != 0;
    event.isAltDown = interactor->GetAltKey() != 0;
    event.isShiftDown = interactor->GetShiftKey() != 0;
    event.keyCode = interactor->GetKeyCode();
    if (const char* keySym = interactor->GetKeySym()) {
        event.keySym = keySym;
    }

    const auto result = m_inputCallback(event);
    if (m_inputCommand) {
        m_inputCommand->SetAbortFlag(
            result.isPropagationStopped ? 1 : 0);
    }
}

} // namespace measure
