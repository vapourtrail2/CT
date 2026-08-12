#include "measure/MeasurementZoomHandler.h"
#include "measure/MeasurementView.h"

#include "Interaction/Viewer2DHandler.h"
#include <vtkCamera.h>
#include <vtkCommand.h>
#include <vtkRenderer.h>

#include <memory>

namespace measure {

    MeasurementZoomHandler::MeasurementZoomHandler(
        InteractiveService* service,
        vtkRenderer* renderer)
        : m_viewer2D(std::make_unique<Viewer2DHandler>(service, nullptr, renderer))
    {
    }

    MeasurementZoomHandler::~MeasurementZoomHandler() = default;

    InteractionResult MeasurementZoomHandler::Send(
        const InteractionEvent& event)
    {
        if (!m_viewer2D
            || !IsSliceVizMode(event.vizMode)) {
            return {};
        }

        if (event.eventKind
            == InteractionEventKind::SecondaryPress) {
            m_zooming = true;
            return m_viewer2D->Send(event);
        }

        if (event.eventKind
            == InteractionEventKind::SecondaryRelease) {
            const auto result =
                m_viewer2D->Send(event);

            m_zooming = false;

            return result.isHandled
                ? result
                : InteractionResult{ true, true };
        }

        if (event.eventKind
            == InteractionEventKind::PointerMove
            && m_zooming) {
            return m_viewer2D->Send(event);
        }

        if (event.eventKind
            == InteractionEventKind::WheelForward
            || event.eventKind
            == InteractionEventKind::WheelBackward) {
            return { true, true };
        }

        return {};
    }
}
