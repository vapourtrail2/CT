#include "measure/EdgeCaptureController.h"

#include "App/AppInterfaces.h"
#include "measure/MeasurementGeometry.h"
#include "measure/MeasurementView.h"

#include <vtkActor.h>
#include <vtkCellArray.h>
#include <vtkImageData.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>

namespace measure {
namespace {

std::size_t ViewIndex(MeasureView view)
{
    return static_cast<std::size_t>(view);
}

void SetIndexAxes(MeasureView view, int& fixedAxis, int& uAxis, int& vAxis)
{
    switch (view) {
    case MeasureView::Axial:
        fixedAxis = 2;
        uAxis = 0;
        vAxis = 1;
        break;
    case MeasureView::Coronal:
        fixedAxis = 1;
        uAxis = 0;
        vAxis = 2;
        break;
    case MeasureView::Sagittal:
        fixedAxis = 0;
        uAxis = 1;
        vAxis = 2;
        break;
    }
}

double Clamp(double value, double minimum, double maximum)
{
    return std::max(minimum, std::min(value, maximum));
}

} 

EdgeCaptureController::EdgeCaptureController(
    std::shared_ptr<AbstractDataManager> dataManager,
    InteractiveService* service,
    vtkRenderer* renderer,
    MeasureView view)
    : m_dataManager(std::move(dataManager))
    , m_service(service)
    , m_renderer(renderer)
    , m_view(view)
{
}

EdgeCaptureController::~EdgeCaptureController()
{
    RemoveProps();
}

InteractionResult EdgeCaptureController::Send(const InteractionEvent& event)
{
    if (!m_enabled || !m_renderer || !m_service || !m_dataManager) {
        return {};
    }

    if (event.eventKind == InteractionEventKind::PrimaryPress) {
        m_consumingLeftButton = true;
        m_dragMode = DragMode::None;
        if (!EnsureDefaultRoi()) {
            return { true, true };
        }

        const auto index = DisplayToContinuousIndex(event.x, event.y);
        if (!index) {
            return { true, true };
        }

        ImageGeometry geometry;
        if (!GetImageGeometry(geometry)) {
            return { true, true };
        }
        const double u = (*index)[geometry.uAxis];
        const double v = (*index)[geometry.vAxis];
        auto& state = CurrentState();

        constexpr double handleRadius = 14.0;
        constexpr double handleRadiusSquared = handleRadius * handleRadius;
        const std::array<std::array<double, 2>, 4> corners = {
            IndexToDisplay(state.minU, state.minV, state.fixedIndex),
            IndexToDisplay(state.maxU, state.minV, state.fixedIndex),
            IndexToDisplay(state.maxU, state.maxV, state.fixedIndex),
            IndexToDisplay(state.minU, state.maxV, state.fixedIndex)
        };
        for (std::size_t corner = 0; corner < corners.size(); ++corner) {
            const double dx = corners[corner][0] - event.x;
            const double dy = corners[corner][1] - event.y;
            if (dx * dx + dy * dy <= handleRadiusSquared) {
                m_dragMode = DragMode::Resize;
                m_resizeUHigh = corner == 1 || corner == 2;
                m_resizeVHigh = corner >= 2;
                break;
            }
        }

        if (m_dragMode == DragMode::None
            && u >= state.minU && u <= state.maxU
            && v >= state.minV && v <= state.maxV) {
            m_dragMode = DragMode::Move;
        }

        if (m_dragMode != DragMode::None) {
            m_pressU = u;
            m_pressV = v;
            m_dragStart = state;
            state.result.reset();
            Refresh();
        }
        return { true, true };
    }

    if (event.eventKind == InteractionEventKind::PointerMove
        && m_consumingLeftButton
        && m_dragMode != DragMode::None) {
        const auto index = DisplayToContinuousIndex(event.x, event.y);
        ImageGeometry geometry;
        if (!index || !GetImageGeometry(geometry)) {
            return { true, true };
        }

        const double uMinimum = geometry.extent[2 * geometry.uAxis];
        const double uMaximum = geometry.extent[2 * geometry.uAxis + 1];
        const double vMinimum = geometry.extent[2 * geometry.vAxis];
        const double vMaximum = geometry.extent[2 * geometry.vAxis + 1];
        const double u = (*index)[geometry.uAxis];
        const double v = (*index)[geometry.vAxis];
        auto& state = CurrentState();

        if (m_dragMode == DragMode::Move) {
            const double width = m_dragStart.maxU - m_dragStart.minU;
            const double height = m_dragStart.maxV - m_dragStart.minV;
            const double newMinU = Clamp(
                m_dragStart.minU + u - m_pressU,
                uMinimum,
                uMaximum - width);
            const double newMinV = Clamp(
                m_dragStart.minV + v - m_pressV,
                vMinimum,
                vMaximum - height);
            state.minU = newMinU;
            state.maxU = newMinU + width;
            state.minV = newMinV;
            state.maxV = newMinV + height;
        }
        else {
            constexpr double minimumSize = 4.0;
            if (m_resizeUHigh) {
                state.maxU = Clamp(u, state.minU + minimumSize, uMaximum);
            }
            else {
                state.minU = Clamp(u, uMinimum, state.maxU - minimumSize);
            }
            if (m_resizeVHigh) {
                state.maxV = Clamp(v, state.minV + minimumSize, vMaximum);
            }
            else {
                state.minV = Clamp(v, vMinimum, state.maxV - minimumSize);
            }
        }
        state.result.reset();
        Refresh();
        return { true, true };
    }

    if (event.eventKind == InteractionEventKind::PrimaryRelease
        && m_consumingLeftButton) {
        const bool shouldMeasure = m_dragMode != DragMode::None;
        m_consumingLeftButton = false;
        m_dragMode = DragMode::None;
        if (shouldMeasure) {
            RunMeasurement();
        }
        return { true, true };
    }

    return {};
}

void EdgeCaptureController::SetEnabled(bool enabled)
{
    m_enabled = enabled;
    m_consumingLeftButton = false;
    m_dragMode = DragMode::None;
    if (enabled) {
        if (EnsureDefaultRoi()) {
            Report("拖动矩形或四角控制点，松开鼠标后执行抓边。");
        }
    }
    Refresh();
}

bool EdgeCaptureController::IsEnabled() const
{
    return m_enabled;
}

void EdgeCaptureController::SetView(MeasureView view)
{
    m_view = view;
    m_consumingLeftButton = false;
    m_dragMode = DragMode::None;
    if (m_enabled) {
        EnsureDefaultRoi();
    }
    Refresh();
}

void EdgeCaptureController::SetStatusCallback(StatusCallback callback)
{
    m_statusCallback = std::move(callback);
}

void EdgeCaptureController::Refresh()
{
    RemoveProps();
    const auto& state = CurrentState();
    if (!state.initialized || !m_renderer || !m_service) {
        RequestRender();
        return;
    }

    const std::vector<Point3> physicalCorners = {
        IndexToPhysical(state.minU, state.minV, state.fixedIndex),
        IndexToPhysical(state.maxU, state.minV, state.fixedIndex),
        IndexToPhysical(state.maxU, state.maxV, state.fixedIndex),
        IndexToPhysical(state.minU, state.maxV, state.fixedIndex)
    };
    std::vector<Point3> worldCorners;
    worldCorners.reserve(5);
    for (const auto& point : physicalCorners) {
        worldCorners.push_back(PhysicalToWorld(point));
    }
    worldCorners.push_back(worldCorners.front());
    AddPath(worldCorners, 1.0, 0.75, 0.05, 2.0);

    std::vector<Point3> handlePoints(worldCorners.begin(), worldCorners.end() - 1);
    AddHandles(handlePoints);

    if (state.result) {
        ImageGeometry geometry;
        if (GetImageGeometry(geometry)) {
            const auto toIndex = [&geometry](double x, double y) {
                const double u = geometry.extent[2 * geometry.uAxis] + x;
                const double v = geometry.extent[2 * geometry.vAxis + 1] - y;
                return std::array<double, 2>{ u, v };
            };
            const auto p1 = toIndex(state.result->x1, state.result->y1);
            const auto p2 = toIndex(state.result->x2, state.result->y2);
            AddPath({
                PhysicalToWorld(IndexToPhysical(p1[0], p1[1], state.fixedIndex)),
                PhysicalToWorld(IndexToPhysical(p2[0], p2[1], state.fixedIndex))
                }, 0.1, 1.0, 0.35, 3.0);
        }
    }
    RequestRender();
}

EdgeCaptureController::ViewState& EdgeCaptureController::CurrentState()
{
    return m_viewStates[ViewIndex(m_view)];
}

const EdgeCaptureController::ViewState& EdgeCaptureController::CurrentState() const
{
    return m_viewStates[ViewIndex(m_view)];
}

bool EdgeCaptureController::GetImageGeometry(ImageGeometry& geometry) const
{
    if (!m_dataManager || !m_service) {
        return false;
    }
    const auto snapshot = m_dataManager->GetImageSnapshot();
    if (!snapshot || !snapshot->image) {
        return false;
    }
    snapshot->image->GetExtent(geometry.extent);
    SetIndexAxes(m_view, geometry.fixedAxis, geometry.uAxis, geometry.vAxis);
    geometry.width = geometry.extent[2 * geometry.uAxis + 1]
        - geometry.extent[2 * geometry.uAxis] + 1;
    geometry.height = geometry.extent[2 * geometry.vAxis + 1]
        - geometry.extent[2 * geometry.vAxis] + 1;

    const auto cursor = m_service->GetCursorWorld();
    double world[3] = { cursor[0], cursor[1], cursor[2] };
    double physical[3] = { 0.0, 0.0, 0.0 };
    double index[3] = { 0.0, 0.0, 0.0 };
    m_service->GetModelPositionFromWorld(world, physical);
    snapshot->image->TransformPhysicalPointToContinuousIndex(physical, index);
    geometry.fixedIndex = static_cast<int>(std::lround(index[geometry.fixedAxis]));
    geometry.fixedIndex = std::max(
        geometry.extent[2 * geometry.fixedAxis],
        std::min(
            geometry.fixedIndex,
            geometry.extent[2 * geometry.fixedAxis + 1]));
    return geometry.width > 1 && geometry.height > 1;
}

bool EdgeCaptureController::EnsureDefaultRoi()//计算矩形位置
{
    auto& state = CurrentState();
    if (state.initialized) {
        return true;
    }
    ImageGeometry geometry;
    if (!GetImageGeometry(geometry)) {
        Report("无法建立抓边矩形：当前切片不可用。");
        return false;
    }
    const double uMinimum = geometry.extent[2 * geometry.uAxis];
    const double uMaximum = geometry.extent[2 * geometry.uAxis + 1];
    const double vMinimum = geometry.extent[2 * geometry.vAxis];
    const double vMaximum = geometry.extent[2 * geometry.vAxis + 1];
    const double uSpan = uMaximum - uMinimum;
    const double vSpan = vMaximum - vMinimum;
    state.fixedIndex = geometry.fixedIndex;
    state.minU = uMinimum + uSpan * 0.30;
    state.maxU = uMinimum + uSpan * 0.70;
    state.minV = vMinimum + vSpan * 0.30;
    state.maxV = vMinimum + vSpan * 0.70;
    state.initialized = true;
    return true;
}

std::optional<std::array<double, 3>>
EdgeCaptureController::DisplayToContinuousIndex(int x, int y) const
{
    if (!m_renderer || !m_service || !m_dataManager) {
        return std::nullopt;
    }
    const auto displayToWorld = [this, x, y](double z)
        -> std::optional<Point3> {
        m_renderer->SetDisplayPoint(
            static_cast<double>(x),
            static_cast<double>(y),
            z);
        m_renderer->DisplayToWorld();
        double* value = m_renderer->GetWorldPoint();
        if (!value || std::abs(value[3]) <= 1e-12) {
            return std::nullopt;
        }
        return Point3{
            value[0] / value[3],
            value[1] / value[3],
            value[2] / value[3]
        };
    };

    const auto nearPoint = displayToWorld(0.0);
    const auto farPoint = displayToWorld(1.0);
    if (!nearPoint || !farPoint) {
        return std::nullopt;
    }
    const Point3 direction = geometry::Subtract(*farPoint, *nearPoint);
    const Point3 normal = GetSliceViewDescriptor(m_view).normal;
    const double denominator = geometry::Dot(normal, direction);
    if (std::abs(denominator) <= 1e-12) {
        return std::nullopt;
    }
    const auto cursor = m_service->GetCursorWorld();
    const Point3 planeOrigin{ cursor[0], cursor[1], cursor[2] };
    const double distance = geometry::Dot(
        normal,
        geometry::Subtract(planeOrigin, *nearPoint)) / denominator;
    const Point3 worldPoint = geometry::Add(
        *nearPoint,
        geometry::Scale(direction, distance));

    double world[3] = { worldPoint[0], worldPoint[1], worldPoint[2] };
    double physical[3] = { 0.0, 0.0, 0.0 };
    double index[3] = { 0.0, 0.0, 0.0 };
    m_service->GetModelPositionFromWorld(world, physical);
    const auto snapshot = m_dataManager->GetImageSnapshot();
    if (!snapshot || !snapshot->image) {
        return std::nullopt;
    }
    snapshot->image->TransformPhysicalPointToContinuousIndex(physical, index);

    int extent[6];
    snapshot->image->GetExtent(extent);
    constexpr double margin = 0.5;
    for (int axis = 0; axis < 3; ++axis) {
        if (index[axis] < extent[2 * axis] - margin
            || index[axis] > extent[2 * axis + 1] + margin) {
            return std::nullopt;
        }
    }
    return std::array<double, 3>{ index[0], index[1], index[2] };
}

Point3 EdgeCaptureController::IndexToPhysical(
    double u,
    double v,
    int fixedIndex) const
{
    ImageGeometry geometry;
    if (!GetImageGeometry(geometry)) {
        return {};
    }
    double index[3] = { 0.0, 0.0, 0.0 };
    index[geometry.fixedAxis] = fixedIndex;
    index[geometry.uAxis] = u;
    index[geometry.vAxis] = v;
    double physical[3] = { 0.0, 0.0, 0.0 };
    const auto snapshot = m_dataManager->GetImageSnapshot();
    snapshot->image->TransformContinuousIndexToPhysicalPoint(index, physical);
    return { physical[0], physical[1], physical[2] };
}

Point3 EdgeCaptureController::PhysicalToWorld(const Point3& physical) const
{
    double source[3] = { physical[0], physical[1], physical[2] };
    double target[3] = { 0.0, 0.0, 0.0 };
    m_service->GetWorldPositionFromModel(source, target);
    const Point3 normal = GetSliceViewDescriptor(m_view).normal;
    constexpr double safeOffset = 0.02;
    return {
        target[0] + normal[0] * safeOffset,
        target[1] + normal[1] * safeOffset,
        target[2] + normal[2] * safeOffset
    };
}

std::array<double, 2> EdgeCaptureController::IndexToDisplay(
    double u,
    double v,
    int fixedIndex) const
{
    const Point3 world = PhysicalToWorld(IndexToPhysical(u, v, fixedIndex));
    m_renderer->SetWorldPoint(world[0], world[1], world[2], 1.0);
    m_renderer->WorldToDisplay();
    const double* display = m_renderer->GetDisplayPoint();
    return { display[0], display[1] };
}

bool EdgeCaptureController::BuildGraySlice(
    const ImageGeometry& geometry,
    ZcGrayImage& gray) const
{
    const auto snapshot = m_dataManager->GetImageSnapshot();
    if (!snapshot || !snapshot->image) {
        return false;
    }
    gray.width = geometry.width;
    gray.height = geometry.height;
    gray.widthStep = (gray.width + 3) & ~3;
    gray.pixels.assign(
        static_cast<std::size_t>(gray.widthStep) * gray.height,
        0);

    const auto windowLevel = m_service->GetWindowLevel();
    const double safeWindow = std::max(windowLevel.windowWidth, 1e-6);
    const double windowMinimum = windowLevel.windowCenter - safeWindow * 0.5;
    for (int row = 0; row < gray.height; ++row) {
        // IplImage 原点在左上；VTK 当前视图的 +V 方向在屏幕向上。
        const int vIndex = geometry.extent[2 * geometry.vAxis + 1] - row;
        for (int column = 0; column < gray.width; ++column) {
            int index[3] = { 0, 0, 0 };
            index[geometry.fixedAxis] = geometry.fixedIndex;
            index[geometry.uAxis] = geometry.extent[2 * geometry.uAxis] + column;
            index[geometry.vAxis] = vIndex;
            const bool valid = !snapshot->validityMask
                || snapshot->validityMask->GetScalarComponentAsDouble(
                    index[0], index[1], index[2], 0) != 0.0;
            if (!valid) {
                continue;
            }
            const double value = snapshot->image->GetScalarComponentAsDouble(
                index[0], index[1], index[2], 0);
            const double normalized = Clamp(
                (value - windowMinimum) / safeWindow,
                0.0,
                1.0);
            gray.pixels[static_cast<std::size_t>(row) * gray.widthStep + column]
                = static_cast<std::uint8_t>(normalized * 255.0 + 0.5);
        }
    }
    return true;
}

bool EdgeCaptureController::RunMeasurement()
{
    ImageGeometry geometry;
    auto& state = CurrentState();
    if (!state.initialized || !GetImageGeometry(geometry)) {
        Report("抓边失败：当前切片不可用。");
        return false;
    }
    geometry.fixedIndex = state.fixedIndex;

    ZcGrayImage gray;
    if (!BuildGraySlice(geometry, gray)) {
        Report("抓边失败：无法生成 8 位灰度切片。");
        return false;
    }

    ZcRectFrame frame;
    frame.startX = state.minU - geometry.extent[2 * geometry.uAxis];
    frame.startY = geometry.extent[2 * geometry.vAxis + 1] - state.maxV;
    frame.width = state.maxU - state.minU;
    frame.height = state.maxV - state.minV;
    frame.cosAngle = 1.0;
    frame.sinAngle = 0.0;

    ZcMeasuredLine line;
    std::string error;
    if (!m_algorithm.MeasureLineByRect(gray, frame, line, error)) {
        state.result.reset();
        Report("抓边失败：" + error);
        Refresh();
        return false;
    }

    const auto finite = [](double value) { return std::isfinite(value); };
    const double tolerance = 2.0;
    const auto pointInsideFrame = [&frame, tolerance](double x, double y) {
        return x >= frame.startX - tolerance
            && x <= frame.startX + frame.width + tolerance
            && y >= frame.startY - tolerance
            && y <= frame.startY + frame.height + tolerance;
    };
    if (!finite(line.x1) || !finite(line.y1)
        || !finite(line.x2) || !finite(line.y2)
        || !pointInsideFrame(line.x1, line.y1)
        || !pointInsideFrame(line.x2, line.y2)) {
        state.result.reset();
        Report("DLL 已返回抓边点，但点坐标不在框选区域；请查看日志确认 ZC_GetMeasuredPoints 是否返回整图像素坐标。");
        Refresh();
        return false;
    }

    state.result = line;
    Report("抓边成功，共获得 " + std::to_string(line.measuredPointsCount) + " 个测量点。");
    Refresh();
    return true;
}

void EdgeCaptureController::AddPath(
    const std::vector<Point3>& worldPath,
    double red,
    double green,
    double blue,
    double width)
{
    if (!m_renderer || worldPath.size() < 2) {
        return;
    }
    auto points = vtkSmartPointer<vtkPoints>::New();
    auto lines = vtkSmartPointer<vtkCellArray>::New();
    for (const auto& point : worldPath) {
        points->InsertNextPoint(point.data());
    }
    lines->InsertNextCell(static_cast<int>(worldPath.size()));
    for (vtkIdType id = 0; id < static_cast<vtkIdType>(worldPath.size()); ++id) {
        lines->InsertCellPoint(id);
    }
    auto polyData = vtkSmartPointer<vtkPolyData>::New();
    polyData->SetPoints(points);
    polyData->SetLines(lines);
    auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputData(polyData);
    auto actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);
    actor->PickableOff();
    actor->GetProperty()->LightingOff();
    actor->GetProperty()->SetColor(red, green, blue);
    actor->GetProperty()->SetLineWidth(width);
    m_renderer->AddActor(actor);
    m_props.push_back(actor);
}

