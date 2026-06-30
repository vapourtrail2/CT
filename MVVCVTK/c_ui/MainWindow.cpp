#include "AppController.h"
#include "c_ui/contextarea/WorkspacePage.h"
#include "c_ui/contextarea/WorkSpaceUIState.h"
#include "c_ui/MainWindow.h"
#include "c_ui/nav/TabMap.h"
#include "c_ui/nav/WorkspaceFlow.h"
#include "c_ui/panels/RenderPanel.h"
#include "c_ui/panels/SceneTreePanel.h"
#include "c_ui/ribbon/RibbonPage.h"
#include "c_ui/ribbon/RibbonPageRegister.h"
#include "c_ui/workbenches/AlignmentPage.h"
#include "c_ui/workbenches/AnalysisPage.h"
#include "c_ui/workbenches/AnimationPage.h"
#include "c_ui/workbenches/CADAndThen.h"
#include "c_ui/workbenches/DocumentPage.h"
#include "c_ui/workbenches/EditPage.h"
#include "c_ui/workbenches/GeometryPage.h"
#include "c_ui/workbenches/MeasurePage.h"
#include "c_ui/workbenches/ReconstructPage.h"
#include "c_ui/workbenches/ReportPage.h"
#include "c_ui/workbenches/SelectPage.h"
#include "c_ui/workbenches/StartPage.h"
#include "c_ui/workbenches/VolumePage.h"
#include "c_ui/workbenches/WindowPage.h"
#include "c_ui/windows/Titlebar.h"

#include <algorithm>
#include <array>
#include <QApplication>
#include <QComboBox>
#include <QDebug>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFileDialog>
#include <QFormLayout>
#include <QGuiApplication>
#include <QHBoxLayout>  
#include <QLabel>
#include <QMessageBox>
#include <QMetaObject>
#include <QMouseEvent>
#include <QPointer>
#include <QProgressDialog>
#include <QRect>        
#include <QScreen>
#include <QSize>
#include <QSizePolicy>  
#include <QStackedWidget>
#include <QStatusBar>
#include <QStringList>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>      
#include <qwindow.h>

#include <QVTKOpenGLNativeWidget.h>
#include <vtkActor.h>
#include <vtkAutoInit.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkOpenGLGPUVolumeRayCastMapper.h>
#include <vtkPolyDataMapper.h>
#include <vtkRenderer.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkSmartPointer.h>
#include <vtkSphereSource.h>

VTK_MODULE_INIT(vtkRenderingVolumeOpenGL2);

CTViewer::CTViewer(QWidget* parent)
    : QMainWindow(parent)
{
    //无边框窗口+深色主题 
    setWindowFlag(Qt::FramelessWindowHint);
    setWindowTitle(QStringLiteral("GviewCT"));
    setStyleSheet(QStringLiteral(
        "QMainWindow{background-color:#121212;}"
        "QMenuBar, QStatusBar{background-color:#1a1a1a; color:#e0e0e0;}"));

    tabMap_ = std::make_unique<TabMap>();

    //结构 
    buildTheTop();
    buildTheMiddle(); 
    setCommands();
    wireConnect();     
}

CTViewer::~CTViewer() {
}

void CTViewer::buildTheTop()
{
	//第一行  包含 撤回 前进 标题 最小化 最大化 关闭
    auto* topBarContainer = new QWidget(this);
    auto* topBarLayout = new QVBoxLayout(topBarContainer);
    topBarLayout->setContentsMargins(0, 0, 0, 0);//这句话的作用是去掉边框
    topBarLayout->setSpacing(0);

    topBarContainer->setAttribute(Qt::WA_StyledBackground, true);
    topBarContainer->setStyleSheet(QStringLiteral("QWidget{background-color:#202020;}"));

    buildTitleBar(topBarContainer, topBarLayout);
    buildRibbonTitleBar(topBarContainer, topBarLayout);

	setMenuWidget(topBarContainer);
}

void CTViewer::buildTheMiddle()
{
    auto totalContainer = new QWidget(this);//totalContainer是总体容器
    auto v = new QVBoxLayout(totalContainer);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);

    buildRibbonStack(totalContainer, v);
    buildRibbonTabs();
    buildContentStack(totalContainer, v);

    setCentralWidget(totalContainer);
	appController_ = new AppController(this);//持有当前打开数据的整套会话对象，
	workspaceFlow_ = std::make_unique<WorkspaceFlow>(appController_);//这句话的意思是创建一个WorkspaceFlow对象，并将appController_的指针传递给它，以便WorkspaceFlow能够与AppController进行交互和通信。这种设计通常用于实现应用程序中不同组件之间的协调和数据共享。
    applyInitialUiState();
}   

