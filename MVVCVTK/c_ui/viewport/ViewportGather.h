#pragma once
#include "App/AppState.h"
#include "c_ui/context/Dataset.h"
#include "c_ui/qt/QtRenderContext.h"
#include "c_ui/viewport/Viewport.h"
#include "Data/DataManager.h"
#include "Service/AppService.h"
#include <functional>
#include <memory>
#include <QPointer> 
#include <QString>
#include <QWidget>

class QGridLayout;
class QToolButton;

class ViewportGather : public QWidget
{
    Q_OBJECT
public:
    explicit ViewportGather(QWidget* parent = nullptr);
    void initWithData(const Dataset& dataset);
    void setPrimary3DMode(VizMode mode);    

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
    void refreshViews();
    void request3DRebuildFromCurrentImage();
        
    std::shared_ptr<AbstractDataManager> m_dataMgr;
	std::shared_ptr<SharedStateBroadcaster> m_stateBroadcaster;
    std::shared_ptr<SharedInteractionState> m_sharedState;
    std::shared_ptr<void> m_lifeToken;//状态位管理

    Viewport m_axial;//z
	Viewport m_coronal;//y
	Viewport m_sagittal;//x
    Viewport m_view3d;

    VizMode m_current3DMode = VizMode::CompositeIsoSurface;

	//以下是放大缩小视口相关的成员
    enum class ViewportId {
        None,
        Axial,
        Sagittal,
        Coronal,
        View3D
    };

    QGridLayout* m_viewGrid = nullptr;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  

    QPointer<QWidget> viewAxialContainer_;
    QPointer<QWidget> viewSagittalContainer_;
    QPointer<QWidget> viewCoronalContainer_;
    QPointer<QWidget> view3DContainer_;

    ViewportId m_maximizedViewport = ViewportId::None;

    QWidget* createViewportContainer(Viewport& vp, ViewportId id);
    void switchViewMaximized(ViewportId id);
    void setViewportLayout();
};
