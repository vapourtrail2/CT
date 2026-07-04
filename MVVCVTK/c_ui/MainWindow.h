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
#include "c_ui/qt/QtRenderContext.h"
#include "c_ui/nav/UIState.h"
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
class UIReconstruct3D;
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

private:
    void buildTheTop();
    void buildTheMiddle();
    void wireConnect();
    void openCtReconUi();

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
    void handleSessionChanged(const std::shared_ptr<AppSession>& session);

    void setCommands();

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
    QPointer<QWidget> whatEmpty_;
   
    QPointer<QWidget> emptyPage_;

    QPointer<QTabBar> tabBar_;

    QPointer<DocumentPage> pageDocument_;
    QPointer<QStackedWidget> stack_;
    QPointer<QStackedWidget> secondstack_;
	QPointer<QtRenderContext> renderContext_;

    std::unique_ptr<TabMap> tabMap_;

    UiState buildUiState(int index) const;
    void applyUiState(const UiState& state);

    UIReconstruct3D* uiRecon3d_ = nullptr;  

    QPointer<QProgressDialog> ProgressDialog_;
    std::shared_ptr<void> loadNotifyToken_;//进度条   

    QPointer<WorkspacePage> workspacePage_;
    std::unique_ptr<RibbonPageRegister> ribbonPageRegister_;

    int iconHeight_ = 100;

    AppContext context_;
};