void CTViewer::wireConnect() {
    connectTabSignals();
    connectDocumentSignals();
    connectAppSignals();
}

void CTViewer::buildTitleBar(QWidget* topBarContainer, QVBoxLayout* topBarLayout) 
{
    auto* titleBar = new TitleBar(topBarContainer);
    topBarLayout->addWidget(titleBar);
}

void CTViewer::buildRibbonTitleBar(QWidget* topBarContainer, QVBoxLayout* topBarLayout) {
    //第二行  选项卡栏
    tabBar_ = new QTabBar(topBarContainer);
    tabBar_->setObjectName(QStringLiteral("mainRibbonTabBar"));
    tabBar_->setDrawBase(false);
    tabBar_->setExpanding(false);
    tabBar_->setMovable(false);
    tabBar_->setAttribute(Qt::WA_StyledBackground, true); 
    tabBar_->setStyleSheet(QStringLiteral(
        "QTabBar#mainRibbonTabBar{background-color:#202020; color:#f5f5f5;}"
        "QTabBar#mainRibbonTabBar::tab{padding:8px 16px; margin:0px; border:none; background-color:#202020;}"
        "QTabBar#mainRibbonTabBar::tab:selected{background-color:#333333;}"
        "QTabBar#mainRibbonTabBar::tab:hover{background-color:#2a2a2a;}"));

    topBarLayout->addWidget(tabBar_);
}

void CTViewer::buildRibbonTabs()
{
    if (!tabBar_ || !ribbonPageRegister_) {
        return;
    }
    
    while (tabBar_->count() > 0)
    {
        tabBar_->removeTab(0);
    }

    tabBar_->addTab(QStringLiteral("文件"));
    
    for (auto page : ribbonPageRegister_->pageslist() ) {
        tabBar_->addTab(page->tabName());
    }

    tabBar_->setCurrentIndex(TabIndex::File);
}

void CTViewer::setRibbonPage(RibbonPage* page)
{
    if (!page || !stack_ || !ribbonPageRegister_ || !tabMap_) {
        return;
    }

    connect(page, &RibbonPage::commandRequested, this, [this](const QString& name) {
        context_.getCommands().run(name);
        });

    stack_->addWidget(page);
    ribbonPageRegister_->add(page);
    tabMap_->bindTabPage(page->tabIndex(), page);
}

void CTViewer::buildRibbonStack(QWidget* totalContainer, QVBoxLayout* rootLayout) {
    ribbonPageRegister_ = std::make_unique<RibbonPageRegister>();

    stack_ = new QStackedWidget(totalContainer);
    stack_->setFixedHeight(iconHeight_);
    rootLayout->addWidget(stack_, 0);

	whatEmpty_ = new QWidget(stack_);
    stack_->addWidget(whatEmpty_);

    setRibbonPage(new StartPagePage(stack_));                                                      
    setRibbonPage(new EditPage(stack_));
    setRibbonPage(new VolumePage(stack_));
    setRibbonPage(new SelectPage(stack_));
    setRibbonPage(new AlignmentPage(stack_));
    setRibbonPage(new GeometryPage(stack_));
    setRibbonPage(new MeasurePage(stack_));
    setRibbonPage(new CADAndThen(stack_));
    setRibbonPage(new AnalysisPage(stack_));
    setRibbonPage(new ReportPage(stack_));  
    setRibbonPage(new AnimationPage(stack_));
    setRibbonPage(new WindowPage(stack_));
}

void CTViewer::buildContentStack(QWidget* totalContainer, QVBoxLayout* rootLayout) {
    secondstack_ = new QStackedWidget(totalContainer);
    rootLayout->addWidget(secondstack_, 1);

    pageDocument_ = new DocumentPage(secondstack_);
    secondstack_->addWidget(pageDocument_);

	buildWorkspacePage();
	buildEmptyPage();
}

void CTViewer::buildWorkspacePage() {
    workspacePage_ = new WorkspacePage(secondstack_);
    secondstack_->addWidget(workspacePage_);
}

void CTViewer::buildEmptyPage() {
    //  Empty 页：无数据提示
    emptyPage_ = new QWidget(secondstack_);
    emptyPage_->setStyleSheet(QStringLiteral("background-color:#000000;"));

    auto ev = new QVBoxLayout(emptyPage_);
    ev->setContentsMargins(0, 0, 0, 0);
    ev->setSpacing(0);

    auto tip = new QLabel(QStringLiteral("请先在“文件”中加载数据"), emptyPage_);
    tip->setAlignment(Qt::AlignCenter);
    tip->setStyleSheet(QStringLiteral("color:#808080;"));
    ev->addWidget(tip, 1);
    secondstack_->addWidget(emptyPage_);
}

