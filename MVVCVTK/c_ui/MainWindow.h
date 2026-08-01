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
class QProgressDialog;//进度条

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
    void handleSessionChanged(
        SessionManager::State state);
    void handleLoadFinished(
        bool issucc,
        QString message);
    void setCommands();
    void onTabChanged(int index);
    void onOpenRequested(const QString& path, 
        const std::array<float,3> & spacing,
        const std::array<float,3>& origin);
    void showSaveSliceStackDialog();
    void showSaveTransformedDataDialog();
    void showMeasureToolsDialog();
    void setOpenProgressDialog(const QString& text, const QString& title);
    void setCloseProgressDialog();
    void setRibbonPage(RibbonPage* page);
    void openCtReconUi();
    void setPrimary3DMode(HostRenderMode mode);
    void setVisibility(HostVisibilityParams visibility);

private:
    QPointer<QWidget> whatEmpty_;
    QPointer<QWidget> emptyPage_;
    QPointer<QTabBar> tabBar_;
    QPointer<DocumentPage> pageDocument_;
    QPointer<QStackedWidget> stack_;
    QPointer<QStackedWidget> secondstack_;


    UiState buildUiState(int index) const;
    void applyUiState(const UiState& state);

    UIReconstruct3D* uiRecon3d_ = nullptr;  

    QPointer<QProgressDialog> ProgressDialog_;

    QPointer<WorkspacePage> workspacePage_;
    std::unique_ptr<RibbonPageRegister> ribbonPageRegister_;

    int iconHeight_ = 100;
    AppContext context_;
};
