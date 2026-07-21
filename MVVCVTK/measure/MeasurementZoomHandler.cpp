#include "measure/MeasurementZoomHandler.h"
#include "measure/MeasurementView.h"

#include "Interaction/Viewer2DHandler.h"
#include <vtkCamera.h>
#include <vtkCommand.h>
#include <vtkRenderer.h>

#include <memory>

namespace measure {

MeasurementZoomHandler::MeasurementZoomHandler(
    AbstractInteractiveService* service,
    vtkRenderer* renderer)
    : m_viewer2D(std::make_unique<Viewer2DHandler>(service, nullptr, renderer))
{
}

MeasurementZoomHandler::~MeasurementZoomHandler() = default;

InteractionResult MeasurementZoomHandler::GetHandleResult(const InteractionEvent& event)
{
    if (!m_viewer2D || !IsSliceVizMode(event.vizMode)) {
        return {};
    }

    if (event.vtkEventId == vtkCommand::RightButtonPressEvent) {
        m_zooming = true;
        return m_viewer2D->GetHandleResult(event);
    }

    if (event.vtkEventId == vtkCommand::RightButtonReleaseEvent) {
        const auto result = m_viewer2D->GetHandleResult(event);
        m_zooming = false;
        return result.handled ? result : InteractionResult{ true, true };
    }

    if (event.vtkEventId == vtkCommand::MouseMoveEvent && m_zooming) {
        return m_viewer2D->GetHandleResult(event);
    }

    if (event.vtkEventId == vtkCommand::MouseWheelForwardEvent
        || event.vtkEventId == vtkCommand::MouseWheelBackwardEvent) {
        return { true, true };
    }

    return {};
}

} // namespace measure