void CTViewer::applyInitialUiState() {
    if (stack_) {
        stack_->setCurrentWidget(whatEmpty_);
        // 文件页启动时隐藏顶部图标栏，并清零高
        stack_->setFixedHeight(0);  
        stack_->setVisible(false);
    }
    if (secondstack_ && pageDocument_) {
        secondstack_->setCurrentWidget(pageDocument_);
    }
}

void CTViewer::connectTabSignals() {
    connect(tabBar_, &QTabBar::currentChanged, this, &CTViewer::onTabChanged);
}

void CTViewer::connectDocumentSignals() {
    connect(pageDocument_, &DocumentPage::moduleClicked, this, [this](const QString& msg) {
        statusBar()->showMessage(msg, 1500);
        });

    connect(pageDocument_, &DocumentPage::recentOpenRequested, this, [this](const QString& name) {
        statusBar()->showMessage(QStringLiteral("正在打开 %1 ...").arg(name), 1500);
        });

    connect(pageDocument_, &DocumentPage::openRequested, this, &CTViewer::onOpenRequested);
}


void CTViewer::setCommands()
{
    //为什么不直接写函数
    context_.getCommands().add(QStringLiteral("recon.open"), [this]() {
        openCtReconUi();
        });
    context_.getCommands().add(QStringLiteral("image.save"), [this]() {
        showSaveTransformedDataDialog();
        });
    context_.getCommands().add(QStringLiteral("slicestack.save"), [this]() {
        showSaveSliceStackDialog();
        });
}

//void CTViewer::connectDistanceSignals() {
  //  connect(pageStart_, &StartPagePage::distanceRequested, this, [this]() {
		//if (mprViews_) mprViews_->setToolMode(ToolMode::DistanceMeasure); // 切换到距离测量工具
  //  });
//}

//void CTViewer::connectAngelSignals() {
    //connect(pageStart_, &StartPagePage::angleRequested, this, [this]() {
    //    if (mprViews_) mprViews_->setToolMode(ToolMode::AngleMeasure); // 切换到距离测量工具
    //    });
//}

void CTViewer::connectAppSignals() {
    if (!appController_) {
        return;
    }

    connect(appController_, &AppController::sessionChanged, this,
        [this](const std::shared_ptr<AppSession>& session) {
            handleSessionChanged(session);
        });
}

//void CTViewer::connectRenderSwitchSignals()
//{
//    connect(workspacePage_->getRenderPanel(),&WorkSpaceUIState::primary3DModeChanged, this, [this](VizMode mode) {
//        if (primary3DMode_) {
//            workspacePage_->getViewportPage()->setPrimary3DMode(mode);
//        }
//        });
//}

//架构优化 buildxxx 和 applyxxx分离，build只负责算，apply只负责改界面 
UiState CTViewer::buildUiState(int index) const{
    UiState state;
    state.tabIndex = index;

    //业务集中在一个地方
    const bool hasData = workspaceFlow_ && workspaceFlow_->hasData();
    //文件页
    if (tabMap_ && tabMap_->isFileTab(index)) {
		state.showRibbon = false;
        state.ribbonHeight = 0;
		state.contentTarget = ContentTarget::Document;
        state.ribbonPage = whatEmpty_;
        return state;
    }

	//其他页
    state.showRibbon = true;
	state.ribbonHeight = iconHeight_;
    state.contentTarget = hasData ? ContentTarget::Workspace : ContentTarget::Empty;
    //tab对应哪个ribbon，统一从Tabmap取
    state.ribbonPage = tabMap_ ? tabMap_->tabPage(index) : nullptr;
	return state;
}

void CTViewer::applyUiState(const UiState& state) {
    //界面赋值动作 UI问题只看这一个函数
    if (!stack_ || !secondstack_) {
        return;
    }
    stack_->setFixedHeight(state.ribbonHeight);
    stack_->setVisible(state.showRibbon);

    if (!state.showRibbon) {
        if (whatEmpty_) {
            stack_->setCurrentWidget(whatEmpty_);
        }
    }
    else if (state.ribbonPage) {
		stack_->setCurrentWidget(state.ribbonPage);
    }

    switch (state.contentTarget) {
    case ContentTarget::Document:
        if (pageDocument_) {
            secondstack_->setCurrentWidget(pageDocument_);
        }
        break;
    case ContentTarget::Workspace:
        if (workspacePage_) {
            secondstack_->setCurrentWidget(workspacePage_);
        }
        break;
    case ContentTarget::Empty:
        if (emptyPage_) {
            secondstack_->setCurrentWidget(emptyPage_);
        }
        break;
    }
}


