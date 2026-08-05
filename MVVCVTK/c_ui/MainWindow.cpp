#include "c_ui/context/SessionManager.h"
#include "c_ui/contextarea/WorkspacePage.h"
#include "c_ui/MainWindow.h"
#include "c_ui/panels/RenderPanel.h"
#include "c_ui/panels/SceneTreePanel.h"
#include "c_ui/ribbon/RibbonPage.h"
#include "c_ui/windows/Titlebar.h"
#include "c_ui/workbenches/DocumentPage.h"
#include "Host/Types/HostRequestTypes.h"
#include "uireconstruct3d.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <memory>

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
    workspacePage_  = new WorkspacePage(secondstack_);

    secondstack_->addWidget(workspacePage_);

    

    QString err;

    context_.getSessionManager().initHost(
            workspacePage_->getHostConfig(),
            &err);

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
    context_.getCommands().add(QStringLiteral("measure.tools.open"), [this]() {
        showMeasureToolsDialog();
        });
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

    QMessageBox::information(
        this,
        QStringLiteral("二维测量"),
        QStringLiteral(
            "二维测量正在迁移到新版 Host 接口，"
            "当前版本暂不可用。"));
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
}

void CTViewer::connectRenderPanel()
{
    auto* renderPanel =
        workspacePage_->getRenderPanel();

    connect(
        renderPanel,
        &RenderPanel::primary3DModeRequested,
        this,
        &CTViewer::setPrimary3DMode);

    connect(
        renderPanel,
        &RenderPanel::visibilityRequested,
        this,
        &CTViewer::setVisibility);
}

//优化 buildxxx 和 applyxxx分离，build只负责算，apply只负责改界面 
UiState CTViewer::buildUiState(int index) const{
    UiState state;
    state.tabIndex = index;

    //业务集中在一个地方
    const bool hasData = context_.hasData();
    //文件页
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
    const bool ok =context_.getSessionManager().openFile(path,spacing,origin, &err);

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

        HostSliceExportRequest request;

        request.outputDir = dir.toUtf8().toStdString();
        request.sourceView.isViewRoleUsed = true;
        request.sourceView.viewRole = viewRole;
        request.angleDeg = angleValue;

        const bool started =
            context_.getSessionManager().sendRequest(
                std::move(request),
                [this, dialogPtr, statusPtr, saveButtonPtr](bool ok) {
                    QMetaObject::invokeMethod(
                        qApp,
                        [this, dialogPtr, statusPtr, saveButtonPtr, ok]() {
                            setCloseProgressDialog();

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
    request.format = HostDataExportFormat::Raw;
    request.sourceView.isViewRoleUsed = true;
    request.sourceView.viewRole = HostRenderViewRole::Primary3D;
    QPointer<CTViewer> self(this);

    const bool started =
        context_.getSessionManager().sendRequest(
            std::move(request),
            [self](bool ok) {
                if (!self) {
                    return;
                }

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

//重建
void CTViewer::openCtReconUi()
{
    if (!uiRecon3d_) {
        uiRecon3d_ = new UIReconstruct3D(this);
        QObject::connect(uiRecon3d_, &UIReconstruct3D::reconFinished, this, [this]() {//重建完成是我点击？还是什么情况
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

            const std::size_t voxelCount =
                static_cast<std::size_t>(outSize[0])
                * static_cast<std::size_t>(outSize[1])
                * static_cast<std::size_t>(outSize[2]);

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
                            if (!self) {
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

            copyThread->start();

            }, Qt::QueuedConnection);
    }

    uiRecon3d_->show();
    uiRecon3d_->raise();
    uiRecon3d_->activateWindow();
}

void CTViewer::setPrimary3DMode(
    HostRenderMode mode)
{
    if (!context_.hasData()) {
        return;
    }

    HostViewSetRequest request;
    request.targetView.isViewRoleUsed = true;
    request.targetView.viewRole =
        HostRenderViewRole::Primary3D;
    request.mode = mode;
    
    context_.getSessionManager().sendRequest(
            std::move(request));
}

void CTViewer::setVisibility(
    HostVisibilityParams visibility)
{
    if (!context_.hasData()) {
        return;
    }

    HostViewSetRequest request;

    request.targetView.isViewRoleUsed = true;
    request.targetView.viewRole =
        HostRenderViewRole::Primary3D;

    request.visibility =
        std::move(visibility);

    const bool started =
        context_.getSessionManager().sendRequest(
            std::move(request));

    if (!started) {
        statusBar()->showMessage(
            QStringLiteral("修改显示状态失败。"),
            3000);
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

void CTViewer::handleSessionChanged(
    SessionManager::State state)
{
    if (workspacePage_) {
        workspacePage_->setDataState(
            state == SessionManager::State::Ready,
            context_.getSessionManager().getSourcePath());
    }

    if (!tabBar_) {
        return;
    }

    applyUiState(
        buildUiState(tabBar_->currentIndex()));
}

void CTViewer::handleLoadFinished(
    bool issucc,
    QString message)
{
    setCloseProgressDialog();

    if (!issucc) {
        const QString errorMessage =
            message.isEmpty()
            ? QStringLiteral("加载失败。")
            : message;

        if (pageDocument_) {
            pageDocument_->notifyFail(errorMessage);
        }

        statusBar()->showMessage(
            errorMessage,
            3000);

        if (tabBar_) {
            applyUiState(
                buildUiState(tabBar_->currentIndex()));
        }

        return;
    }

    if (pageDocument_) {
        pageDocument_->notifySucc();
    }

    if (tabBar_
        && tabBar_->currentIndex() == TabIndex::File) {
        tabBar_->setCurrentIndex(TabIndex::Start);
        return;
    }

    if (tabBar_) {
        applyUiState(
            buildUiState(tabBar_->currentIndex()));
    }
}
