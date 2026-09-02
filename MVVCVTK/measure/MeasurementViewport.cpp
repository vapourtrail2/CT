#include "measure/MeasurementViewport.h"

#include "measure/EdgeCaptureController.h"
#include "measure/MeasurementInteractionHandler.h"
#include "measure/MeasurementOverlayStrategy.h"
#include "measure/MeasurementSession.h"
#include "measure/MeasurementZoomHandler.h"
#include "measure/MeasureViewAdapter.h"

#include <QVTKOpenGLNativeWidget.h>

#include <utility>

namespace measure {

MeasurementViewport::MeasurementViewport() = default;

MeasurementViewport::~MeasurementViewport()
{
    Reset();
}

bool MeasurementViewport::Build(
    QVTKOpenGLNativeWidget* widget,
    TrustedImageSnapshot image,
    const std::shared_ptr<MeasurementSession>& session,
    MeasureView view,
    const MeasurementViewInitState& initialState)
{
    Reset();
    if (!widget || !image || !image->image || !session) {
        return false;
    }

    m_widget = widget;
    m_session = session;
    m_currentView = view;
    m_adapter = std::make_unique<MeasureViewAdapter>();
    if (!m_adapter->Build(
            widget,
            std::move(image),
            view,
            initialState)) {
        Reset();
        return false;
    }

    m_overlay = std::make_shared<MeasurementOverlayStrategy>(
        m_session,
        m_adapter.get(),
        m_currentView);
    m_overlay->AttachRenderer(m_adapter->GetRenderer());

    m_edgeCapture = std::make_unique<EdgeCaptureController>(
        m_adapter.get(),
        m_adapter->GetRenderer(),
        m_currentView);
    BuildHandlers();

    m_adapter->SetInputCallback(
        [this](const InteractionEvent& event) {
            if (m_zoomHandler) {
                const auto zoomResult = m_zoomHandler->Send(event);
                if (zoomResult.isHandled) {
                    return zoomResult;
                }
            }

            if (m_edgeCapture && m_edgeCapture->IsEnabled()) {
                const auto edgeResult = m_edgeCapture->Send(event);
                if (edgeResult.isHandled) {
                    return edgeResult;
                }
            }

            if (m_measurementHandler) {
                return m_measurementHandler->Send(event);
            }
            return InteractionResult{};
        });

    Refresh();
    return true;
}

void MeasurementViewport::Reset()
{
    if (m_adapter) {
        m_adapter->SetInputCallback({});
    }

    m_measurementHandler.reset();
    m_zoomHandler.reset();
    m_edgeCapture.reset();

    if (m_overlay) {
        m_overlay->DetachRenderer();
    }
    m_overlay.reset();

    if (m_adapter) {
        m_adapter->Reset();
    }
    m_adapter.reset();
    m_session.reset();
    m_widget.clear();
    m_currentView = MeasureView::Axial;
}

bool MeasurementViewport::SetView(MeasureView view)
{
    if (!IsReady()) {
        return false;
    }
    if (view == m_currentView) {
        Refresh();
        return true;
    }
    if (!m_adapter->SetView(view)) {
        return false;
    }

    m_currentView = view;
    BuildHandlers();
    if (m_edgeCapture) {
        m_edgeCapture->SetView(view);
    }
    if (m_overlay) {
        m_overlay->SetView(view);
    }
    Refresh();
    return true;
}

void MeasurementViewport::Refresh()
{
    if (!IsReady()) {
        return;
    }
    if (m_overlay) {
        m_overlay->Refresh();
    }
    if (m_edgeCapture) {
        m_edgeCapture->Refresh();
    }
    m_adapter->SendRender();
}

bool MeasurementViewport::IsReady() const
{
    return m_widget
        && m_adapter
        && m_adapter->GetIsReady()
        && m_session;
}

void MeasurementViewport::SetEdgeCaptureEnabled(bool isEnabled)
{
    if (!m_edgeCapture) {
        return;
    }
    m_edgeCapture->SetEnabled(isEnabled);
    if (m_widget) {
        m_widget->setFocus(Qt::MouseFocusReason);
    }
}

bool MeasurementViewport::IsEdgeCaptureEnabled() const
{
    return m_edgeCapture && m_edgeCapture->IsEnabled();
}

void MeasurementViewport::SetEdgeStatusCallback(
    std::function<void(const std::string&)> callback)
{
    if (m_edgeCapture) {
        m_edgeCapture->SetStatusCallback(std::move(callback));
    }
}

void MeasurementViewport::BuildHandlers()
{
    m_measurementHandler.reset();
    m_zoomHandler.reset();
    if (!m_adapter || !m_session) {
        return;
    }

    m_zoomHandler = std::make_unique<MeasurementZoomHandler>(
        m_adapter.get(),
        m_adapter->GetRenderer());
    m_measurementHandler =
        std::make_unique<MeasurementInteractionHandler>(
            m_session,
            m_adapter.get(),
            m_adapter->GetRenderer(),
            m_currentView);
}

} // namespace measure