void CTViewer::onTabChanged(int index) {
    const UiState state = buildUiState(index);
    applyUiState(state);
}

void CTViewer::onOpenRequested(const QString& path, const std::array<float,3>& spacing, const std::array<float, 3>& origin) 
{
    setCloseProgressDialog();

    setOpenProgressDialog(QStringLiteral("loading"), QStringLiteral("loading"));

    QString err;
    const bool ok = workspaceFlow_ && workspaceFlow_->openFile(path,spacing,origin, &err);

    if (!ok) {
        if (ProgressDialog_) {
            setCloseProgressDialog();
        }

        const QString msg = err.isEmpty() ? QStringLiteral("打开失败") : err;
        statusBar()->showMessage(msg, 3000);
        if (pageDocument_) {
            pageDocument_->notifyFail(msg);
        }
        return;
    }

    if (pageDocument_) {
        pageDocument_->closeOpenDialog();
    }
}

//保存切片
void CTViewer::showSaveSliceStackDialog()
{
    if (!workspacePage_ || !workspaceFlow_ ||!workspaceFlow_->hasData()) {
        QMessageBox::warning(this, QStringLiteral("保存图像堆栈"), QStringLiteral("请先加载数据。"));
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("保存影片/图像堆栈"));
    dialog.resize(400, 270);

    auto* root = new QVBoxLayout(&dialog);
    auto* body = new QHBoxLayout();

    auto* form = new QFormLayout();
    auto* direction = new QComboBox(&dialog);
    direction->addItem(QStringLiteral("从上到下(轴向)"), static_cast<int>(VizMode::SliceTop_down));
    direction->addItem(QStringLiteral("从前到后(径向)"), static_cast<int>(VizMode::SliceFront_back));
    direction->addItem(QStringLiteral("从左到右(切向)"), static_cast<int>(VizMode::SliceLeft_right));

    form->addRow(QStringLiteral("方向:"), direction);
  
    auto* angle = new QDoubleSpinBox(this);
    angle->setRange(0.0, 180.0);
    angle->setDecimals(1);
    angle->setSingleStep(1.0);
    angle->setSuffix("  deg");
    angle->setValue(0.0);

    form->addRow(QStringLiteral("角度:"), angle);

    auto* statusLabel = new QLabel(QStringLiteral("选择方向和输入角度后点击保存。"), &dialog);
    statusLabel->setStyleSheet(QStringLiteral("color:#cfcfcf;"));
    form->addRow(statusLabel);

    body->addLayout(form);

    auto* buttons = new QDialogButtonBox(&dialog);
    auto* saveButton = buttons->addButton(QStringLiteral("保存..."), QDialogButtonBox::AcceptRole);
    buttons->addButton(QStringLiteral("取消"), QDialogButtonBox::RejectRole);

    root->addLayout(body);
    root->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    connect(saveButton, &QPushButton::clicked, &dialog, [this, &dialog, direction, angle,statusLabel, saveButton]() {
        const QString dir = QFileDialog::getExistingDirectory(//弹出一个“选择文件夹”系统对话框
            &dialog,//父窗口
            QStringLiteral("选择保存目录"));

        if (dir.isEmpty()) {
            return;
        }

        const auto mode = static_cast<VizMode>(direction->currentData().toInt());
        const auto angleValue = static_cast<double>(angle->value());

        saveButton->setEnabled(false);
		setOpenProgressDialog(QStringLiteral("saving..."), QStringLiteral("saving"));

        QPointer<QDialog> dialogPtr(&dialog);
        QPointer<QLabel> statusPtr(statusLabel);
        QPointer<QPushButton> saveButtonPtr(saveButton);

        const bool started = workspacePage_->saveSliceStackAsync(
            dir,
            mode,
            angleValue,
            [this,dialogPtr, statusPtr, saveButtonPtr](bool ok) {
                QMetaObject::invokeMethod(qApp, [this,dialogPtr, statusPtr, saveButtonPtr, ok]() {
                    setCloseProgressDialog();

                    if (statusPtr) {
                        statusPtr->setText(ok
                            ? QStringLiteral("保存完成。")
                            : QStringLiteral("保存失败。"));
                    }

                    if (saveButtonPtr) {
                        saveButtonPtr->setEnabled(true);
                    }

                    if (ok) {
                        dialogPtr->accept();
                    }
                    }, Qt::QueuedConnection);
            });

        if (!started) {
            setCloseProgressDialog();
            statusLabel->setText(QStringLiteral("保存任务启动失败。"));
            saveButton->setEnabled(true);
        }
        });

    dialog.exec();
}

