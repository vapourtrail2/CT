#pragma once

#include "App/AppTypes.h"
#include <functional>
#include <memory>
#include <QPointer>
#include <QString>
#include <QWidget>

struct Dataset;

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

	bool bindSession(const Dataset& dataset,
		QString* err = nullptr);

	bool saveSliceStackAsync(
		const QString& outputDir,
		VizMode sliceMode,
		const double& angle,
		std::function<void(bool)> onComplete = nullptr);

	bool saveTransformedDataAsync(
		const QString& outputPath,
		std::function<void(bool)> onComplete = nullptr);

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
