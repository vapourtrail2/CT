#include "c_ui/contextarea/WorkspacePage.h"
#include "c_ui/workbenches/ReconstructPage.h"
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



