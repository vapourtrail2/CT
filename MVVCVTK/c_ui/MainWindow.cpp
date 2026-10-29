#include "c_ui/context/SessionManager.h"
#include "c_ui/GapAnalysisDialog.h"
#include "c_ui/contextarea/WorkspacePage.h"
#include "c_ui/MainWindow.h"
#include "c_ui/panels/RenderPanel.h"
#include "c_ui/panels/SceneTreePanel.h"
#include "c_ui/viewport/ViewportGather.h"
#include "c_ui/ribbon/RibbonPage.h"
#include "c_ui/windows/Titlebar.h"
#include "c_ui/workbenches/DocumentPage.h"
#include "measure/MeasureToolDialog.h"
#include "MVVCVTK/API/Host/Types/HostRequestTypes.h"
#include "c_ui/external/online_reconstruction_dialog.h"
#include "uireconstruct3d.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <cmath>
#include <memory>
#include <map>
#include <limits>
#include <iostream>

#include <QShortcut>
#include <QKeySequence>
#include <QTimer>
#include <QApplication>
#include <QComboBox>
#include <QDebug>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEvent>
#include <QElapsedTimer>
#include <QCloseEvent>
#include <QFileDialog>
#include <QFormLayout>
#include <QGuiApplication>
#include <QHBoxLayout>  
#include <QInputDialog>
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
#include <QThread>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>      
#include <qwindow.h>
#include <utility>
#include <vector>

#include <QVTKOpenGLNativeWidget.h>
#include <vtkActor.h>
#include <vtkAutoInit.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkImageData.h>
#include <vtkOpenGLGPUVolumeRayCastMapper.h>
#include <vtkPolyDataMapper.h>
#include <vtkRenderer.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkSmartPointer.h>
#include <vtkSphereSource.h>

VTK_MODULE_INIT(vtkRenderingVolumeOpenGL2);

std::array<HostRenderViewRole, 3> SliceRoles{
            HostRenderViewRole::TopDownSlice,
            HostRenderViewRole::FrontBackSlice,
            HostRenderViewRole::LeftRightSlice
};

CTViewer::CTViewer(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowFlag(Qt::FramelessWindowHint);
    setWindowTitle(QStringLiteral("GviewCT"));
    setStyleSheet(QStringLiteral(
        "QMainWindow{background-color:#121212;}"
        "QMenuBar, QStatusBar{background-color:#1a1a1a; color:#e0e0e0;}"));

    buildTheTop();
    buildTheMiddle(); 
    setCommands();
    wireConnect(); 
    auto* showA = new QShortcut(QKeySequence(QStringLiteral("Ctrl+1")), this);
    showA->setAutoRepeat(false);
    connect(showA, &QShortcut::activated, this, [this]() {
        switchVolumeForTest(true);
        });

    auto* showB = new QShortcut(QKeySequence(QStringLiteral("Ctrl+2")), this);
    showB->setAutoRepeat(false);
    connect(showB, &QShortcut::activated, this, [this]() {
        switchVolumeForTest(false);
        });
}

CTViewer::~CTViewer() {
}

void CTViewer::closeEvent(QCloseEvent* event)
{
    isClosing_ = true;
    setEnabled(false);

    // 1. 检查是否存在当前窗口的模态子窗口
    QWidget* modalWidget = QApplication::activeModalWidget();

    if (modalWidget != nullptr &&
        modalWidget != this &&
        isAncestorOf(modalWidget))
    {
        QDialog* dialog = qobject_cast<QDialog*>(modalWidget);

        if (dialog != nullptr)
        {
            dialog->reject();
        }

        event->ignore();
        QTimer::singleShot(0, this, &QWidget::close);
        return;
    }

    // 2. 停止在线重建任务
    bool workersStopped = true;

    auto dialogs = findChildren<fdkui::OnlineReconstructionDialog*>();

    for (auto* dialog : dialogs)
    {
        bool stopped = dialog->Stop();

        if (!stopped)
        {
            workersStopped = false;
        }
    }

    // 3. 检查数据复制线程是否全部结束
    for (const auto& thread : copyThreads_)
    {
        if (thread != nullptr && thread->isRunning())
        {
            workersStopped = false;
        }
    }

    // 4. 如果仍有任务运行，稍后重新尝试关闭
    if (!workersStopped)
    {
        event->ignore();

        QTimer::singleShot(50, this, &QWidget::close);
        return;
    }

    // 5. 停止 SDK
    QString error;

    bool sdkStopped = context_.getSessionManager().Stop(&error);

    if (sdkStopped)
    {
        event->accept();
        return;
    }

    // 6. SDK 尚未成功停止，稍后重试
    event->ignore();

    statusBar()->showMessage(
        QStringLiteral("正在停止 SDK：%1").arg(error)
    );

    QTimer::singleShot(50, this, &QWidget::close);
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
    applyInitialUiState();
}   

void CTViewer::wireConnect() {
    connectTabSignals();
    connectDocumentSignals();
    connectAppSignals();
	connectRenderPanel();
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
    
    for (auto page : ribbonPageRegister_->pages()) {
        tabBar_->addTab(page->tabName());
    }

    tabBar_->setCurrentIndex(TabIndex::File);
}

void CTViewer::setRibbonPage(RibbonPage* page)
{
    if (!page || !stack_ || !ribbonPageRegister_) {
        return;
    }

    stack_->addWidget(page);
    ribbonPageRegister_->add(page);

    connect(page, &RibbonPage::commandRequested, this, [this](const QString& name) {
        context_.getCommands().run(name);
        });
    
}

