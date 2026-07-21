#pragma once
#include "App/AppState.h"
#include "App/AppInterfaces.h"
#include "Service/AppService.h"
#include "c_ui/qt/QtRenderContext.h"
#include <QPointer>
#include <memory>
#include <utility>

class QVTKOpenGLNativeWidget;

// 一个渲染视口：自带 VTK 控件 + 渲染上下文 + 渲染服务 + 显示模式。
class Viewport
{
public:
    void setAttach(QVTKOpenGLNativeWidget* widget) { widget_ = widget; }
    void setMode(VizMode mode)
    {
        mode_ = mode;
        if (svc_) svc_->SetVizMode(mode_);
        if (ctx_) ctx_->SetCameraStyleByVizMode(mode_);
    }
    void setOrientationAxesVisible(bool visible) { orientationAxesVisible_ = visible; }
    void setInteractionHandlerFactory(
        QtRenderContext::InteractionHandlerFactory factory)
    {
        interactionHandlerFactory_ = std::move(factory);
        if (ctx_) ctx_->SetInteractionHandlerFactory(interactionHandlerFactory_);
    }

    // 用数据/状态装配这个视口的渲染管线
    void setBuild(const std::shared_ptr<AbstractDataManager>& dataMgr,
        const std::shared_ptr<SharedInteractionState>& state,
        const std::shared_ptr<SharedStateBroadcaster>& broadcaster)
    {
        svc_ = std::make_shared<MedicalVizService>(dataMgr, state, broadcaster);
        ctx_ = std::make_shared<QtRenderContext>();
        ctx_->SetInteractionHandlerFactory(interactionHandlerFactory_);
        ctx_->SetQtWidget(widget_);
        ctx_->SetServiceBound(svc_);
        setMode(mode_);
        ctx_->ToggleOrientationAxes(orientationAxesVisible_);
    }

    void reset() { ctx_.reset(); svc_.reset(); }
    void start() { if (ctx_) ctx_->SetStarted(); }
    void processPendingUpdates() { if (svc_) svc_->SetPendingUpdatesProcessed(); }
    void render() { if (ctx_) ctx_->SetRendered(); }
    void refresh() { processPendingUpdates(); render(); }

    const std::shared_ptr<MedicalVizService>& getService() const { return svc_; }
    const std::shared_ptr<QtRenderContext>& getContext() const { return ctx_; }

private:
    QPointer<QVTKOpenGLNativeWidget> widget_;
    std::shared_ptr<QtRenderContext>   ctx_;
    std::shared_ptr<MedicalVizService> svc_;
    QtRenderContext::InteractionHandlerFactory interactionHandlerFactory_;
    VizMode mode_ = VizMode::Volume;
    bool orientationAxesVisible_ = true;
};
