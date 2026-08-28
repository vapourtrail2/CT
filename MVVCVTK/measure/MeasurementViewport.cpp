#include "measure/MeasurementViewport.h"

#include "measure/EdgeCaptureController.h"
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
        m_dataManager = std::make_shared<RawVolumeDataManager>();

        const auto image = m_dataManager->GetImageSnapshot();
        ImageState localImageState = *imageSnapshot;//复制imagestate里的指针，
        ImageSnapshot publishedSnapShot;

        if (!m_dataManager->SetCurrentData(localImageState, image, publishedSnapShot)) {
            Reset();
            return false;
        }
       
        // 创建测量窗口独占的状态。
        m_broadcaster = std::make_shared<SharedStateBroadcaster>();
        m_state = std::make_shared<SharedInteractionState>(m_broadcaster);
        m_state->SetModelMatrix(*initialState.modelMatrix);
        m_state->SetWindowLevel(
                (*initialState.windowLevel)[0],
                (*initialState.windowLevel)[1]);
        
        BackgroundColor background;
        background.r = (*initialState.background)[0];
        background.g = (*initialState.background)[1];
        background.b = (*initialState.background)[2];
        m_state->SetBackground(background);
       
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
        m_service = std::make_shared<VizService>(
                m_dataManager,
                m_state,
                m_broadcaster);
        m_context = std::make_shared<StdRenderContext>();
        vtkSmartPointer<vtkRenderWindow> renderWindow =widget->renderWindow();
        m_context->SetRenderWindow(std::move(renderWindow));
        m_context->SetServiceBound(m_service);
        const VizMode vizMode = GetSliceViewDescriptor(view).vizMode;
        m_service->SetVizMode(vizMode);
        m_context->SetCameraStyle(vizMode);
        m_context->SetOrientationAxesVisible(false);

        // 使用测量窗口自己复制后的数据参数。
        const auto scalarRange =m_dataManager->GetScalarRange();
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

        m_state->SetImageDataReady(
            scalarRange[0],
            scalarRange[1],
            spacing);
        m_context->SetInteractorReady();
        m_service->SendUpdates();

        const auto& cursor = *initialState.cursorWorld;
        m_state->SetCursorRawWorld(
                cursor[0], cursor[1], cursor[2]);
        m_state->SetCursorAxis(-1);
        m_state->SetCursorWorld(
                cursor[0], cursor[1], cursor[2]);
        m_service->SendUpdates();
      
        m_edgeCapture = std::make_unique<EdgeCaptureController>(
            m_dataManager,
            m_service.get(),
            m_context->GetRenderer(),
            m_currentView);
        RebuildHandlers();
        m_overlay = std::make_shared<MeasurementOverlayStrategy>(
                m_session,
                m_service.get(),
                m_currentView);
        m_service->AttachOverlayStrategy(m_overlay);
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
        m_edgeCapture.reset();
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
            },
            {});
    }

    void MeasurementViewport::Refresh()
    {
        if (!IsReady()) {
            return;
        }
        m_overlay->Refresh();
        if (m_edgeCapture) {
            m_edgeCapture->Refresh();
        }
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

        if (m_edgeCapture) {
            m_edgeCapture->SetView(view);
        }

        if (m_overlay) {
            m_overlay->SetView(view);
            m_service->AttachOverlayStrategy(m_overlay);
            m_service->SendUpdates();
        }

        m_context->ResetCamera();
        m_context->SendRender();

        return true;
    }

    void MeasurementViewport::SetEdgeCaptureEnabled(bool enabled)
    {
        if (!m_edgeCapture) {
            return;
        }
        m_edgeCapture->SetEnabled(enabled);
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

} // namespace measure
