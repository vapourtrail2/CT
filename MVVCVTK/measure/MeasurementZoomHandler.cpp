#include "measure/MeasurementZoomHandler.h"
#include "measure/MeasureViewAdapter.h"

#include <vtkCamera.h>
#include <vtkRenderer.h>

#include <algorithm>
#include <cmath>

namespace measure {

    MeasurementZoomHandler::MeasurementZoomHandler(
        MeasureViewAdapter* adapter,
        vtkRenderer* renderer)
        : m_adapter(adapter)
        , m_renderer(renderer)
    {
    }

    InteractionResult MeasurementZoomHandler::Send(
        const InteractionEvent& event)
    {
        if (!m_adapter || !m_renderer) {
            return {};
        }

        if (event.eventKind
            == InteractionEventKind::SecondaryPress) {
            m_zooming = true;
            m_lastY = event.y;
            return { true, true };
        }

        if (event.eventKind
            == InteractionEventKind::SecondaryRelease) {
            m_zooming = false;
            return { true, true };
        }

        if (event.eventKind
            == InteractionEventKind::PointerMove
            && m_zooming) {
            const int deltaY = event.y - m_lastY;
            m_lastY = event.y;
            const double factor = std::clamp(
                std::pow(1.01, static_cast<double>(deltaY)),
                0.2,
                5.0);
            m_renderer->GetActiveCamera()->Zoom(factor);
            m_renderer->ResetCameraClippingRange();
            m_adapter->SendRender();
            return { true, true };
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
