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

#include "c_ui/command/commands.h"
#include "c_ui/context/AppContext.h"

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

class RenderPanel;
class SceneTreePanel;
class TabMap;
class WorkspaceFlow;
class WorkspacePage;
class RibbonPageRegister;
class RibbonPage;
class UIReconstruct3D;
class QProgressDialog;

class CTViewer : public QMainWindow
{
    Q_OBJECT
public:
    explicit CTViewer(QWidget* parent = nullptr);
    ~CTViewer();

private:
    void buildTheTop();
    void buildTheMiddle();
    void wireConnect();
    void buildTitleBar(QWidget* topBarContainer, QVBoxLayout* topBarLayout);
    void buildRibbonTitleBar(QWidget* topBarContainer, QVBoxLayout* topBarLayout);
    void buildRibbonTabs();
    void buildRibbonStack(QWidget* totalContainer, QVBoxLayout* rootLayout);
    void buildContentStack(QWidget* totalContainer, QVBoxLayout* rootLayout);
    void buildWorkspacePage();
    void buildEmptyPage();
    void applyInitialUiState();
    void connectTabSignals();
    void connectDocumentSignals();
    void connectAppSignals();
    void connectRenderPanel();
    void handleSessionChanged(
        SessionManager::State state);
    void handleLoadFinished(
        bool issucc,
        QString message);
    void setCommands();
    void onTabChanged(int index);
    void onOpenRequested(const QString& path,
        const std::array<int,3>& dims,
        const std::array<float,3> & spacing,
        const std::array<float,3>& origin);
    void showSaveSliceStackDialog();
    void showSaveTransformedDataDialog();
    void showMeasureToolsDialog();
    void setOpenProgressDialog(const QString& text, const QString& title);
    void setCloseProgressDialog();
    void setRibbonPage(RibbonPage* page);
    void openCtReconUi();
    void openCtReconUi2();
    void set3DMode(HostRenderMode mode,HostVisibilityParams visibility);
    void setVisibility(HostVisibilityParams visibility);
    void setIsoValue(double isoValue);
    void startBoxCrop();
    void startPlaneCrop();
    void updateCropTree();
    void handleCropBuildFinished(bool isSuccess, QString message);
   /* void setWindowLevel(HostWindowLevelParams windowLevel);*/

private:
    UiState buildUiState(int index) const;
    void applyUiState(const UiState& state);

private:
    QPointer<QWidget> whatEmpty_;
    QPointer<QWidget> emptyPage_;
    QPointer<QTabBar> tabBar_;
    QPointer<DocumentPage> pageDocument_;
    QPointer<QStackedWidget> stack_;
    QPointer<QStackedWidget> secondstack_;
    UIReconstruct3D* uiRecon3d_ = nullptr;
    QPointer<QProgressDialog> ProgressDialog_;
    QPointer<WorkspacePage> workspacePage_;
    std::unique_ptr<RibbonPageRegister> ribbonPageRegister_;
    AppContext context_;

private:
    int iconHeight_ = 100;
    bool fileHasData_ = false;
};
