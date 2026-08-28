#pragma once

#include "App/AppInterfaces.h"
#include "measure/MeasurementTypes.h"
#include "measure/MeasurementViewInitState.h"
#include <QPointer>
#include <functional>
#include <memory>
#include <string>

class QVTKOpenGLNativeWidget;
class RawVolumeDataManager;
class SharedStateBroadcaster;
class SharedInteractionState;
class VizService;
class StdRenderContext;

namespace measure {

    class MeasurementInteractionHandler;
    class MeasurementOverlayStrategy;
    class MeasurementSession;
    class MeasurementZoomHandler;
    class EdgeCaptureController;

    class MeasurementViewport final {
    public:
        MeasurementViewport();
        ~MeasurementViewport();

        MeasurementViewport(
            const MeasurementViewport&) = delete;

        MeasurementViewport& operator=(
            const MeasurementViewport&) = delete;

        bool Build(
            QVTKOpenGLNativeWidget* widget,
            const ImageSnapshot& imageSnapshot,
            const std::shared_ptr<MeasurementSession>& session,
            MeasureView view,
            const MeasurementViewInitState& initialState);

        void Reset();

        bool SetView(MeasureView view);

        void Refresh();

        bool IsReady() const;

        void SetEdgeCaptureEnabled(bool enabled);
        bool IsEdgeCaptureEnabled() const;
        void SetEdgeStatusCallback(
            std::function<void(const std::string&)> callback);

    private:
        void RebuildHandlers();

        QPointer<QVTKOpenGLNativeWidget> m_widget;

        std::shared_ptr<RawVolumeDataManager>
            m_dataManager;

        std::shared_ptr<SharedStateBroadcaster>
            m_broadcaster;

        std::shared_ptr<SharedInteractionState>
            m_state;

        std::shared_ptr<VizService>
            m_service;

        std::shared_ptr<StdRenderContext>
            m_context;

        std::shared_ptr<MeasurementSession>
            m_session;

        std::shared_ptr<MeasurementOverlayStrategy>
            m_overlay;

        std::unique_ptr<MeasurementZoomHandler>
            m_zoomHandler;

        std::unique_ptr<MeasurementInteractionHandler>
            m_measurementHandler;

        std::unique_ptr<EdgeCaptureController>
            m_edgeCapture;

        MeasureView m_currentView = MeasureView::Axial;
    };

}

/*
m_dataManager
    Dialog 独占的体数据副本

m_broadcaster + m_state
    Dialog 独占的切片位置、窗宽窗位等状态

m_service
    构建并更新二维切片管线

m_context
    把管线绑定到 Dialog 的 QVTK 控件，并接收鼠标事件

m_session
    保存线、圆、圆弧、草稿、Undo、Redo

m_overlay
    把测量结果画出来

m_zoomHandler
    处理右键缩放

m_measurementHandler
    处理左键取点和鼠标移动预览
*/
