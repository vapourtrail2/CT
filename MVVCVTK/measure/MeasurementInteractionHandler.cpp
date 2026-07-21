#include "measure/MeasurementInteractionHandler.h"
#include "measure/MeasurementGeometry.h"
#include "measure/MeasurementSession.h"

#include "App/AppInterfaces.h"
#include <vtkCommand.h>
#include <vtkImageData.h>
#include <vtkRenderer.h>

#include <algorithm>
#include <cmath>
#include <string>

namespace measure {

MeasurementInteractionHandler::MeasurementInteractionHandler(
    const std::shared_ptr<MeasurementSession>& session,
    const std::shared_ptr<AbstractDataManager>& dataManager,
    AbstractInteractiveService* service,
    vtkRenderer* renderer,
    MeasureView view)
    : m_session(session)
    , m_dataManager(dataManager)
    , m_service(service)
    , m_renderer(renderer)
    , m_view(view)
{
}

InteractionResult MeasurementInteractionHandler::GetHandleResult(const InteractionEvent& event)
{
    if (!m_session || !m_session->IsActive() || !m_service || !m_renderer) {
        if (event.vtkEventId == vtkCommand::LeftButtonReleaseEvent && m_consumingLeftButton) {
            m_consumingLeftButton = false;
            return { true, true };
        }
        return {};
    }

    const auto& draft = m_session->Draft();
    if (!draft || draft->request.view != m_view) {
        return {};
    }

    if (event.vtkEventId == vtkCommand::KeyPressEvent) {
        if (event.keySym == "Escape") {
            m_session->CancelDraft();
            return { true, true };
        }
        if (event.keySym == "BackSpace" || event.keySym == "Backspace") {
            m_session->UndoDraftPoint();
            return { true, true };
        }
    }

    if (event.vtkEventId == vtkCommand::LeftButtonPressEvent) {
        m_consumingLeftButton = true;
        m_pressX = event.x;
        m_pressY = event.y;
        m_lineDragMoved = false;
        m_lineDragCandidate = draft->request.tool == MeasureTool::Line
            && draft->physicalPoints.empty();
        const auto physical = DisplayToPhysical(event.x, event.y);
        if (physical) {
            std::string error;
            m_session->AppendPoint(*physical, SnapshotPhysicalPlane(), &error);
        }
        else {
            m_session->SetStatusMessage("点击位置不在图像有效范围内，请在图像上选择点。");
            m_lineDragCandidate = false;
        }
        return { true, true };
    }

    if (event.vtkEventId == vtkCommand::LeftButtonReleaseEvent && m_consumingLeftButton) {
        if (m_lineDragCandidate && m_lineDragMoved && m_session->IsActive()) {
            const auto& currentDraft = m_session->Draft();
            if (currentDraft
                && currentDraft->request.tool == MeasureTool::Line
                && currentDraft->physicalPoints.size() == 1) {
                const auto physical = DisplayToPhysical(event.x, event.y);
                if (physical) {
                    std::string error;
                    m_session->AppendPoint(*physical, SnapshotPhysicalPlane(), &error);
                }
                else {
                    m_session->SetStatusMessage("终点不在图像有效范围内，请重新选择。");
                }
            }
        }
        m_consumingLeftButton = false;
        m_lineDragCandidate = false;
        m_lineDragMoved = false;
        return { true, true };
    }

    if (event.vtkEventId == vtkCommand::MouseMoveEvent) {
        if (!draft->physicalPoints.empty()) {
            const auto preview = DisplayToPhysical(event.x, event.y);
            m_session->UpdatePreview(preview);
            if (m_lineDragCandidate) {
                const int dx = event.x - m_pressX;
                const int dy = event.y - m_pressY;
                m_lineDragMoved = dx * dx + dy * dy >= 9;
            }
            return { true, true };
        }
        return {};
    }

    if ((event.vtkEventId == vtkCommand::MouseWheelForwardEvent
            || event.vtkEventId == vtkCommand::MouseWheelBackwardEvent)
        && !draft->physicalPoints.empty()) {
        return { true, true };
    }

    return {};
}

std::optional<Point3> MeasurementInteractionHandler::DisplayToPhysical(int x, int y) const
{
    if (!m_renderer || !m_service) {
        return std::nullopt;
    }

    auto displayToWorld = [this, x, y](double z) -> std::optional<Point3> {
        m_renderer->SetDisplayPoint(static_cast<double>(x), static_cast<double>(y), z);
        m_renderer->DisplayToWorld();
        double* homogeneous = m_renderer->GetWorldPoint();
        if (!homogeneous || std::abs(homogeneous[3]) <= 1e-12) {
            return std::nullopt;
        }
        return Point3{
            homogeneous[0] / homogeneous[3],
            homogeneous[1] / homogeneous[3],
            homogeneous[2] / homogeneous[3]
        };
    };

    const auto nearPoint = displayToWorld(0.0);
    const auto farPoint = displayToWorld(1.0);
    if (!nearPoint || !farPoint) {
        return std::nullopt;
    }

    const Point3 direction = geometry::Subtract(*farPoint, *nearPoint);
    const Point3 normal = CurrentWorldNormal();
    const auto cursor = m_service->GetCursorWorld();
    const Point3 planeOrigin{ cursor[0], cursor[1], cursor[2] };
    const double denominator = geometry::Dot(normal, direction);
    if (std::abs(denominator) <= 1e-12) {
        return std::nullopt;
    }

    const double t = geometry::Dot(normal, geometry::Subtract(planeOrigin, *nearPoint)) / denominator;
    const Point3 worldPoint = geometry::Add(*nearPoint, geometry::Scale(direction, t));
    double world[3] = { worldPoint[0], worldPoint[1], worldPoint[2] };
    double physical[3] = { 0.0, 0.0, 0.0 };
    m_service->GetModelPositionFromWorld(world, physical);
    const Point3 result{ physical[0], physical[1], physical[2] };
    return IsInsideImage(result) ? std::optional<Point3>(result) : std::nullopt;
}

MeasurementPlane MeasurementInteractionHandler::SnapshotPhysicalPlane() const
{
    const auto cursor = m_service->GetCursorWorld();
    Point3 worldOrigin{ cursor[0], cursor[1], cursor[2] };
    Point3 worldU{ 1.0, 0.0, 0.0 };
    Point3 worldV{ 0.0, 1.0, 0.0 };
    if (m_view == MeasureView::Coronal) {
        worldV = { 0.0, 0.0, 1.0 };
    }
    else if (m_view == MeasureView::Sagittal) {
        worldU = { 0.0, 1.0, 0.0 };
        worldV = { 0.0, 0.0, 1.0 };
    }

    auto toPhysical = [this](const Point3& world) {
        double source[3] = { world[0], world[1], world[2] };
        double target[3] = { 0.0, 0.0, 0.0 };
        m_service->GetModelPositionFromWorld(source, target);
        return Point3{ target[0], target[1], target[2] };
    };

    MeasurementPlane plane;
    plane.origin = toPhysical(worldOrigin);
    const Point3 physicalUPoint = toPhysical(geometry::Add(worldOrigin, worldU));
    const Point3 physicalVPoint = toPhysical(geometry::Add(worldOrigin, worldV));
    const auto normalizedU = geometry::Normalized(geometry::Subtract(physicalUPoint, plane.origin));
    Point3 rawV = geometry::Subtract(physicalVPoint, plane.origin);
    if (normalizedU) {
        rawV = geometry::Subtract(rawV, geometry::Scale(*normalizedU, geometry::Dot(rawV, *normalizedU)));
    }
    const auto normalizedV = geometry::Normalized(rawV);
    if (normalizedU) plane.u = *normalizedU;
    if (normalizedV) plane.v = *normalizedV;
    if (const auto normal = geometry::Normalized(geometry::Cross(plane.u, plane.v))) {
        plane.normal = *normal;
        if (const auto correctedV = geometry::Normalized(geometry::Cross(plane.normal, plane.u))) {
            plane.v = *correctedV;
        }
    }

    if (m_dataManager) {
        const auto spacing = m_dataManager->GetSpacing();
        const double minimumSpacing = std::min({
            std::abs(spacing[0]), std::abs(spacing[1]), std::abs(spacing[2]) });
        plane.tolerance = std::max(1e-9, minimumSpacing * 1e-6);
        plane.sliceTolerance = std::max(1e-6, minimumSpacing * 0.51);
    }
    return plane;
}

bool MeasurementInteractionHandler::IsInsideImage(const Point3& physicalPoint) const
{
    if (!m_dataManager) {
        return false;
    }
    auto image = m_dataManager->GetVtkImage();
    if (!image) {
        return false;
    }

    double source[3] = { physicalPoint[0], physicalPoint[1], physicalPoint[2] };
    double continuousIndex[3] = { 0.0, 0.0, 0.0 };
    image->TransformPhysicalPointToContinuousIndex(source, continuousIndex);
    int extent[6];
    image->GetExtent(extent);
    constexpr double margin = 0.5;
    for (int axis = 0; axis < 3; ++axis) {
        if (continuousIndex[axis] < extent[2 * axis] - margin
            || continuousIndex[axis] > extent[2 * axis + 1] + margin) {
            return false;
        }
    }
    return true;
}

Point3 MeasurementInteractionHandler::CurrentWorldNormal() const
{
    switch (m_view) {
    case MeasureView::Axial: return { 0.0, 0.0, 1.0 };
    case MeasureView::Coronal: return { 0.0, 1.0, 0.0 };
    case MeasureView::Sagittal: return { 1.0, 0.0, 0.0 };
    }
    return { 0.0, 0.0, 1.0 };
}

} // namespace measure
