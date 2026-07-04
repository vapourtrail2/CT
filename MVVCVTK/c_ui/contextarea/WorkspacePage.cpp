#include "c_ui/contextarea/WorkspacePage.h"
#include "c_ui/viewport/ViewportGather.h"
#include "c_ui/panels/SceneTreePanel.h"
#include "c_ui/panels/RenderPanel.h"
#include "c_ui/contextarea/WorkSpaceUIState.h"
#include "c_ui/context/DataSet.h"

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
    viewportGather_ = new ViewportGather(workspaceSplit_);

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
    connect(workSpaceUIState_, &WorkSpaceUIState::primary3DModeChanged, viewportGather_, &ViewportGather::setPrimary3DMode);
    
    viewportGather_->setPrimary3DMode(workSpaceUIState_->getPrimary3DMode());
    //安装
    workspaceSplit_->addWidget(viewportGather_);
    workspaceSplit_->addWidget(rightSplit_);
    workspaceSplit_->setStretchFactor(0, 50);
    workspaceSplit_->setStretchFactor(1, 3);
}

ViewportGather* WorkspacePage::getViewportGather() const
{
    return viewportGather_;
}

SceneTreePanel* WorkspacePage::getSceneTreePanel() const
{
    return sceneTreePanel_;
}

RenderPanel* WorkspacePage::getRenderPanel() const
{
    return renderPanel_;
}

bool WorkspacePage::bindSession(const Dataset& dataset, QString* err)
{
    if (!dataset.getValid()) {
        if (err) {
            *err = QStringLiteral("Invalid session.");
        }
        return false;
    }

    viewportGather_->initWithData(dataset);
    sceneTreePanel_->setSession(dataset);
    renderPanel_->setSession(dataset);
  
    return true;
}

bool WorkspacePage::saveSliceStackAsync(
    const QString& outputDir,
    VizMode sliceMode,
    const double& angle,
    std::function<void(bool)> onComplete)    
{
    return viewportGather_->saveSliceStackAsync(outputDir, sliceMode, angle, std::move(onComplete));
}

bool WorkspacePage::saveTransformedDataAsync(
    const QString& outputPath,
    std::function<void(bool)> onComplete) 
{
    return viewportGather_->saveTransformedDataAsync(outputPath, std::move(onComplete));
}


