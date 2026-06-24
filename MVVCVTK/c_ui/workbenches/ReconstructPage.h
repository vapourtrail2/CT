#pragma once
#include <QWidget>
#include <QPointer> 
#include <QString>
#include <functional>
#include <memory>
#include "Service/AppService.h"
#include "Data/DataManager.h"
#include "App/AppState.h"
#include "c_ui/qt/QtRenderContext.h"

class QGridLayout;
class QToolButton;

class ReconstructPage : public QWidget
{
    Q_OBJECT
public:
    explicit ReconstructPage(QWidget* parent = nullptr);
    void initWithData(
        std::shared_ptr<AbstractDataManager> data,
        std::shared_ptr<SharedInteractionState> state,
        std::shared_ptr<SharedStateBroadcaster> broadcaster);
    void setPrimary3DMode(VizMode mode);
    /*void setToolMode(ToolMode mode);*/

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

   /* void applyPrimary3DMode(VizMode mode);*/
    void request3DRebuildFromCurrentImage();
        
    QPointer<QWidget> viewAxial_;
    QPointer<QWidget> viewSagittal_;
    QPointer<QWidget> viewCoronal_;
    QPointer<QWidget> viewReserved_;

    std::shared_ptr<AbstractDataManager> m_dataMgr;
	std::shared_ptr<SharedStateBroadcaster> m_stateBroadcaster;
    std::shared_ptr<SharedInteractionState> m_sharedState;
    std::shared_ptr<void> m_lifeToken;//状态位管理

    std::shared_ptr<MedicalVizService> m_svcAxial;
    std::shared_ptr<QtRenderContext>   m_ctxAxial;
    std::shared_ptr<MedicalVizService> m_svcCoronal;
    std::shared_ptr<QtRenderContext>   m_ctxCoronal;
    std::shared_ptr<MedicalVizService> m_svcSagittal;
    std::shared_ptr<QtRenderContext>   m_ctxSagittal;
    std::shared_ptr<MedicalVizService> m_svc3D;
    std::shared_ptr<QtRenderContext>   m_ctx3D;

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

    QWidget* createViewportContainer(QPointer<QWidget>& vtkView, ViewportId id);
    void switchViewMaximized(ViewportId id);
    void setViewportLayout();
};