void CTViewer::buildRibbonStack(QWidget* totalContainer, QVBoxLayout* rootLayout) {

    ribbonPageRegister_ = std::make_unique<RibbonPageRegister>();

    stack_ = new QStackedWidget(totalContainer);    
    stack_->setFixedHeight(iconHeight_);
    rootLayout->addWidget(stack_, 0);

    whatEmpty_ = new QWidget(stack_);
    stack_->addWidget(whatEmpty_);

    for (RibbonPage* page : createAllRibbonPages(stack_)) {
        setRibbonPage(page);
    }
}

void CTViewer::buildContentStack(QWidget* totalContainer, QVBoxLayout* rootLayout) {
    secondstack_ = new QStackedWidget(totalContainer);
    rootLayout->addWidget(secondstack_, 1);

    pageDocument_ = new DocumentPage(secondstack_);
    secondstack_->addWidget(pageDocument_);

	buildWorkspacePage();
	buildEmptyPage();
}

void CTViewer::buildWorkspacePage()
{
    workspacePage_ = new WorkspacePage(secondstack_);
    secondstack_->addWidget(workspacePage_);
    QString err;

    auto& session = context_.getSessionManager();
    const QPointer<ViewportGather> viewport(workspacePage_->getViewportGather());
    session.SetViewVisibility([viewport](const std::string& id) { return viewport && viewport->GetViewVisible(id); });
    connect(viewport, &ViewportGather::refreshRequested, &session, &SessionManager::RequestRender);
    connect(&session, &SessionManager::hostError, this, [this](const QString& message) { statusBar()->showMessage(message, 5000); });
    if (!session.initHost(workspacePage_->getHostConfig(),&err)) statusBar()->showMessage(err);
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
    context_.getCommands().add(QStringLiteral("reconn.open"), [this]() {
        openCtReconUi2();
        });
    context_.getCommands().add(QStringLiteral("image.save"), [this]() {
        showSaveTransformedDataDialog();
        });
    context_.getCommands().add(QStringLiteral("slicestack.save"), [this]() {
        showSaveSliceStackDialog();
        });
    context_.getCommands().add(QStringLiteral("measure.tools.open"), [this]() {
        showMeasureToolsDialog();
        });
    context_.getCommands().add(QStringLiteral("gap.analysis.open"), [this]() {
        showGapAnalysisDialog();
        });
    context_.getCommands().add(QStringLiteral("crop.box"), [this]() {
        startBoxCrop();
        });
    context_.getCommands().add(QStringLiteral("crop.plane"), [this]() {
        startPlaneCrop();
        });
}

void CTViewer::showGapAnalysisDialog()
{
    if (!context_.hasData()) {
        QMessageBox::warning(
            this,
            QStringLiteral("孔隙分析"),
            QStringLiteral("请先加载 RAW 数据或重建结果。"));
        return;
    }

    if (gapAnalysisDialog_) {
        gapAnalysisDialog_->show();
        gapAnalysisDialog_->raise();
        gapAnalysisDialog_->activateWindow();
        return;
    }

    gapAnalysisDialog_ = new GapAnalysisDialog(
        context_.getSessionManager(),
        this);
    gapAnalysisDialog_->show();
}

void CTViewer::showMeasureToolsDialog()
{
    if (!context_.hasData()) {
        QMessageBox::warning(
            this,
            QStringLiteral("二维测量"),
            QStringLiteral("请先加载 RAW 数据或重建结果。"));
        return;
    }

    HostViewTarget sourceTarget;
    sourceTarget.isViewRoleUsed = true;
    sourceTarget.viewRole = HostRenderViewRole::TopDownSlice;

    auto& sessionManager = context_.getSessionManager();
    std::optional<measure::MeasureHostData> hostData;
    std::optional<HostRenderViewState> sourceState;
    for (int attempt = 0; attempt < 2; ++attempt) {
        hostData = sessionManager.getMeasureHostData(sourceTarget);
        sourceState = sessionManager.getRenderViewState(sourceTarget);
        if (hostData
            && hostData->image
            && sourceState
            && hostData->image->data && hostData->image->binding
            && hostData->image->data->self == sourceState->dataRevision
            && hostData->image->binding->revision == sourceState->bindingRevision) {
            break;
        }
        hostData.reset();
        sourceState.reset();
    }
    if (!hostData
        || !hostData->image
        || !hostData->image->image
        || !sourceState) {
        QMessageBox::warning(
            this,
            QStringLiteral("二维测量"),
            QStringLiteral("无法取得当前图像或视图状态。"));
        return;
    }

    measure::MeasurementViewInitState initialState;

    initialState.cursorWorld = sourceState->cursorWorld;
    initialState.windowLevel = std::array<double, 2>{
            sourceState->windowLevel.windowWidth,
            sourceState->windowLevel.windowCenter
    }; 
    initialState.background = std::array<double, 3>{
            sourceState->background.r,
            sourceState->background.g,
            sourceState->background.b
    };
    initialState.visibilityMask = sourceState->visibilityMask;
    initialState.modelMatrix = hostData->modelToWorld;

    measure::MeasureToolDialog dialog(
        hostData->image,
        initialState,
        this);

    dialog.exec();
}

