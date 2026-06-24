#pragma once

#include <QWidget>
#include <QPointer>

class QSplitter;
class ReconstructPage;
class SceneTreePanel;
class RenderPanel;
class WorkSpaceUIState;

class WorkspacePage : public QWidget {
	Q_OBJECT
public:
	explicit WorkspacePage(QWidget* parent = nullptr);

	ReconstructPage* getViewportPage() const;//getter
	SceneTreePanel* getSceneTreePanel() const;
	RenderPanel* getRenderPanel() const;

private:
	void buildUi();	

private:
	QPointer<QSplitter> workspaceSplit_;
	QPointer<QSplitter> rightSplit_;
	QPointer<ReconstructPage> viewportPage_;
	QPointer<SceneTreePanel> sceneTreePanel_;
	QPointer<RenderPanel> renderPanel_;
	QPointer<WorkSpaceUIState> workSpaceUIState_;
};
