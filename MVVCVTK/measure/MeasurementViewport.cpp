#include "measure/MeasurementViewport.h"

#include "measure/MeasurementInteractionHandler.h"
#include "measure/MeasurementOverlayStrategy.h"
#include "measure/MeasurementSession.h"
#include "measure/MeasurementView.h"
#include "measure/MeasurementZoomHandler.h"

#include "App/AppState.h"
#include "App/AppStateEvents.h"
#include "App/Services/AppService.h"
#include "Data/DataManager.h"
#include "Render/StdRenderContext.h"

#include <QVTKOpenGLNativeWidget.h>

#include <vtkRenderWindow.h>
#include <vtkSmartPointer.h>

#include <algorithm>
#include <utility>

// 实现独立数据复制、独立渲染管线和清理

namespace measure {

    MeasurementViewport::MeasurementViewport() = default;

    MeasurementViewport::~MeasurementViewport()
    {
        Reset();
    }

    bool MeasurementViewport::Build(
        QVTKOpenGLNativeWidget* widget,
        const ImageSnapshot& imageSnapshot,
        const std::shared_ptr<MeasurementSession>& session,
        MeasureView view,
        const MeasurementViewInitState& initialState)
    {
        Reset();

        if (!widget
            || !widget->renderWindow()
            || !imageSnapshot
            || !imageSnapshot->image
            || !session) {
            return false;
        }

        m_widget = widget;
        m_session = session;
        m_currentView = view;

        // 创建测量窗口独占的数据管理器。
        m_dataManager = std::make_shared<RawVolumeDataManager>();

        // SetImageSnapshot 内部会深拷贝，
        // 因此后续不依赖主窗口里的图像对象。
        if (!m_dataManager->SetImageSnapshot(
            imageSnapshot->image)) {
            Reset();
            return false;
        }

        // SetImageSnapshot 先把数据放进 pending，
        // 这里提交为测量窗口自己的 current。
        bool hasPending = false;

        if (!m_dataManager->SetCurrentFromPending(
            hasPending)
            || !hasPending) {
            Reset();
            return false;
        }

        // 创建测量窗口独占的状态。
        m_broadcaster = std::make_shared<SharedStateBroadcaster>();

        m_state = std::make_shared<SharedInteractionState>(
                m_broadcaster);

        if (initialState.modelMatrix) {
            m_state->SetModelMatrix(
                *initialState.modelMatrix);
        }

        if (initialState.windowLevel) {
            m_state->SetWindowLevel(
                (*initialState.windowLevel)[0],
                (*initialState.windowLevel)[1]);
        }

        if (initialState.background) {
            BackgroundColor background;
            background.r = (*initialState.background)[0];
            background.g = (*initialState.background)[1];
            background.b = (*initialState.background)[2];
            m_state->SetBackground(background);
        }

        if (initialState.visibilityMask) {
            const auto visibility =
                *initialState.visibilityMask;
            m_state->SetElementVisible(
                VisFlags::Planes3D,
                (visibility & VisFlags::Planes3D) != 0);
            m_state->SetElementVisible(
                VisFlags::Crosshair,
                (visibility & VisFlags::Crosshair) != 0);
            m_state->SetElementVisible(
                VisFlags::Ruler,
                (visibility & VisFlags::Ruler) != 0);
        }

        // 创建测量窗口独占的渲染服务。
        m_service =
            std::make_shared<VizService>(
                m_dataManager,
                m_state,
                m_broadcaster);

        m_context = std::make_shared<StdRenderContext>();

        vtkSmartPointer<vtkRenderWindow> renderWindow =
            widget->renderWindow();

        m_context->SetRenderWindow(std::move(renderWindow));

        m_context->SetServiceBound(m_service);

        const VizMode vizMode = GetSliceViewDescriptor(view).vizMode;

        m_service->SetVizMode(vizMode);
        m_context->SetCameraStyle(vizMode);
        m_context->SetOrientationAxesVisible(false);

        // 使用测量窗口自己复制后的数据参数。
        const auto scalarRange =
            m_dataManager->GetScalarRange();

        const auto spacing = m_dataManager->GetSpacing();

        const double windowWidth =
            std::max(
                0.01,
                scalarRange[1] - scalarRange[0]);

        const double windowCenter =
            (scalarRange[0] + scalarRange[1]) * 0.5;

        if (!initialState.windowLevel) {
            m_state->SetWindowLevel(
                windowWidth,
                windowCenter);
        }

        // 通知这个局部 service：数据已经可用，
        // 下一次 SendUpdates 构建二维切片管线。
        m_state->SetImageDataReady(
            scalarRange[0],
            scalarRange[1],
            spacing);
        m_context->SetInteractorReady();

        /*
        * 先显示二维图像
        → 再安装鼠标事件
            → 再挂测量 Overlay
                → 最后渲染
        */

        // 第一次更新：先构建二维切片管线。
        m_service->SendUpdates();

        // BuildPipeline 会先把局部 cursor 放到数据中心，
        // 管线建立后再恢复打开测量窗口时复制的主会话位置。
        if (initialState.cursorWorld) {
            const auto& cursor = *initialState.cursorWorld;
            m_state->SetCursorRawWorld(
                cursor[0], cursor[1], cursor[2]);
            m_state->SetCursorAxis(-1);
            m_state->SetCursorWorld(
                cursor[0], cursor[1], cursor[2]);
            m_service->SendUpdates();
        }
        // 给这个独立视口安装缩放和测量事件。
        RebuildHandlers();
        // 创建并挂载测量图层。
        m_overlay = std::make_shared<MeasurementOverlayStrategy>(
                m_session,
                m_service.get(),
                m_currentView);
        m_service->AttachOverlayStrategy(m_overlay);

        // 第二次更新：把 Overlay 同步到 renderer。
        m_service->SendUpdates();
        m_context->ResetCamera();
        m_context->SendRender();

        return true;
    }

