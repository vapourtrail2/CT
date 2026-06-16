#pragma once
#include <QMainWindow>
#include <QPointer>
#include <QPoint>
#include <QToolButton>
#include <QLabel>
#include <QStackedWidget>
#include <QTabBar>
#include <QSplitter>
#include <array>
#include <memory>
#include <QDoubleSpinBox>
#include "c_ui/workbenches/ReconstructPage.h"
#include "c_ui/nav/UIState.h"

class QVBoxLayout;
class DocumentPage;
class StartPagePage;
class EditPage;
class VolumePage;
class SelectPage;
class AlignmentPage;
class GeometryPage;
class MeasurePage;
class CADAndThen;
class AnalysisPage;
class WindowPage;
class ReportPage;
class AnimationPage;
class ReconstructPage;
class UIReconstruct3D;
class AppController;
struct AppSession;
class RenderPanel;
class SceneTreePanel;

class QProgressDialog;//进度条

class TabMap;
class WorkspaceFlow;
class WorkspacePage;
class RibbonPageRegister;
class RibbonPage;

class CTViewer : public QMainWindow
{
    Q_OBJECT
public:
    explicit CTViewer(QWidget* parent = nullptr);
    ~CTViewer();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void buildTheTop();
    void buildTheMiddle();
    void wireConnect();
    void updateMaximizeButtonIcon();
    void openCtReconUi();

    void buildTitleBar(QWidget* topBarContainer, QVBoxLayout* topBarLayout);
    void buildRibbonTitleBar(QWidget* topBarContainer, QVBoxLayout* topBarLayout);
    void buildRibbonTabs();
    void connectWindowButtonSignals();

    void buildRibbonStack(QWidget* totalContainer, QVBoxLayout* rootLayout);
    void buildContentStack(QWidget* totalContainer, QVBoxLayout* rootLayout);
    void buildWorkspacePage();
    void buildEmptyPage();
    void applyInitialUiState();

    void connectTabSignals();
    void connectDocumentSignals();
    void connectReconSignals();
    void connectDistanceSignals();
    void connectAngelSignals();
    void connectAppSignals();
    void connectRenderSwitchSignals();
    void handleSessionChanged(const std::shared_ptr<AppSession>& session);

    void onTabChanged(int index);
    void onOpenRequested(const QString& path, 
        const std::array<float,3> & spacing,
        const std::array<float,3>& origin);

    void showSaveSliceStackDialog();
    void showSaveTransformedDataDialog();

    void setOpenProgressDialog(const QString& text, const QString& title);
    void setCloseProgressDialog();

    void setRibbonPage(RibbonPage* page);

private:
    bool draggingWindow_ = false;
    QPoint dragOffset_;

    QPointer<QWidget> titleBar_;
    QPointer<QWidget> titleLeftArea_;
    QPointer<QWidget> titleCenterArea_;

    QPointer<QWidget> whatEmpty_;
   
    QPointer<QWidget> emptyPage_;

    QPointer<QLabel> titleLabel_;
    QPointer<QToolButton> btnTitleUndo_;
    QPointer<QToolButton> btnTitleUndo02_;
    QPointer<QToolButton> btnMinimize_;
    QPointer<QToolButton> btnMaximize_;
    QPointer<QToolButton> btnClose_;
    QPointer<QTabBar> tabBar_;

    QPointer<DocumentPage> pageDocument_;
    QPointer<StartPagePage> pageStart_;
    QPointer<EditPage> pageEdit_;
    QPointer<VolumePage> pageVolume_;
    QPointer<SelectPage> pageSelect_;
    QPointer<AlignmentPage> pageAlignment_;
    QPointer<GeometryPage> pageGeometry_;
    QPointer<MeasurePage> pageMeasure_;
    QPointer<CADAndThen> pageCAD_;
    QPointer<AnalysisPage> pageAnalysis_;
    QPointer<WindowPage> pageWindow_;
    QPointer<ReportPage> pageReport_;
    QPointer<AnimationPage> pageAnimation_;
    QPointer<QStackedWidget> stack_;
    QPointer<QStackedWidget> secondstack_;
	QPointer<QtRenderContext> renderContext_;

    std::unique_ptr<TabMap> tabMap_;
    std::unique_ptr<WorkspaceFlow> workspaceFlow_;

    UiState buildUiState(int index) const;
    void applyUiState(const UiState& state);

    UIReconstruct3D* uiRecon3d_ = nullptr;  
    QPointer<AppController> appController_;

    QPointer<QProgressDialog> ProgressDialog_;
    std::shared_ptr<void> loadNotifyToken_;//进度条   

    QPointer<WorkspacePage> workspacePage_;
    std::unique_ptr<RibbonPageRegister> ribbonPageRegister_;

    int iconHeight_ = 100;
};
