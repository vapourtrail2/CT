#pragma once

#include "MVVCVTK/SPI/Data/TrustedImageState.h"
#include "measure/MeasurementTypes.h"
#include "measure/MeasurementViewInitState.h"

#include <QPointer>

#include <functional>
#include <memory>
#include <string>

class QVTKOpenGLNativeWidget;

namespace measure {

class EdgeCaptureController;
class MeasurementInteractionHandler;
class MeasurementOverlayStrategy;
class MeasurementSession;
class MeasurementZoomHandler;
class MeasureViewAdapter;

class MeasurementViewport final {
public:
    MeasurementViewport();
    ~MeasurementViewport();

    MeasurementViewport(const MeasurementViewport&) = delete;
    MeasurementViewport& operator=(const MeasurementViewport&) = delete;

    bool Build(
        QVTKOpenGLNativeWidget* widget,
        TrustedImageSnapshot image,
        const std::shared_ptr<MeasurementSession>& session,
        MeasureView view,
        const MeasurementViewInitState& initialState);
    void Reset();

    bool SetView(MeasureView view);
    void Refresh();
    bool IsReady() const;

    void SetEdgeCaptureEnabled(bool isEnabled);
    bool IsEdgeCaptureEnabled() const;
    void SetEdgeStatusCallback(
        std::function<void(const std::string&)> callback);

private:
    void BuildHandlers();

    QPointer<QVTKOpenGLNativeWidget> m_widget;
    std::unique_ptr<MeasureViewAdapter> m_adapter;
    std::shared_ptr<MeasurementSession> m_session;
    std::shared_ptr<MeasurementOverlayStrategy> m_overlay;
    std::unique_ptr<MeasurementZoomHandler> m_zoomHandler;
    std::unique_ptr<MeasurementInteractionHandler> m_measurementHandler;
    std::unique_ptr<EdgeCaptureController> m_edgeCapture;
    MeasureView m_currentView = MeasureView::Axial;
};

} // namespace measure