    void MeasurementViewport::Reset()
    {
        if (m_context) {
            m_context->ClearInputHandler();
        }

        if (m_service && m_overlay) {
            m_service->RemoveOverlayStrategy(
                m_overlay);
        }

        m_measurementHandler.reset();
        m_zoomHandler.reset();
        m_overlay.reset();

        m_context.reset();
        m_service.reset();
        m_state.reset();
        m_broadcaster.reset();
        m_dataManager.reset();
        m_session.reset();

        m_widget.clear();
        m_currentView = MeasureView::Axial;
    }

    bool MeasurementViewport::IsReady() const
    {
        return m_widget
            && m_dataManager
            && m_state
            && m_service
            && m_context
            && m_session;
    }

    void MeasurementViewport::RebuildHandlers()
    {
        if (!m_context
            || !m_service
            || !m_dataManager
            || !m_session) 
        {
            return;
        }

        // 先让 Context 放弃旧回调，
        // 再销毁旧 Handler。
        m_context->ClearInputHandler();
        m_measurementHandler.reset();
        m_zoomHandler.reset();

        vtkRenderer* renderer = m_context->GetRenderer();

        m_zoomHandler = std::make_unique<MeasurementZoomHandler>(
                m_service.get(),
                renderer);

        m_measurementHandler =
            std::make_unique<
            MeasurementInteractionHandler>(
                m_session,
                m_dataManager,
                m_service.get(),
                renderer,
                m_currentView);

        m_context->SetInputHandler(
            [this](const InteractionEvent& event) {
                if (m_zoomHandler) {
                    const auto zoomResult =
                        m_zoomHandler->Send(event);

                    if (zoomResult.isHandled) {
                        return zoomResult;
                    }
                }

                if (m_measurementHandler) {
                    return m_measurementHandler->Send(
                        event);
                }

                return InteractionResult{};
            },
            {});
    }

    void MeasurementViewport::Refresh()
    {
        if (!IsReady()) {
            return;
        }
        m_overlay->Refresh();
        m_service->SetDirty();
    }

    bool MeasurementViewport::SetView(
        MeasureView view)
    {
        if (!IsReady()) {
            return false;
        }

        if (view == m_currentView) {
            Refresh();
            return true;
        }

        // 切换管线前，先把 Overlay 从旧 renderer 卸下。
        if (m_overlay) {
            m_service->RemoveOverlayStrategy(m_overlay);
        }

        m_currentView = view;
        const VizMode vizMode = GetSliceViewDescriptor(view).vizMode;
        m_service->SetVizMode(vizMode);
        m_context->SetCameraStyle(vizMode);

        // MeasurementInteractionHandler 内部保存了 view，
        // 所以切换方向后必须重建。
        RebuildHandlers();

        // 先让 Core 完成新方向切片管线的构建。
        m_service->SendUpdates();

        if (m_overlay) {
            m_overlay->SetView(view);
            m_service->AttachOverlayStrategy(m_overlay);
            m_service->SendUpdates();
        }

        m_context->ResetCamera();
        m_context->SendRender();

        return true;
    }

} // namespace measure