void CTViewer::connectAppSignals()
{
    auto& sessionManager =
        context_.getSessionManager();

    connect(
        &sessionManager,
        &SessionManager::sessionChanged,
        this,
        &CTViewer::handleSessionChanged);

    connect(
        &sessionManager,
        &SessionManager::loadFinished,
        this,
        &CTViewer::handleLoadFinished);

    connect(
        &sessionManager,
        &SessionManager::cropHistoryChanged,
        this,
        &CTViewer::updateCropTree);

    connect(
        &sessionManager,
        &SessionManager::cropBuildFinished,
        this,
        &CTViewer::handleCropBuildFinished);

    auto* sceneTree = workspacePage_->getSceneTreePanel();
    connect(
        sceneTree,
        &SceneTreePanel::cropNodeActivated,
        this,
        [this](qulonglong nodeId) {
            if (context_.getSessionManager().setCropNode(
                    static_cast<CropNodeId>(nodeId))) {
                workspacePage_->getViewportGather()->requestRefresh();
            }
        });
    connect(
        sceneTree,
        &SceneTreePanel::cropKeepInsideRequested,
        this,
        [this]() {
            if (context_.getSessionManager().keepCropInside()) {
                workspacePage_->getViewportGather()->requestRefresh();
            }
        });
    connect(
        sceneTree,
        &SceneTreePanel::cropRemoveInsideRequested,
        this,
        [this]() {
            if (context_.getSessionManager().removeCropInside()) {
                workspacePage_->getViewportGather()->requestRefresh();
            }
        });
    connect(
        sceneTree,
        &SceneTreePanel::cropApplyRequested,
        this,
        [this]() {
			auto& sessionManager = context_.getSessionManager();
            cropSourceVolumeId_ =  context_.getVolumeManager().getActiveVolumeid();
            if (cropSourceVolumeId_ == 0) {
                qDebug() << "没有已经登录的激活体积";
                return;
            }
            cropWindowLevels_ = {};
            for (int i = 0;i < 3; ++i) {
                HostViewTarget target;
                target.isViewRoleUsed = true;
                target.viewRole = SliceRoles[i];
                const auto state = sessionManager.getRenderViewState(target);
                if (state) cropWindowLevels_[i] = state->windowLevel;

            }
            if (!context_.getSessionManager().applyCrop()) {
                cropWindowLevels_ = {};
                qDebug() << "裁切应用未启动。";
            }
        });
    connect(
        sceneTree,
        &SceneTreePanel::cropRestoreOriginalRequested,
        this,
        [this]() {
            if (context_.getSessionManager().restoreOriginalCrop()) {
                workspacePage_->getViewportGather()->requestRefresh();
            }
        });
    connect(
        sceneTree,
        &SceneTreePanel::cropExitRequested,
        this,
        [this]() {
            if (context_.getSessionManager().exitCrop()) {
                workspacePage_->getViewportGather()->requestRefresh();
            }
        });
}

void CTViewer::connectRenderPanel()
{
    auto* renderPanel =
        workspacePage_->getRenderPanel();

    connect(
        renderPanel,
        &RenderPanel::primary3DModeRequested,
        this,
        &CTViewer::set3DMode);

    connect(
        renderPanel,
        &RenderPanel::volumeQualityRequested,//应该判断当前的渲染模式？
        this,
        &CTViewer::setvolumeQuality);

    connect(
        renderPanel,
        &RenderPanel::visibilityRequested,
        this,
        &CTViewer::setVisibility);

    connect(
        renderPanel,
        &RenderPanel::isoValueRequested,
        this,
		&CTViewer::setIsoValue);

    auto& sessionManager = context_.getSessionManager();

	connect(//core -> ui
        &sessionManager,
        &SessionManager::isoStateChanged,
        renderPanel,
        &RenderPanel::setIsoValue);

    connect(
        &sessionManager,
        &SessionManager::isoStateCleared,
        renderPanel,
        &RenderPanel::clearIsoValue);

   /* connect(
        renderPanel,
        &RenderPanel::windowLevelRequested,
        this,
        &CTViewer::setWindowLevel);*/
}

