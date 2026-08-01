#pragma once
#include "Host/Types/HostSessionTypes.h"
#include "App/AppTypes.h"
#include <functional>
#include <memory>
#include <QPointer>
#include <QString>
#include <QWidget>

class QSplitter;
class ViewportGather;
class SceneTreePanel;
class RenderPanel;
class WorkSpaceUIState;

class WorkspacePage : public QWidget {
	Q_OBJECT
public:
	explicit WorkspacePage(QWidget* parent = nullptr);

	ViewportGather* getViewportGather() const;
	SceneTreePanel* getSceneTreePanel() const;
	RenderPanel* getRenderPanel() const;

	HostSessionConfig getHostConfig() const;

	void setDataState(
		bool hasData,
		const QString& sourcePath);

signals:
	void primary3DModeRequested(
		HostRenderMode mode);

	void visibilityRequested(
		HostVisibilityParams visibility);
private:
	void buildUi();	

private:
	QPointer<QSplitter> workspaceSplit_;
	QPointer<QSplitter> rightSplit_;
	QPointer<ViewportGather> viewportGather_;
	QPointer<SceneTreePanel> sceneTreePanel_;
	QPointer<RenderPanel> renderPanel_;
	QPointer<WorkSpaceUIState> workSpaceUIState_;
};
