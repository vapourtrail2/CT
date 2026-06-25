#include "c_ui/contextarea/WorkspacePage.h"
#include "c_ui/workbenches/ReconstructPage.h"
#include "c_ui/panels/SceneTreePanel.h"
#include "c_ui/panels/RenderPanel.h"
#include "c_ui/contextarea/WorkSpaceUIState.h"
#include "AppController.h"

#include <QSplitter>
#include <QVBoxLayout>
#include <QSizePolicy>

WorkspacePage::WorkspacePage(QWidget* parent)
	: QWidget(parent)
{
	buildUi();
}

void WorkspacePage::buildUi() {
    auto root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    workspaceSplit_ = new QSplitter(Qt::Horizontal, this);//第一个参数是水平分割
    workspaceSplit_->setObjectName("workspaceSplit");
    root->addWidget(workspaceSplit_, 1);

    //左侧
    viewportPage_ = new ReconstructPage(workspaceSplit_);

    //右侧
    rightSplit_ = new QSplitter(Qt::Vertical, workspaceSplit_);
    rightSplit_->setObjectName("rightsplit");

    renderPanel_ = new RenderPanel(rightSplit_);  
    renderPanel_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    renderPanel_->setMaximumHeight(520);
    rightSplit_->addWidget(renderPanel_);

    sceneTreePanel_ = new SceneTreePanel(rightSplit_);
    rightSplit_->addWidget(sceneTreePanel_);

    rightSplit_->setStretchFactor(0, 0);
    rightSplit_->setStretchFactor(1, 1);
    rightSplit_->setSizes({ 520, 260 });

    workSpaceUIState_ = new WorkSpaceUIState(this);
    renderPanel_->setWorkSpaceUIState(workSpaceUIState_);
    connect(workSpaceUIState_, &WorkSpaceUIState::primary3DModeChanged, viewportPage_, &ReconstructPage::setPrimary3DMode);
    
    viewportPage_->setPrimary3DMode(workSpaceUIState_->getPrimary3DMode());
    //安装
    workspaceSplit_->addWidget(viewportPage_);
    workspaceSplit_->addWidget(rightSplit_);
    workspaceSplit_->setStretchFactor(0, 50);
    workspaceSplit_->setStretchFactor(1, 3);
}

ReconstructPage* WorkspacePage::getViewportPage() const
{
    return viewportPage_;
}

SceneTreePanel* WorkspacePage::getSceneTreePanel() const
{
    return sceneTreePanel_;
}

RenderPanel* WorkspacePage::getRenderPanel() const
{
    return renderPanel_;
}

bool WorkspacePage::bindSession(const std::shared_ptr<AppSession>& session, QString* err)
{
    if (!session || !session->dataMgr || !session->sharedState) {
        if (err) {
            *err = QStringLiteral("Invalid session.");
        }
        return false;
    }

    if (viewportPage_) {
        viewportPage_->initWithData(
            session->dataMgr,
            session->sharedState,
            session->sharedStateBroadcaster);
    }

    if (sceneTreePanel_) {
        sceneTreePanel_->setSession(
            session->dataMgr,
            session->sharedState,
            session->sharedStateBroadcaster,
            session->sourcePath);
    }

    if (renderPanel_) {
        renderPanel_->setSession(session);
    }
    return true;
}

bool WorkspacePage::saveSliceStackAsync(
    const QString& outputDir,
    VizMode sliceMode,
    const double& angle,
    std::function<void(bool)> onComplete)    
{
    return viewportPage_->saveSliceStackAsync(outputDir, sliceMode, angle, std::move(onComplete));
}

bool WorkspacePage::saveTransformedDataAsync(
    const QString& outputPath,
    std::function<void(bool)> onComplete) 
{
    return viewportPage_->saveTransformedDataAsync(outputPath, std::move(onComplete));
}