//优化 buildxxx 和 applyxxx分离，build只负责算，apply只负责改界面 
UiState CTViewer::buildUiState(int index) const{
    UiState state;
    state.tabIndex = index;
    const bool hasData = context_.hasData();

    if (index == TabIndex::File) {
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
    state.ribbonPage = ribbonPageRegister_ ? ribbonPageRegister_->pageByTab(index) : nullptr;
	return state;
}

void CTViewer::applyUiState(const UiState& state) {
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

void CTViewer::onOpenRequested(const QString& path, const std::array<int, 3>& dims, const std::array<float,3>& spacing, const std::array<float, 3>& origin)
{
    setCloseProgressDialog();
    setOpenProgressDialog(QStringLiteral("loading"), QStringLiteral("loading"));

    fileHasData_ = true;

    QString err;
    const bool ok =context_.getSessionManager().openData(path,dims,spacing,origin, &err);

    if (!ok) {
       
       setCloseProgressDialog();
       fileHasData_ = false;
       const QString msg = err.isEmpty()
           ? QStringLiteral("无法开始加载，请检查文件路径和参数。")
           : err;
       pageDocument_->notifyFail(msg);
       return;
    }

}

//保存切片
void CTViewer::showSaveSliceStackDialog()
{
    if (!workspacePage_ || !context_.hasData()) {
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
    direction->addItem(QStringLiteral("从上到下(轴向)"), static_cast<int>(HostRenderViewRole::TopDownSlice));
    direction->addItem(QStringLiteral("从前到后(径向)"), static_cast<int>(HostRenderViewRole::FrontBackSlice));
    direction->addItem(QStringLiteral("从左到右(切向)"), static_cast<int>(HostRenderViewRole::LeftRightSlice));

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

        const auto viewRole = static_cast<HostRenderViewRole>(direction->currentData().toInt());
        const auto angleValue = static_cast<double>(angle->value());

        saveButton->setEnabled(false);
		setOpenProgressDialog(QStringLiteral("saving..."), QStringLiteral("saving"));

        QPointer<QDialog> dialogPtr(&dialog);
        QPointer<QLabel> statusPtr(statusLabel);
        QPointer<QPushButton> saveButtonPtr(saveButton);
        QPointer<CTViewer> self(this);

        HostSliceExportRequest request;

        request.outputDir = dir.toUtf8().toStdString();
        request.sourceView.isViewRoleUsed = true;
        request.sourceView.viewRole = viewRole;
        request.angleDeg = angleValue;

        const bool started =
            context_.getSessionManager().sendRequest(
                std::move(request),
                [self, dialogPtr, statusPtr, saveButtonPtr](HostResult result) {
                    if (!self) return;
                    const bool ok = result.isSucceeded;
                    QMetaObject::invokeMethod(
                        self.data(),
                        [self, dialogPtr, statusPtr, saveButtonPtr, ok]() {
                            if (!self) return;
                            self->setCloseProgressDialog();

                            if (statusPtr) {
                                statusPtr->setText(
                                    ok
                                    ? QStringLiteral("保存完成。")
                                    : QStringLiteral("保存失败。"));
                            }

                            if (saveButtonPtr) {
                                saveButtonPtr->setEnabled(true);
                            }

                            if (ok && dialogPtr) {
                                dialogPtr->accept();
                            }
                        },
                        Qt::QueuedConnection);
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
    if (!context_.hasData()) {
        QMessageBox::warning(
            this,
            QStringLiteral("保存图像"),
            QStringLiteral("请先加载数据。"));
        return;
    }

    const QStringList formatNames{
        QStringLiteral("RAW 体数据 (*.raw)"),
        QStringLiteral("PLY 表面模型 (*.ply)"),
        QStringLiteral("STL 表面模型 (*.stl)"),
        QStringLiteral("OBJ 表面模型 (*.obj)")
    };
    constexpr std::array<HostDataExportFormat, 4> exportFormats{
        HostDataExportFormat::Raw,
        HostDataExportFormat::Ply,
        HostDataExportFormat::Stl,
        HostDataExportFormat::Obj
    };

    bool formatAccepted = false;
    const QString selectedFormat = QInputDialog::getItem(
        this,
        QStringLiteral("选择导出格式"),
        QStringLiteral("文件格式："),
        formatNames,
        0,
        false,
        &formatAccepted);
   
    const int i = formatNames.indexOf(selectedFormat);

    const QString dir =
        QFileDialog::getExistingDirectory(
            this,
            QStringLiteral("选择保存目录"));

    if (dir.isEmpty()) {
        return;
    }

    setOpenProgressDialog(
        QStringLiteral("saving..."),
        QStringLiteral("saving"));

    HostDataExportRequest request;

    request.outputPath = dir.toUtf8().toStdString();
    request.format = exportFormats[i];
    request.sourceView.isViewRoleUsed = true;
    request.sourceView.viewRole = HostRenderViewRole::Primary3D;
    QPointer<CTViewer> self(this);

    const bool started =
        context_.getSessionManager().sendRequest(
            std::move(request),
            [self](HostResult result) {
                const bool ok = result.isSucceeded;
                QMetaObject::invokeMethod(
                    self.data(),
                    [self, ok]() {
                        if (!self) {
                            return;
                        }

                        self->setCloseProgressDialog();

                        if (auto* bar = self->statusBar()) {
                            bar->showMessage(
                                ok
                                ? QStringLiteral("保存完成。")
                                : QStringLiteral("保存失败。"),
                                3000);
                        }
                    },
                    Qt::QueuedConnection);
            });

    if (!started) {
        setCloseProgressDialog();

        if (auto* bar = statusBar()) {
            bar->showMessage(
                QStringLiteral("保存任务启动失败。"),
                3000);
        }
    }
}

//重建1
void CTViewer::openCtReconUi()
{
    if (!uiRecon3d_) {
        uiRecon3d_ = new UIReconstruct3D(this);
        uiRecon3d_->setAttribute(Qt::WA_DeleteOnClose,false);
        QObject::connect(uiRecon3d_, &UIReconstruct3D::reconFinished, this, [this]() {
            float* data = nullptr;
            std::array<float, 3> spacing{}, origin{};
            std::array<int, 3> outSize{};

            uiRecon3d_->getReconData(data, spacing, origin, outSize);

            if (!data) {
                if (auto* bar = statusBar()) {
                    bar->showMessage(QStringLiteral("Empty reconstruction result."), 3000);
                }
                return;
            }

            setCloseProgressDialog();
            setOpenProgressDialog(QStringLiteral("loading"), QStringLiteral("loading"));

            if (outSize[0] <= 0
             || outSize[1] <= 0
             || outSize[2] <= 0) 
            {
                return;
            }

            std::size_t voxelCount=1;
            for(const auto dimension:outSize) {
                if(voxelCount>std::numeric_limits<std::size_t>::max()/static_cast<std::size_t>(dimension)) {
                    setCloseProgressDialog();statusBar()->showMessage(QStringLiteral("重建尺寸溢出。"),5000);return;
                }
                voxelCount*=static_cast<std::size_t>(dimension);
            }
            if(voxelCount>static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max())/sizeof(float)) {
                setCloseProgressDialog();statusBar()->showMessage(QStringLiteral("重建体积超过可寻址范围。"),5000);return;
            }
            // 复制期间保留并冻结供应商窗口；裸缓冲仅借用到拥有型 vector 构建完成。
            uiRecon3d_->setEnabled(false);

            auto voxels = std::make_shared<std::vector<float>>();
            QPointer<CTViewer> self(this);

            QThread* copyThread = QThread::create(
                [self, voxels, data, voxelCount, outSize, spacing, origin]() mutable {

                    // 后台线程执行这次大数据复制，避免卡住界面
                    try {
                        voxels->assign(data, data + voxelCount);
                    }
                    catch (const std::exception& e) {
                        if (!self) {
                            return;
                        }

                        const QString message =
                            QStringLiteral("Failed to copy reconstruction data: %1")
                            .arg(QString::fromUtf8(e.what()));

                        QMetaObject::invokeMethod(
                            self.data(),
                            [self, message]() {
                                if (!self) {
                                    return;
                                }

                                self->setCloseProgressDialog();

                                if (auto* bar = self->statusBar()) {
                                    bar->showMessage(message, 5000);
                                }
                            },
                            Qt::QueuedConnection);

                        return;
                    }
                    if (!self) {
                        return;
                    }

                    // 复制完成后，回到 Qt 主线程调用 SessionManager
                    QMetaObject::invokeMethod(
                        self.data(),
                        [self, voxels, outSize, spacing, origin]() mutable {
                            if (!self || self->isClosing_) {
                                return;
                            }

                            QString err;

                            const bool ok =
                                self->context_.getSessionManager().openReconstructedData(
                                    std::move(*voxels),
                                    outSize,
                                    spacing,
                                    origin,
                                    QStringLiteral("CT reconstruction"),
                                    &err);

                            if (!ok) {
                                if (self->ProgressDialog_) {
                                    self->ProgressDialog_->close();
                                    self->ProgressDialog_.clear();
                                }

                                if (auto* bar = self->statusBar()) {
                                    bar->showMessage(
                                        err.isEmpty()
                                        ? QStringLiteral(
                                            "Failed to open reconstruction session.")
                                        : err,
                                        3000);
                                }
                                return;
                            }
                            if (auto* bar = self->statusBar()) {
                                bar->showMessage(
                                    QStringLiteral(
                                        "Reconstruction data submitted to Core."),
                                    3000);
                            }

                            if (self->uiRecon3d_) {
                                self->uiRecon3d_->close();
                            }
                        },
                        Qt::QueuedConnection);
                });

            QObject::connect(
                copyThread,
                &QThread::finished,
                copyThread,
                &QObject::deleteLater);

            copyThreads_.push_back(copyThread);
            connect(copyThread,&QThread::finished,this,[this] {
                copyThreads_.erase(std::remove_if(copyThreads_.begin(),copyThreads_.end(),[](const auto& thread){return !thread||thread->isFinished();}),copyThreads_.end());
                if(uiRecon3d_&&!isClosing_)uiRecon3d_->setEnabled(true);
            });
            copyThread->start();

            }, Qt::QueuedConnection);
    }

    uiRecon3d_->show();
    uiRecon3d_->raise();
    uiRecon3d_->activateWindow();
}

//重建2
void CTViewer::openCtReconUi2()
{
    auto* dialog = new fdkui::OnlineReconstructionDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    connect(
        dialog,
        &fdkui::OnlineReconstructionDialog::reconstructionCompleted,
        this,
        [this](fdkui::ReconstructionResultPtr result) {
            if (!result || !result->voxelsZyx || result->voxelsZyx->empty()) {
                statusBar()->showMessage(
                    QStringLiteral("增量重建未返回有效体数据。"), 3000);
                return;
            }

            const std::array<int, 3> dims{
                static_cast<int>(result->sizeX),
                static_cast<int>(result->sizeY),
                static_cast<int>(result->sizeZ)
            };

            // 该换算的前提：SDK 的 FOV 是完整体积物理长度， center 是该体积中心坐标。下面“物理坐标”一节必须确认。
            const std::array<float, 3> spacing{
                static_cast<float>(result->fovXmm / result->sizeX),
                static_cast<float>(result->fovYmm / result->sizeY),
                static_cast<float>(result->fovZmm / result->sizeZ)
            };
            const std::array<float, 3> origin{
                static_cast<float>(result->centerXmm - result->fovXmm / 2.0),
                static_cast<float>(result->centerYmm - result->fovYmm / 2.0),
                static_cast<float>(result->centerZmm - result->fovZmm / 2.0)
            };

            setOpenProgressDialog(
                QStringLiteral("正在导入增量重建结果"),
                QStringLiteral("加载中"));

            auto copiedVoxels = std::make_shared<std::vector<float>>();
            QPointer<CTViewer> self(this);
            const QString sourcePath = result->configIniPath;

            auto* copyThread = QThread::create(
                [self, result, copiedVoxels, dims, spacing, origin, sourcePath]() {
                    try {
                        *copiedVoxels = *result->voxelsZyx;
                    }
                    catch (const std::exception& e) {
                        QMetaObject::invokeMethod(
                            self.data(),
                            [self, message = QString::fromUtf8(e.what())]() {
                                if (!self) return;
                                self->setCloseProgressDialog();
                                self->statusBar()->showMessage(
                                    QStringLiteral("复制重建结果失败：%1").arg(message),
                                    5000);
                            },
                            Qt::QueuedConnection);
                        return;
                    }

                    QMetaObject::invokeMethod(
                        self.data(),
                        [self, copiedVoxels, dims, spacing, origin, sourcePath]() mutable {
                            if (!self || self->isClosing_) return;

                            QString error;
                            const bool ok =
                                self->context_.getSessionManager().openReconstructedData(
                                    std::move(*copiedVoxels),
                                    dims,
                                    spacing,
                                    origin,
                                    sourcePath,
                                    &error);

                            if (!ok) {
                                self->setCloseProgressDialog();
                                self->statusBar()->showMessage(
                                    error.isEmpty()
                                    ? QStringLiteral("增量重建结果导入失败。")
                                    : error,
                                    5000);
                            }
                        },
                        Qt::QueuedConnection);
                });

            connect(copyThread, &QThread::finished,
                copyThread, &QObject::deleteLater);
            copyThreads_.push_back(copyThread);
            connect(copyThread,&QThread::finished,this,[this] {
                copyThreads_.erase(std::remove_if(copyThreads_.begin(),copyThreads_.end(),[](const auto& thread){return !thread||thread->isFinished();}),copyThreads_.end());
            });
            copyThread->start();
        });

    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}

void CTViewer::set3DMode(
    HostRenderMode mode,
    HostVisibilityParams visibility)
{
    if (!context_.hasData()) {
        return;
    }

    HostViewSetRequest request;
    request.targetView.isViewRoleUsed = true;
    request.targetView.viewRole = HostRenderViewRole::Primary3D;
    request.mode = mode;
	request.visibility = std::move(visibility);
    context_.getSessionManager().sendRequest(std::move(request));
    auto* ptr = workspacePage_->getViewportGather();
    ptr->requestRefresh();
}

void CTViewer::setvolumeQuality(HostVolumeQuality quality)
{
    if (!context_.hasData()) {
        return;
    }
	HostViewSetRequest request;
    request.targetView.isViewRoleUsed = true;
    request.targetView.viewRole = HostRenderViewRole::Primary3D;
    request.volumeQuality = quality;
    context_.getSessionManager().sendRequest(std::move(request));
    auto* ptr = workspacePage_->getViewportGather();
    ptr->requestRefresh();
}

void CTViewer::setVisibility(
    HostVisibilityParams visibility)
{
    if (!context_.hasData()) {
        return;
    }

    auto& sessionManager = context_.getSessionManager();
    bool isAccepted = true;

    HostVisibilityParams primaryVisibility;
    primaryVisibility.isPlanes3DVisible =visibility.isPlanes3DVisible;
    primaryVisibility.isRulerVisible =visibility.isRulerVisible;

    HostViewSetRequest request;
        request.targetView = {
            "", true, HostRenderViewRole::Primary3D };
        if (primaryVisibility.isPlanes3DVisible || primaryVisibility.isRulerVisible) {
            request.visibility = std::move(primaryVisibility);
            const bool isViewAccepted = sessionManager.sendRequest(std::move(request));
            isAccepted = isViewAccepted && isAccepted;
        }
 

    if (visibility.isCrosshairVisible.has_value()) {
        constexpr std::array<HostRenderViewRole, 3> sliceRoles{
            HostRenderViewRole::TopDownSlice,
            HostRenderViewRole::FrontBackSlice,
            HostRenderViewRole::LeftRightSlice
        };
        for (const auto role : sliceRoles) {
            HostVisibilityParams sliceVisibility;
            sliceVisibility.isCrosshairVisible =
                visibility.isCrosshairVisible;
            HostViewSetRequest request;
            request.targetView = { "", true, role };
            request.visibility = std::move(sliceVisibility);
            const bool isViewAccepted = sessionManager.sendRequest(std::move(request));
            isAccepted = isViewAccepted && isAccepted;
        }
    }

    if (!isAccepted) {
        statusBar()->showMessage(
            QStringLiteral("显示状态更新失败。"),
            3000);
    }
    workspacePage_->getViewportGather()->requestRefresh();
}

void CTViewer::setIsoValue(double isoValue)//ui的value ->core
{
    if (!context_.hasData() || !std::isfinite(isoValue)) {
        return;
    }

    HostViewSetRequest request;

    request.targetView.isViewRoleUsed = true;
    request.targetView.viewRole = HostRenderViewRole::Primary3D;
    request.iso = isoValue;
    const bool started = context_.getSessionManager().sendRequest(std::move(request));

    if (!started) 
    {
        return;
    }

    workspacePage_ ->getViewportGather() ->requestRefresh();
}

void CTViewer::startBoxCrop()
{
    if (!context_.hasData()) {
        statusBar()->showMessage(
            QStringLiteral("请先加载数据。"),
            3000);
        return;
    }

    if (!context_.getSessionManager().startBoxCrop()) {
        statusBar()->showMessage(
            QStringLiteral("框裁切未启动。"),
            3000);
        return;
    }

    workspacePage_->getViewportGather()->requestRefresh();
}

void CTViewer::startPlaneCrop()
{
    if (!context_.hasData()) {
        statusBar()->showMessage(
            QStringLiteral("请先加载数据。"),
            3000);
        return;
    }

    if (!context_.getSessionManager().startPlaneCrop()) {
        statusBar()->showMessage(
            QStringLiteral("平面裁切未启动。"),
            3000);
        return;
    }

    workspacePage_->getViewportGather()->requestRefresh();
}

void CTViewer::updateCropTree()
{
    if (!workspacePage_) {
        return;
    }

    workspacePage_->getSceneTreePanel()->setCropTreeState(context_.getSessionManager().getCropTreeState());
}

void CTViewer::handleCropBuildFinished(
    bool isSuccess,
    QString message)
{
    if (!isSuccess) {
        return;
    }

    const int sourceVolumeId = cropSourceVolumeId_;
    cropSourceVolumeId_ = 0;
    QString captureError;
    const auto captured = context_.getSessionManager().capturedVolumeSnapshot(&captureError);

    if (!captured) {
        qDebug() << "[VolumeCapture] B failed:" << captureError;
    }
    else {
        croppedSnapshot_ = captured;
        const auto& b = croppedSnapshot_;
        qDebug() << "[VolumeCapture] B saved"
            << "sourceVolumeId=" << sourceVolumeId
            << "version=" << b->binding->revision
            << "dims=" << b->image->GetDimensions()[0] << b->image->GetDimensions()[1] << b->image->GetDimensions()[2]
            << "hasMask=" << (b->validityMask != nullptr)
            << "image=" << static_cast<const void*>(b->image.GetPointer());
    }

	auto& sessionManager = context_.getSessionManager();

    for (int i = 0; i < 3; ++i)
    {
		const auto role = SliceRoles[i];
		HostViewResetRequest req;
        req.targetView.isViewRoleUsed = true;
        req.targetView.viewRole = role;
        sessionManager.sendRequest(std::move(req));     

        HostViewSetRequest setRequest;
        setRequest.targetView.isViewRoleUsed = true;
        setRequest.targetView.viewRole = role;
        setRequest.windowLevel = cropWindowLevels_[i];
        sessionManager.sendRequest(std::move(setRequest));
    }

    cropWindowLevels_ = {};

    workspacePage_->getViewportGather()->requestRefresh();
}

void CTViewer::switchVolumeForTest(bool useOriginal)
{
    if (volumePublishTestBusy_) return;
    if (!originalSnapshot_ || !croppedSnapshot_) {
        QMessageBox::warning(this, QStringLiteral("体积切换测试"),
            QStringLiteral("请先加载 A 并成功应用一次裁切，确认 A/B 都已保存。"));
        return;
    }
    const auto archive = useOriginal ? originalSnapshot_ : croppedSnapshot_;
    const QString name = useOriginal ? QStringLiteral("A") : QStringLiteral("B");
    volumePublishTestBusy_ = true;
    setEnabled(false);
    auto expectedRevision = std::make_shared<DataRevisionRef>();
    auto expectedBinding = std::make_shared<DataBindingRevision>(0);
    auto attempts = std::make_shared<int>(0);
    auto activationResult = std::make_shared<std::optional<HostResult>>();
    auto activationCompletedAt = std::make_shared<qint64>(-1);
    auto renderTargets = std::make_shared<std::map<std::string, std::uint64_t>>();
    auto deadline = std::make_shared<QElapsedTimer>();
    deadline->start();
    auto* timer = new QTimer(this);
    timer->setInterval(50);
    connect(timer, &QTimer::timeout, this,
        [this, timer, attempts, expectedRevision, expectedBinding, activationResult, activationCompletedAt, renderTargets, deadline, archive, name]() {
            if (isClosing_) { timer->stop(); timer->deleteLater(); volumePublishTestBusy_ = false; return; }
            QString report;
            auto& session = context_.getSessionManager();
            // 沿用 CT 加载清理的 30 秒绝对保护；异步准备不再当成 60 次绘制核验。
            const bool preparationTimedOut = activationResult->has_value()
                ? *activationCompletedAt >= 30000 : deadline->elapsed() >= 30000;
            if (preparationTimedOut) {
                timer->stop(); timer->deleteLater();
                volumePublishTestBusy_ = false; setEnabled(true);
                // 停止旧 Host 的准备任务；失败时 resetHost 保留可重试的旧 Host/账本。
                session.clearSession();
                QMessageBox::warning(this, QStringLiteral("视图同步未通过"),
                    session.getState() == SessionManager::State::Empty
                    ? QStringLiteral("显示激活超时，SDK 工作已取消，请重新加载后重试。")
                    : QStringLiteral("显示激活超时，SDK 停止仍未完成；已保留清理责任，请关闭后重试。"));
                return;
            }
            if (!*expectedBinding) {
                ++(*attempts);
                bool isPending = false;
                const auto published = session.publishVolumeSnapshotForTest(archive, &report, &isPending);
                if (!published && isPending && *attempts < 60) return;
                if (!published) {
                    timer->stop(); timer->deleteLater();
                    volumePublishTestBusy_ = false; setEnabled(true);
                    if (isPending) session.failVolumePublishTest(report);
                    QMessageBox::warning(this, QStringLiteral("发布测试未完成"), report);
                    return;
                }
                qDebug() << "[VolumePublish] data published" << name
                    << "archiveVersion=" << archive->binding->revision
                    << "publishedVersion=" << published->binding->revision
                    << "hasMask=" << (published->validityMask != nullptr);
                *expectedRevision = published->data->self;
                // 完整显示组激活使用当前 binding 的 CAS，并在提交时推进一次版本。
                const auto binding = published->binding->revision;
                if (binding == std::numeric_limits<DataBindingRevision>::max()) {
                    timer->stop(); timer->deleteLater(); volumePublishTestBusy_ = false; setEnabled(true);
                    QMessageBox::warning(this, QStringLiteral("发布测试未完成"), QStringLiteral("数据绑定版本已耗尽。"));
                    return;
                }
                *expectedBinding = binding + 1;
                *attempts = 0;
                HostLoadActivationRequest activate;
                activate.dataRevision = *expectedRevision;
                activate.expectedBindingRevision = binding;
                QPointer<CTViewer> self(this);
                QPointer<QTimer> pendingTimer(timer);
                // 同步返回仅表示接纳，拒绝不回调；接纳后在 owner 线程恰好完成一次，
                // 可能在 sendRequest 返回前执行。只查询已提交场景并冻结目标 epoch，
                // 不发送请求/绘制，不持有 VTK 租约。
                const bool accepted = session.sendRequest(std::move(activate),
                    [self, pendingTimer, activationResult, activationCompletedAt, deadline, renderTargets, expectedRevision, expectedBinding](HostResult result) {
                        if (!self || self->isClosing_ || !pendingTimer || !pendingTimer->isActive()) return;
                        *activationCompletedAt = deadline->elapsed();
                        if (result.isSucceeded) {
                            QString report;
                            (void)self->context_.getSessionManager().areAllRenderViewsAtVersion(
                                *expectedRevision, *expectedBinding, *renderTargets, &report);
                            if (renderTargets->size() != 4) {
                                result.isSucceeded = false;
                                result.message = report.toStdString();
                            }
                        }
                        *activationResult = std::move(result);
                    });
                if (!self || self->isClosing_) return;
                if (!accepted) {
                    timer->stop(); timer->deleteLater(); volumePublishTestBusy_ = false; setEnabled(true);
                    QMessageBox::warning(this, QStringLiteral("显示激活未接纳"), QStringLiteral("数据已发布，SDK 显示激活请求被拒绝；保留资源以便退出或重试。"));
                    return;
                }
                workspacePage_->getViewportGather()->requestRefresh();
                return;
            }
            if (!activationResult->has_value()) return;
            if (!activationResult->value().isSucceeded) {
                timer->stop(); timer->deleteLater(); volumePublishTestBusy_ = false; setEnabled(true);
                QMessageBox::warning(this, QStringLiteral("显示激活未完成"),
                    QStringLiteral("数据已发布，显示激活失败或取消：%1").arg(QString::fromStdString(activationResult->value().message)));
                return;
            }
            ++(*attempts);
            const bool synchronized = session.areAllRenderViewsAtVersion(
                *expectedRevision, *expectedBinding, *renderTargets, &report);
            if (!synchronized && *attempts < 60) return;
            timer->stop(); timer->deleteLater();
            volumePublishTestBusy_ = false; setEnabled(true);
            if (synchronized) {
                qDebug() << "[VolumePublish] four views synchronized" << name << report;
                statusBar()->showMessage(
                    QStringLiteral("已发布 %1，四视图版本一致；请继续检查图像和 mask。")
                    .arg(name), 5000);
            }
            else {
                const QString reason = QStringLiteral(
                    "等待四视图同步超时，期望绑定版本 %1；实际：%2。请重新加载后测试。")
                    .arg(static_cast<qulonglong>(*expectedBinding)).arg(report);
                session.failVolumePublishTest(reason);
                QMessageBox::warning(this, QStringLiteral("视图同步未通过"), reason);
            }
        });
    timer->start();
}

//void CTViewer::setWindowLevel(
//    HostWindowLevelParams windowLevel)
//{
//    if (!context_.hasData()) {
//        return;
//    }
//
//    HostViewSetRequest request;
//    request.targetView.isViewRoleUsed = true;
//    request.targetView.viewRole = HostRenderViewRole::Primary3D;
//    request.windowLevel = windowLevel;
//
//    context_.getSessionManager().sendRequest(
//        std::move(request));
//
//    workspacePage_->getViewportGather()->requestRefresh();
//}

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

void CTViewer::handleSessionChanged(
    SessionManager::State state)
{
    if (workspacePage_) {
        workspacePage_->setDataState(
            state == SessionManager::State::Ready,
            context_.getSessionManager().getSourcePath());
        updateCropTree();
    }

    if (!tabBar_) {
        return;
    }

    applyUiState(buildUiState(tabBar_->currentIndex()));
}

void CTViewer::handleLoadFinished(
    bool issucc,
    QString message)
{
    setCloseProgressDialog();
    bool isDocuOpen = fileHasData_;

    if (!issucc) {
        if (isDocuOpen) {
			pageDocument_->notifyFail(message.isEmpty() ? QStringLiteral("加载失败，请检查输入参数。") : message);
        }
        return;
    }

    auto& volumes = context_.getVolumeManager();
    volumes.setActiveVolumeid(0);
    originVolumeId_ = 0;
    latestCroppedVolumeId_ = 0;
    cropSourceVolumeId_ = 0;
    originalSnapshot_.reset();
    croppedSnapshot_.reset();
    QString err;
    originalSnapshot_ = context_.getSessionManager().capturedVolumeSnapshot(&err);

    if (!originalSnapshot_) {
        qWarning() << "[volumeCaptured] a failed" << err;
    }
    else {
        // 登记同一份独立归档，不再捕获或复制一份体积。
        originVolumeId_ = volumes.addVolume(QStringLiteral("原始体积"), originalSnapshot_);
        volumes.setActiveVolumeid(originVolumeId_);
        const auto& a = originalSnapshot_;
        qDebug() << "[VolumeCaptured] A saved"
            << "version=" << a->binding->revision
            << "dims=" << a->image->GetDimensions()[0] << a->image->GetDimensions()[1] << a->image->GetDimensions()[2]
            << "hasMask=" << (a->validityMask != nullptr)
            << "image=" << static_cast<void*>(a->image.GetPointer());
    }

    if (isDocuOpen) {
        pageDocument_->closeOpenDialog();
    }

    HostVisibilityParams p;
    p.isCrosshairVisible = false;
    setVisibility(std::move(p));
    setvolumeQuality(HostVolumeQuality::Low);
    
    auto* viewport = workspacePage_->getViewportGather();
    viewport->requestRefresh();

    if (tabBar_->currentIndex() == TabIndex::File)
    {
        tabBar_->setCurrentIndex(TabIndex::Start);
        return;
    }
}