void EdgeCaptureController::AddHandles(const std::vector<Point3>& worldPoints)
{
    if (!m_renderer || worldPoints.empty()) {
        return;
    }
    auto points = vtkSmartPointer<vtkPoints>::New();
    auto vertices = vtkSmartPointer<vtkCellArray>::New();
    for (const auto& point : worldPoints) {
        const vtkIdType id = points->InsertNextPoint(point.data());
        vertices->InsertNextCell(1, &id);
    }
    auto polyData = vtkSmartPointer<vtkPolyData>::New();
    polyData->SetPoints(points);
    polyData->SetVerts(vertices);
    auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputData(polyData);
    auto actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);
    actor->PickableOff();
    actor->GetProperty()->LightingOff();
    actor->GetProperty()->SetPointSize(10.0);
    actor->GetProperty()->RenderPointsAsSpheresOn();
    actor->GetProperty()->SetColor(1.0, 0.75, 0.05);
    m_renderer->AddActor(actor);
    m_props.push_back(actor);
}

void EdgeCaptureController::RemoveProps()
{
    if (m_renderer) {
        for (const auto& prop : m_props) {
            m_renderer->RemoveViewProp(prop);
        }
    }
    m_props.clear();
}

void EdgeCaptureController::RequestRender()
{
    if (m_service) {
        m_service->SetDirty();
    }
    if (m_renderer && m_renderer->GetRenderWindow()) {
        m_renderer->GetRenderWindow()->Render();
    }
}

void EdgeCaptureController::Report(const std::string& message) const
{
      m_statusCallback(message);
}

} 
