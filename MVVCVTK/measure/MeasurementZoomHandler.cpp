#include "measure/MeasurementZoomHandler.h"

#include "App/AppInterfaces.h"
#include <vtkCamera.h>
#include <vtkCommand.h>
#include <vtkRenderer.h>

#include <algorithm>
#include <cmath>

namespace measure {
namespace {
bool IsSliceMode(VizMode mode)
{
    return mode == VizMode::SliceTop_down
        || mode == VizMode::SliceFront_back
        || mode == VizMode::SliceLeft_right;
}
}

MeasurementZoomHandler::MeasurementZoomHandler(
    AbstractInteractiveService* service,
    vtkRenderer* renderer)
    : m_service(service)
    , m_renderer(renderer)
{
}

InteractionResult MeasurementZoomHandler::GetHandleResult(const InteractionEvent& event)
{
    if (!m_renderer || !IsSliceMode(event.vizMode)) {
        return {};
    }

    if (event.vtkEventId == vtkCommand::RightButtonPressEvent) {
        m_zooming = true;
        m_startY = event.y;
        if (auto* camera = m_renderer->GetActiveCamera()) {
            m_startScale = camera->GetParallelScale();
        }
        if (m_service) m_service->SetInteracting(true);
        return { true, true };
    }

    if (event.vtkEventId == vtkCommand::RightButtonReleaseEvent) {
        m_zooming = false;
        if (m_service) m_service->SetInteracting(false);
        return { true, true };
    }

    if (event.vtkEventId == vtkCommand::MouseMoveEvent) {
        if (m_zooming) {
            if (auto* camera = m_renderer->GetActiveCamera()) {
                const int totalDy = event.y - m_startY;
                const double factor = std::pow(1.01, totalDy);
                camera->SetParallelScale(std::max(1e-6, m_startScale * factor));
                m_renderer->ResetCameraClippingRange();
                if (m_service) m_service->SetDirtyMarked();
            }
        }
        return { true, true };
    }

    if (event.vtkEventId == vtkCommand::LeftButtonPressEvent
        || event.vtkEventId == vtkCommand::LeftButtonReleaseEvent
        || event.vtkEventId == vtkCommand::MouseWheelForwardEvent
        || event.vtkEventId == vtkCommand::MouseWheelBackwardEvent) {
        return { true, true };
    }

    return {};
}

} // namespace measure
