#include "c_ui/contextarea/WorkspacePage.h"
#include "c_ui/viewport/ViewportGather.h"
#include "c_ui/panels/SceneTreePanel.h"
#include "c_ui/panels/RenderPanel.h"
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

    workspaceSplit_ = new QSplitter(Qt::Horizontal, this);
    workspaceSplit_->setObjectName("workspaceSplit");
    root->addWidget(workspaceSplit_, 1);

    //左侧
    viewportGather_ = new ViewportGather(workspaceSplit_);

    //右侧
    rightSplit_ = new QSplitter(Qt::Vertical, workspaceSplit_);
    rightSplit_->setObjectName("rightsplit");

    renderPanel_ = new RenderPanel(rightSplit_);  
    renderPanel_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    renderPanel_->setMaximumHeight(270);
    rightSplit_->addWidget(renderPanel_);

    /*connect(
        viewportGather_,
        &ViewportGather::windowLevelStateChanged,
        renderPanel_,
        &RenderPanel::setWindowLevelState);*/

    sceneTreePanel_ = new SceneTreePanel(rightSplit_);
    rightSplit_->addWidget(sceneTreePanel_);

    rightSplit_->setStretchFactor(0, 0);
    rightSplit_->setStretchFactor(1, 1);
    rightSplit_->setSizes({ 270, 260 });

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

HostSessionConfig WorkspacePage::getHostConfig() const
{
    if (!viewportGather_)
    {
        return {};
    }

    return viewportGather_->getHostConfig();
}

void WorkspacePage::setDataState(
    bool hasData,
    const QString& sourcePath)
{
    sceneTreePanel_->setDataState(hasData,sourcePath);
    /*renderPanel_->setDataState(hasData);

    if (!hasData) {
        viewportGather_->resetWindowLevelState();
    }*/
}
