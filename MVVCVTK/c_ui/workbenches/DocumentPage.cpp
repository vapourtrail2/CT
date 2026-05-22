#include "DocumentPage.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QHeaderView>
#include <QAbstractItemView>
#include <QTableWidgetItem>
#include <QListWidget>
#include <QListWidgetItem>
#include <QFileDialog>
#include <QDialog>
#include <QStackedWidget>
#include <QDoubleSpinBox>
#include <QFormLayout>

DocumentPage::DocumentPage(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
    wireLeftDockSignals();
}

//构建整页：左右分栏（左：导航，右：主内容）
void DocumentPage::buildUi()
{
    setObjectName(QStringLiteral("pageDocument"));
    setStyleSheet(QStringLiteral(
        "QWidget#pageDocument{background-color:#404040;}"
        "QLabel{color:#f2f2f2;}"));

    auto hl = new QHBoxLayout(this);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(0);

    // 左侧Dock区
    buildLeftDock();
	hl->addWidget(listNav_, 0); //第二个参数的意思是伸缩比例，0表示不伸缩

    // 右侧主内容
    auto right = buildRightContent(this);
    hl->addWidget(right, 1);
}

 //构建左侧导航列表
void DocumentPage::buildLeftDock()
{
	listNav_ = new QListWidget(this);//listNav_代表的是左侧的导航列表
    listNav_->setMinimumWidth(180);
    listNav_->setStyleSheet(R"(
        QListWidget { background:#181818; color:#ddd; border:none; }
        QListWidget::item { height:30px; padding-left:8px; }
        QListWidget::item:selected { background:#444; color:#fff; }
    )");

    // 工具函数:分割线
    auto addSeparator = [this]() {
        auto sep = new QListWidgetItem();//占位
        sep->setFlags(Qt::NoItemFlags);//这个项只能用来占位或展示，不允许用户交互
        sep->setSizeHint(QSize(180, 10));
        listNav_->addItem(sep);

        auto line = new QFrame(listNav_);
        line->setFrameShape(QFrame::HLine);
        line->setStyleSheet("background:#444; margin:4px 12px;");
        listNav_->setItemWidget(sep, line);
    };

    auto addItem = [this](const QString& text) {
        auto item = new QListWidgetItem(text);
        item->setSizeHint(QSize(180, 30));
        listNav_->addItem(item);
    };

    addItem(QStringLiteral("新建"));
    addItem(QStringLiteral("打开"));
    addItem(QStringLiteral("保存"));
    addItem(QStringLiteral("另存为"));
    addItem(QStringLiteral("打包"));
    addItem(QStringLiteral("导出为"));
    addSeparator();

    addItem(QStringLiteral("快速导入"));
    //addItem(QStringLiteral("CT重建"));
    addItem(QStringLiteral("导入"));
    addItem(QStringLiteral("导出"));
    addSeparator();

    addItem(QStringLiteral("合并对象"));
    addItem(QStringLiteral("保存对象"));
    addItem(QStringLiteral("保存图像"));
    addItem(QStringLiteral("保存影像/图像堆栈"));
	addItem(QStringLiteral("保存报告"));
	addItem(QStringLiteral("批处理"));
	addItem(QStringLiteral("首选项"));
}

//  构建右侧主内容
QWidget* DocumentPage::buildRightContent(QWidget* parent)
{
    auto right = new QWidget(parent);
    auto vl = new QVBoxLayout(right);
    vl->setContentsMargins(18, 18, 18, 18);
    vl->setSpacing(16);

    // 顶部横幅
    auto banner = new QFrame(right);
    banner->setObjectName(QStringLiteral("heroBanner"));
    banner->setStyleSheet(QStringLiteral(
        "QFrame#heroBanner{background:#322F30; border-radius:10px;}"
        "QFrame#heroBanner QLabel{color:#f9f9f9;}"));
    auto bannerLayout = new QVBoxLayout(banner);
    bannerLayout->setContentsMargins(20, 16, 20, 16);
    bannerLayout->setSpacing(8);
    auto title = new QLabel(QStringLiteral("欢迎使用 GviewCT"), banner);
    title->setStyleSheet(QStringLiteral("font-size:24px; font-weight:700;"));
    bannerLayout->addWidget(title);

    vl->addWidget(banner);

    // 操作提示
    auto tipsFrame = new QFrame(right);
    tipsFrame->setObjectName(QStringLiteral("tipsFrame"));
    tipsFrame->setStyleSheet(QStringLiteral(
        "QFrame#tipsFrame{background:#322F30; border-radius:10px;}"
        "QFrame#tipsFrame QLabel{color:#d8d8d8;}"));
    tipsFrame->setFixedHeight(92);
    auto tipsLayout = new QVBoxLayout(tipsFrame);
    tipsLayout->setContentsMargins(20, 18, 20, 18);
    tipsLayout->setSpacing(10);
    auto tipsTitle = new QLabel(QStringLiteral("操作提示"), tipsFrame);
    tipsTitle->setStyleSheet(QStringLiteral("font-size:16px; font-weight:600;"));
    tipsLayout->addWidget(tipsTitle);
    auto tips = new QLabel(QStringLiteral(
        "1.可导入 RAW 等常见工业 CT 数据。\n"
        "2.三维重建。"), tipsFrame);
    tips->setWordWrap(true);
    tips->setStyleSheet(QStringLiteral("font-size:13px; line-height:20px;"));
    tipsLayout->addWidget(tips);
    tipsLayout->addStretch();
    vl->addWidget(tipsFrame);

    vl->addStretch();
    return right;
}

 // 连接左侧栏点击
void DocumentPage::wireLeftDockSignals(){
    if (!listNav_)
        return;
    connect(listNav_, &QListWidget::itemClicked, this,
        [this](QListWidgetItem* it) {
            const QString t = it ? it->text() : QString();
            if (t == QStringLiteral("打开")){
                emit moduleClicked(QStringLiteral("选择：%1").arg(t));
                showOpenDialog();
            }
            else if (t == QStringLiteral("CT重建")
                  || t == QStringLiteral("快速导入")
                  || t == QStringLiteral("导入")
                  || t == QStringLiteral("导出")) {
                emit moduleClicked(QStringLiteral("正在进入：%1").arg(t));
            }
            else if (t == QStringLiteral("退出")) {
                emit moduleClicked(QStringLiteral("准备退出"));
            }
            else {
                emit moduleClicked(QStringLiteral("选择：%1").arg(t));
            }
        });
}

void DocumentPage::showOpenDialog()
{
    if (!docDialog_) {
        docDialog_ = new QDialog(this);
		docDialog_->setFixedSize(460, 260);
        docDialog_->setModal(true);
        docDialog_->setWindowTitle(QStringLiteral("打开"));
       
        auto* dialogLayout = new QVBoxLayout(docDialog_);
        dialogLayout->setContentsMargins(14, 14, 14, 14);
        dialogLayout->setSpacing(12);

        //importStack
		importStack_ = new QStackedWidget(docDialog_);
		dialogLayout->addWidget(importStack_);

        //第一页 选择文件
		auto* filePage = new QWidget(docDialog_);
		auto* fileLayout = new QVBoxLayout(filePage);
		fileLayout->setContentsMargins(0, 0, 0, 0);
		fileLayout->setSpacing(12);

        auto* introLabel = new QLabel(QStringLiteral("请选择RAW文件，然后点击“下一步”。"), filePage);
        introLabel->setWordWrap(true);
        fileLayout->addWidget(introLabel);

        // 输入与按钮区域
        auto* inputRow = new QHBoxLayout();
        inputRow->setSpacing(8);
        auto* dirLabel = new QLabel(QStringLiteral("文件:"), filePage);
        inputRow->addWidget(dirLabel);

        inputPath_ = new QLineEdit(filePage);
        inputPath_->setPlaceholderText(QStringLiteral("选择或输入 RAW 文件"));
		inputRow->addWidget(inputPath_, 1);

        btnBrowse_ = new QPushButton(QStringLiteral("浏览..."), filePage);
        inputRow->addWidget(btnBrowse_);

        fileLayout->addLayout(inputRow);
		fileLayout->addStretch();

		importStack_->addWidget(filePage);

		//第二页 填写信息
		auto* infoPage = new QWidget(docDialog_);
		auto* infoLayout = new QFormLayout(infoPage);//QFormLayout用于标签和输入的配对显示
		infoLayout->setContentsMargins(0, 0, 0, 0);
		infoLayout->setSpacing(12);

        auto makeNum_1 = [this]() {
            auto* box = new QDoubleSpinBox(docDialog_);
            box->setDecimals(3);
            box->setRange(0.000001, 100000.0);
            box->setSingleStep(0.1);
            box->setSuffix(QStringLiteral("mm"));   
            return box;
        };

        auto makeNum_2 = [this]() {
            auto* box = new QDoubleSpinBox(docDialog_);
            box->setDecimals(2);
            box->setRange(-100000.0, 100000.0);
            box->setSingleStep(0.1);
            /*box->setSuffix(QStringLiteral("mm"));*/
            return box;
            };

		spacingX_ = makeNum_1();
		spacingY_ = makeNum_1();
		spacingZ_ = makeNum_1();

		spacingX_->setValue(0.02125);
		spacingY_->setValue(0.02125);
		spacingZ_->setValue(0.02125);

        originX_ = makeNum_2();
        originY_ = makeNum_2();
        originZ_ = makeNum_2();

        originX_->setValue(0.0);
        originY_->setValue(0.0);
        originZ_->setValue(0.0);

		infoLayout->addRow(QStringLiteral("像素间距X:"), spacingX_);
		infoLayout->addRow(QStringLiteral("像素间距Y:"), spacingY_);
		infoLayout->addRow(QStringLiteral("像素间距Z:"), spacingZ_);
		infoLayout->addRow(QStringLiteral("原点X:"), originX_);
		infoLayout->addRow(QStringLiteral("原点Y:"), originY_);
		infoLayout->addRow(QStringLiteral("原点Z:"), originZ_);

        importStack_->addWidget(infoPage);

        
		// 底部状态与操作按钮
        auto* actionRow = new QHBoxLayout();
        actionRow->setSpacing(8);
        dicomStatusLabel_ = new QLabel(QStringLiteral("尚未加载数据"), docDialog_);
        dicomStatusLabel_->setStyleSheet(QStringLiteral("color:#d0d0d0;"));
        actionRow->addWidget(dicomStatusLabel_, 1);

        btnBack_ = new QPushButton(QStringLiteral("上一步"), docDialog_);
        btnBack_->setVisible(false);
		actionRow->addWidget(btnBack_);

		btnNext_ = new QPushButton(QStringLiteral("下一步"), docDialog_);
		actionRow->addWidget(btnNext_);

        btnLoad_ = new QPushButton(QStringLiteral("加载"), docDialog_);
        btnLoad_->setDefault(true);
        actionRow->addWidget(btnLoad_);
        dialogLayout->addLayout(actionRow);

        connect(btnBrowse_, &QPushButton::clicked, this, [this]() {
            const QString filePath = QFileDialog::getOpenFileName(//读文件
                this,
                QStringLiteral("选择数据文件"),
                "",
                QStringLiteral("Raw Data (*.raw);;All Files (*.*)")
            );

            if (!filePath.isEmpty()) {
                inputPath_->setText(filePath); // 显示路径
            }
            });

        connect(btnNext_, &QPushButton::clicked, this, [this]() {
			const QString path = inputPath_->text().trimmed();
            if (path.isEmpty()) {
                updateStatusLabel(QStringLiteral("路径为空，请选择文件。"), true);
                return;
            }

		    importStack_->setCurrentIndex(1);
            btnBack_->setVisible(true);
			btnNext_->setVisible(false);
            btnLoad_->setVisible(true);
            updateStatusLabel(QStringLiteral("请填写分辨率和原点。"), false);
            });

        connect(btnBack_, &QPushButton::clicked, this, [this]() {
            importStack_->setCurrentIndex(0);
            btnBack_->setVisible(false);
            btnNext_->setVisible(true);
            btnLoad_->setVisible(false);
            updateStatusLabel(QStringLiteral("请选择 RAW 文件"), false);
            });


        // 加载按钮连接
        connect(btnLoad_, &QPushButton::clicked, this, [this]() {//connect该怎么传
            loadFilePath(inputPath_->text().trimmed());
            });
    }

    importStack_->setCurrentIndex(0);
    btnBack_->setVisible(false);
    btnNext_->setVisible(true);
    btnLoad_->setVisible(false);
    
    // 每次展示前重置状态文案
    updateStatusLabel(QStringLiteral("尚未加载数据"), false);
    docDialog_->show();
    docDialog_->raise();
    docDialog_->activateWindow();
}

void DocumentPage::loadFilePath(const QString& path)
{
    const QString p = path.trimmed();
    if (p.isEmpty()) {
        updateStatusLabel(QStringLiteral("路径为空，请选择文件。"), true);
        return;
    }

    std::array<float, 3> spacing{
        static_cast<float>(spacingX_->value()),
        static_cast<float>(spacingY_->value()),
        static_cast<float>(spacingZ_->value())
    };

    std::array<float, 3> origin{
        static_cast<float>(originX_->value()),
        static_cast<float>(originY_->value()),
        static_cast<float>(originZ_->value())
    };


    updateStatusLabel(QStringLiteral("正在加载..."), false);
    emit openRequested(p,spacing,origin);   
}

void DocumentPage::closeOpenDialog()
{
    if (docDialog_) {
        docDialog_->accept();
    }
}

void DocumentPage::notifySucc()
{
    updateStatusLabel(QStringLiteral("数据加载成功。"), false);
    if (docDialog_) {
		docDialog_->accept();
    }
}

void DocumentPage::notifyFail(const QString& reason)
{
    const QString msg = reason.isEmpty() ? QStringLiteral("加载失败") : reason;
	updateStatusLabel(msg, true);
}

// 统一更新状态提示，带上错误标记
void DocumentPage::updateStatusLabel(const QString& text, bool isError)
{
    if (!dicomStatusLabel_) {
        return;
    }

    dicomStatusLabel_->setText(text);

    // 根据状态选择颜色：错误为红色，成功为绿色，其余保持中性灰色
    if (isError) {
        dicomStatusLabel_->setStyleSheet(QStringLiteral("color:#ff6464;"));
    }
    else if (text.contains(QStringLiteral("成功"))) {
        dicomStatusLabel_->setStyleSheet(QStringLiteral("color:#8ae66a;"));
    }
    else {
        dicomStatusLabel_->setStyleSheet(QStringLiteral("color:#d0d0d0;"));
    }
}