//保存图像
void CTViewer::showSaveTransformedDataDialog()
{
    if (!workspacePage_->getViewportPage() || !workspaceFlow_ || !workspaceFlow_->hasData()) {
        QMessageBox::warning(this, QStringLiteral("保存图像"), QStringLiteral("请先加载数据。"));
        return;
    }

    const QString path = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("保存图像"),
        QString(),
        QStringLiteral("RAW 文件 (*.raw);;所有文件 (*.*)"));

    if (path.isEmpty()) {
        return;
    }

    if (auto* bar = statusBar()) {
        setOpenProgressDialog(
            QStringLiteral("saving..."),
            QStringLiteral("saving"));
    }

    QPointer<CTViewer> self(this);

    const bool started = workspacePage_->saveTransformedDataAsync(
        path,
        [this,self](bool ok) {
            QMetaObject::invokeMethod(qApp, [this,self, ok]() {
                
                setCloseProgressDialog();
                if (auto* bar = self->statusBar()) {
                    bar->showMessage(
                        ok ? QStringLiteral("保存完成。") : QStringLiteral("保存失败。"),
                        3000);
                }
                }, Qt::QueuedConnection);
        });

    if (!started) {
		setCloseProgressDialog();
        if (auto* bar = statusBar()) {
            bar->showMessage(QStringLiteral("保存任务启动失败。"), 3000);
        }
    }
}

void CTViewer::setOpenProgressDialog(const QString& text, const QString& title)
{
    setCloseProgressDialog();

    ProgressDialog_ = new QProgressDialog(
        text,
        QString(),
        0,
        0,
        this);

    ProgressDialog_->setWindowTitle(title);
    ProgressDialog_->setWindowModality(Qt::ApplicationModal);
    ProgressDialog_->setMinimumDuration(0);
    ProgressDialog_->setAutoClose(false);
    ProgressDialog_->setAutoReset(false);
    ProgressDialog_->setCancelButton(nullptr);
    ProgressDialog_->setRange(0, 0);
    ProgressDialog_->show();
}

void CTViewer::setCloseProgressDialog()
{
    if (!ProgressDialog_) {
        return;
    }

    ProgressDialog_->close();
    ProgressDialog_.clear();
}

void CTViewer::handleSessionChanged(const std::shared_ptr<AppSession>& session)
{
    if (!workspacePage_) {
        return;
    }

    if (!session) {
        if (ProgressDialog_) {
			setCloseProgressDialog();
        }

        loadNotifyToken_.reset();

        if (tabBar_) {
            applyUiState(buildUiState(tabBar_->currentIndex()));
        }
        return;
    }

    QString err;
    const bool ok = workspacePage_->bindSession(session, &err);

    if (!ok) {
        if (ProgressDialog_) {
            setCloseProgressDialog();
        }

        loadNotifyToken_.reset();

        if (auto* bar = statusBar()) {
            bar->showMessage(
                err.isEmpty() ? QStringLiteral("Failed to bind workspace session.") : err,
                3000);
        }
        return;
    }

    loadNotifyToken_ = std::make_shared<int>(1);

    session->sharedStateBroadcaster->SetObserver(loadNotifyToken_, [this](UpdateFlags flags) {
        if (HasFlag(flags, UpdateFlags::DataReady)) {
            QMetaObject::invokeMethod(this, [this]() {
                if (ProgressDialog_) {
                    setCloseProgressDialog();
                }

                loadNotifyToken_.reset();

                if (pageDocument_) {
                    pageDocument_->notifySucc();
                }
                }, Qt::QueuedConnection);
            return;
        }

        if (HasFlag(flags, UpdateFlags::LoadFailed)) {
            QMetaObject::invokeMethod(this, [this]() {
                if (ProgressDialog_) {
                    setCloseProgressDialog();
                }

                loadNotifyToken_.reset();

                if (pageDocument_) {
                    pageDocument_->notifyFail(QStringLiteral("加载失败"));
                }

                if (auto* bar = statusBar()) {
                    bar->showMessage(QStringLiteral("加载失败"), 3000);
                }
                }, Qt::QueuedConnection);
        }
        });

    if (!tabBar_) {
        return;
    }

    if (tabBar_->currentIndex() == TabIndex::File) {
        tabBar_->setCurrentIndex(TabIndex::Start);
        return;
    }

    applyUiState(buildUiState(tabBar_->currentIndex()));
}