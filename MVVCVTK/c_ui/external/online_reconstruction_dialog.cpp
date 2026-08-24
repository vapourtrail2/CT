#include "online_reconstruction_dialog.h"

#include "reconstruction_worker.h"

#include <QCloseEvent>
#include <QDateTime>
#include <QDoubleSpinBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QTextEdit>
#include <QThread>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace fdkui {
namespace {

QLabel* makeSectionTitle(const QString& text, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("sectionTitle"));
    return label;
}

void configureSizeSpin(QSpinBox* spin) {
    spin->setRange(16, 4096);
    spin->setSingleStep(16);
    spin->setValue(512);
    spin->setAlignment(Qt::AlignRight);
}

void configureFovSpin(QDoubleSpinBox* spin) {
    spin->setRange(0.001, 10000.0);
    spin->setDecimals(3);
    spin->setSingleStep(1.0);
    spin->setValue(50.0);
    spin->setSuffix(QStringLiteral(" mm"));
    spin->setAlignment(Qt::AlignRight);
}

QString formatBytes(quint64 bytes) {
    constexpr double mib = 1024.0 * 1024.0;
    constexpr double gib = 1024.0 * 1024.0 * 1024.0;
    if (bytes >= static_cast<quint64>(gib)) {
        return QStringLiteral("%1 GiB").arg(static_cast<double>(bytes) / gib, 0, 'f', 2);
    }
    return QStringLiteral("%1 MiB").arg(static_cast<double>(bytes) / mib, 0, 'f', 1);
}

} // namespace

OnlineReconstructionDialog::OnlineReconstructionDialog(QWidget* parent)
    : QDialog(parent) {
    qRegisterMetaType<fdkui::ReconstructionResultPtr>("fdkui::ReconstructionResultPtr");
    buildUi();

    const QString demoConfig = QStringLiteral("G:/data/20260801/config.ini");
    if (QFileInfo::exists(demoConfig)) {
        configPathEdit_->setText(QDir::toNativeSeparators(QFileInfo(demoConfig).absoluteFilePath()));
    }
    updateVolumeEstimate();
}

OnlineReconstructionDialog::~OnlineReconstructionDialog() {
    if (worker_) {
        worker_->requestCancellation();
    }
    if (workerThread_ && workerThread_->isRunning()) {
        workerThread_->wait();
    }
}

void OnlineReconstructionDialog::setInitialConfigPath(const QString& path) {
    if (!running_) {
        configPathEdit_->setText(QDir::toNativeSeparators(QFileInfo(path).absoluteFilePath()));
    }
}

void OnlineReconstructionDialog::buildUi() {
    setObjectName(QStringLiteral("onlineReconstructionDialog"));
    setWindowTitle(tr("在线 FDK 重建"));
    setMinimumSize(760, 690);
    resize(820, 740);

    auto* root = new QVBoxLayout(this);
    // Keep the explicit dialog minimum size authoritative. Otherwise Qt's
    // default top-level layout constraint can retain the expanded size hint
    // after the advanced panel is hidden and prevent restoring the old height.
    root->setSizeConstraint(QLayout::SetNoConstraint);
    root->setContentsMargins(24, 22, 24, 22);
    root->setSpacing(16);

    auto* title = new QLabel(tr("在线 FDK 重建"), this);
    title->setObjectName(QStringLiteral("dialogTitle"));
    auto* subtitle = new QLabel(
        tr("设置目标体数据参数，并从持续增长的投影 RAW 文件执行增量重建。"),
        this);
    subtitle->setObjectName(QStringLiteral("dialogSubtitle"));
    subtitle->setWordWrap(true);
    root->addWidget(title);
    root->addWidget(subtitle);

    auto* configCard = new QFrame(this);
    configCard->setObjectName(QStringLiteral("card"));
    auto* configLayout = new QVBoxLayout(configCard);
    configLayout->setContentsMargins(18, 16, 18, 16);
    configLayout->setSpacing(10);
    configLayout->addWidget(makeSectionTitle(tr("扫描配置"), configCard));

    auto* pathRow = new QHBoxLayout;
    configPathEdit_ = new QLineEdit(configCard);
    configPathEdit_->setObjectName(QStringLiteral("configPathEdit"));
    configPathEdit_->setPlaceholderText(tr("请选择采集端生成的 config.ini"));
    browseButton_ = new QPushButton(tr("浏览..."), configCard);
    browseButton_->setObjectName(QStringLiteral("browseConfigButton"));
    pathRow->addWidget(configPathEdit_, 1);
    pathRow->addWidget(browseButton_);
    configLayout->addLayout(pathRow);
    root->addWidget(configCard);

    auto* parameterCard = new QFrame(this);
    parameterCard->setObjectName(QStringLiteral("card"));
    auto* parameterLayout = new QVBoxLayout(parameterCard);
    parameterLayout->setContentsMargins(18, 16, 18, 16);
    parameterLayout->setSpacing(12);
    parameterLayout->addWidget(makeSectionTitle(tr("重建空间"), parameterCard));

    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(10);
    grid->addWidget(new QLabel(tr("参数"), parameterCard), 0, 0);
    grid->addWidget(new QLabel(tr("X"), parameterCard), 0, 1);
    grid->addWidget(new QLabel(tr("Y"), parameterCard), 0, 2);
    grid->addWidget(new QLabel(tr("Z"), parameterCard), 0, 3);

    grid->addWidget(new QLabel(tr("体数据尺寸"), parameterCard), 1, 0);
    sizeXSpin_ = new QSpinBox(parameterCard);
    sizeYSpin_ = new QSpinBox(parameterCard);
    sizeZSpin_ = new QSpinBox(parameterCard);
    for (auto* spin : {sizeXSpin_, sizeYSpin_, sizeZSpin_}) {
        configureSizeSpin(spin);
    }
    sizeXSpin_->setObjectName(QStringLiteral("volumeSizeXSpin"));
    sizeYSpin_->setObjectName(QStringLiteral("volumeSizeYSpin"));
    sizeZSpin_->setObjectName(QStringLiteral("volumeSizeZSpin"));
    grid->addWidget(sizeXSpin_, 1, 1);
    grid->addWidget(sizeYSpin_, 1, 2);
    grid->addWidget(sizeZSpin_, 1, 3);

    grid->addWidget(new QLabel(tr("视野范围 FOV"), parameterCard), 2, 0);
    fovXSpin_ = new QDoubleSpinBox(parameterCard);
    fovYSpin_ = new QDoubleSpinBox(parameterCard);
    fovZSpin_ = new QDoubleSpinBox(parameterCard);
    for (auto* spin : {fovXSpin_, fovYSpin_, fovZSpin_}) {
        configureFovSpin(spin);
    }
    fovXSpin_->setObjectName(QStringLiteral("fovXSpin"));
    fovYSpin_->setObjectName(QStringLiteral("fovYSpin"));
    fovZSpin_->setObjectName(QStringLiteral("fovZSpin"));
    grid->addWidget(fovXSpin_, 2, 1);
    grid->addWidget(fovYSpin_, 2, 2);
    grid->addWidget(fovZSpin_, 2, 3);
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(2, 1);
    grid->setColumnStretch(3, 1);
    parameterLayout->addLayout(grid);

    estimateLabel_ = new QLabel(parameterCard);
    estimateLabel_->setObjectName(QStringLiteral("estimateLabel"));
    parameterLayout->addWidget(estimateLabel_);

    advancedToggle_ = new QToolButton(parameterCard);
    advancedToggle_->setObjectName(QStringLiteral("advancedToggle"));
    advancedToggle_->setText(tr("高级参数"));
    advancedToggle_->setCheckable(true);
    advancedToggle_->setChecked(false);
    advancedToggle_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    advancedToggle_->setArrowType(Qt::RightArrow);
    parameterLayout->addWidget(advancedToggle_);

    advancedPanel_ = new QWidget(parameterCard);
    auto* advancedForm = new QFormLayout(advancedPanel_);
    advancedForm->setContentsMargins(18, 2, 0, 2);
    advancedForm->setHorizontalSpacing(18);
    advancedForm->setVerticalSpacing(8);
    binningSpin_ = new QSpinBox(advancedPanel_);
    binningSpin_->setRange(1, 16);
    binningSpin_->setValue(1);
    batchViewsSpin_ = new QSpinBox(advancedPanel_);
    batchViewsSpin_->setRange(1, 4096);
    batchViewsSpin_->setValue(8);
    latencySpin_ = new QSpinBox(advancedPanel_);
    latencySpin_->setRange(1, 60000);
    latencySpin_->setValue(250);
    latencySpin_->setSuffix(QStringLiteral(" ms"));
    advancedForm->addRow(tr("探测器合并倍数"), binningSpin_);
    advancedForm->addRow(tr("最大批处理投影视图数"), batchViewsSpin_);
    advancedForm->addRow(tr("最大批处理等待时间"), latencySpin_);
    advancedPanel_->setVisible(false);
    parameterLayout->addWidget(advancedPanel_);
    root->addWidget(parameterCard);

    auto* statusCard = new QFrame(this);
    statusCard->setObjectName(QStringLiteral("card"));
    auto* statusLayout = new QVBoxLayout(statusCard);
    statusLayout->setContentsMargins(18, 16, 18, 16);
    statusLayout->setSpacing(9);
    auto* statusHeader = new QHBoxLayout;
    statusHeader->addWidget(makeSectionTitle(tr("重建状态"), statusCard));
    statusHeader->addStretch();
    stateLabel_ = new QLabel(tr("就绪"), statusCard);
    stateLabel_->setObjectName(QStringLiteral("stateBadge"));
    statusHeader->addWidget(stateLabel_);
    statusLayout->addLayout(statusHeader);

    progressBar_ = new QProgressBar(statusCard);
    progressBar_->setObjectName(QStringLiteral("reconstructionProgress"));
    progressBar_->setRange(0, 100);
    progressBar_->setValue(0);
    progressBar_->setFormat(tr("等待启动"));
    statusLayout->addWidget(progressBar_);

    auto* statusDetails = new QHBoxLayout;
    frameCountsLabel_ = new QLabel(tr("预计 0  |  已接收 0  |  已处理 0"), statusCard);
    elapsedLabel_ = new QLabel(tr("耗时 00:00"), statusCard);
    statusDetails->addWidget(frameCountsLabel_);
    statusDetails->addStretch();
    statusDetails->addWidget(elapsedLabel_);
    statusLayout->addLayout(statusDetails);

    logEdit_ = new QTextEdit(statusCard);
    logEdit_->setObjectName(QStringLiteral("reconstructionLog"));
    logEdit_->setReadOnly(true);
    logEdit_->setMinimumHeight(95);
    logEdit_->setPlaceholderText(tr("算法运行日志将显示在这里。"));
    statusLayout->addWidget(logEdit_);
    root->addWidget(statusCard, 1);

    auto* buttonRow = new QHBoxLayout;
    closeButton_ = new QPushButton(tr("关闭"), this);
    closeButton_->setObjectName(QStringLiteral("closeButton"));
    buttonRow->addWidget(closeButton_);
    buttonRow->addStretch();
    cancelButton_ = new QPushButton(tr("取消"), this);
    cancelButton_->setObjectName(QStringLiteral("cancelButton"));
    cancelButton_->setEnabled(false);
    startButton_ = new QPushButton(tr("开始重建"), this);
    startButton_->setObjectName(QStringLiteral("startButton"));
    startButton_->setDefault(true);
    buttonRow->addWidget(cancelButton_);
    buttonRow->addWidget(startButton_);
    root->addLayout(buttonRow);

    elapsedTimer_ = new QTimer(this);
    elapsedTimer_->setInterval(500);

    connect(browseButton_, &QPushButton::clicked, this, &OnlineReconstructionDialog::browseConfig);
    connect(startButton_, &QPushButton::clicked, this, &OnlineReconstructionDialog::startReconstruction);
    connect(cancelButton_, &QPushButton::clicked, this, &OnlineReconstructionDialog::cancelReconstruction);
    connect(closeButton_, &QPushButton::clicked, this, &QDialog::close);
    connect(advancedToggle_, &QToolButton::toggled, this, &OnlineReconstructionDialog::toggleAdvanced);
    connect(elapsedTimer_, &QTimer::timeout, this, &OnlineReconstructionDialog::updateElapsedDisplay);
    for (auto* spin : {sizeXSpin_, sizeYSpin_, sizeZSpin_}) {
        connect(spin, qOverload<int>(&QSpinBox::valueChanged), this, &OnlineReconstructionDialog::updateVolumeEstimate);
    }

    setStyleSheet(QStringLiteral(R"(
        QDialog { background: #f3f6fa; color: #172033; }
        QLabel#dialogTitle { font-size: 24px; font-weight: 700; color: #13213c; }
        QLabel#dialogSubtitle { color: #64748b; font-size: 13px; }
        QFrame#card { background: white; border: 1px solid #dce3ec; border-radius: 9px; }
        QLabel#sectionTitle { font-size: 14px; font-weight: 600; color: #1e293b; }
        QLabel#estimateLabel { color: #64748b; }
        QLabel#stateBadge { background: #e8eef7; color: #475569; border-radius: 9px; padding: 3px 10px; font-weight: 600; }
        QLineEdit, QSpinBox, QDoubleSpinBox, QTextEdit {
            background: #fbfcfe; border: 1px solid #cdd7e4; border-radius: 5px; padding: 6px;
            selection-background-color: #2563eb;
        }
        QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QTextEdit:focus { border: 1px solid #2563eb; }
        QPushButton { min-height: 30px; padding: 2px 16px; border: 1px solid #c8d2df; border-radius: 5px; background: white; }
        QPushButton:hover { background: #f1f5f9; }
        QPushButton#startButton { background: #2563eb; color: white; border-color: #2563eb; font-weight: 600; }
        QPushButton#startButton:hover { background: #1d4ed8; }
        QPushButton#cancelButton { color: #b42318; }
        QToolButton#advancedToggle { color: #315a9a; border: none; padding: 4px 0; font-weight: 600; }
        QProgressBar { border: none; border-radius: 4px; background: #e7edf5; height: 12px; text-align: center; }
        QProgressBar::chunk { border-radius: 4px; background: #2563eb; }
    )"));
}

void OnlineReconstructionDialog::browseConfig() {
    const QString current = configPathEdit_->text().trimmed();
    const QString startDirectory = current.isEmpty() ? QString() : QFileInfo(current).absolutePath();
    const QString selected = QFileDialog::getOpenFileName(
        this, tr("选择扫描配置"), startDirectory, tr("INI 文件 (*.ini);;所有文件 (*.*)"));
    if (!selected.isEmpty()) {
        configPathEdit_->setText(QDir::toNativeSeparators(QFileInfo(selected).absoluteFilePath()));
    }
}

void OnlineReconstructionDialog::startReconstruction() {
    if (running_) {
        return;
    }

    const QString iniPath = configPathEdit_->text().trimmed();
    const QFileInfo iniInfo(iniPath);
    if (iniPath.isEmpty() || !iniInfo.exists() || !iniInfo.isFile()) {
        QMessageBox::warning(this, tr("配置无效"), tr("开始前请选择一个存在的 config.ini 文件。"));
        configPathEdit_->setFocus();
        return;
    }

    constexpr quint64 warningThreshold = 4ULL * 1024ULL * 1024ULL * 1024ULL;
    if (estimatedOutputBytes() >= warningThreshold) {
        const auto choice = QMessageBox::warning(
            this,
            tr("重建体数据较大"),
            tr("仅输出体数据预计需要 %1，算法还会使用额外的显存和内存。是否继续？")
                .arg(formatBytes(estimatedOutputBytes())),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (choice != QMessageBox::Yes) {
            return;
        }
    }

    ReconstructionRequest request;
    request.configIniPath = iniInfo.absoluteFilePath();
    request.volumeSizeX = static_cast<std::uint32_t>(sizeXSpin_->value());
    request.volumeSizeY = static_cast<std::uint32_t>(sizeYSpin_->value());
    request.volumeSizeZ = static_cast<std::uint32_t>(sizeZSpin_->value());
    request.fovXmm = fovXSpin_->value();
    request.fovYmm = fovYSpin_->value();
    request.fovZmm = fovZSpin_->value();
    request.detectorBinning = static_cast<std::uint32_t>(binningSpin_->value());
    request.maximumBatchViews = static_cast<std::uint32_t>(batchViewsSpin_->value());
    request.maximumBatchLatencyMs = static_cast<std::uint32_t>(latencySpin_->value());

    auto* thread = new QThread(this);
    auto* worker = new ReconstructionWorker(std::move(request));
    worker->moveToThread(thread);
    workerThread_ = thread;
    worker_ = worker;

    connect(thread, &QThread::started, worker, &ReconstructionWorker::run);
    connect(worker, &ReconstructionWorker::progressChanged, this, &OnlineReconstructionDialog::onProgress);
    connect(worker, &ReconstructionWorker::succeeded, this, &OnlineReconstructionDialog::onSucceeded);
    connect(worker, &ReconstructionWorker::failed, this, &OnlineReconstructionDialog::onFailed);
    connect(worker, &ReconstructionWorker::cancelled, this, &OnlineReconstructionDialog::onCancelled);
    connect(worker, &ReconstructionWorker::workFinished, worker, &QObject::deleteLater);
    connect(worker, &ReconstructionWorker::workFinished, thread, &QThread::quit, Qt::DirectConnection);
    connect(thread, &QThread::finished, this, &OnlineReconstructionDialog::onWorkerThreadFinished);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);

    running_ = true;
    cancellationRequested_ = false;
    closeWhenFinished_ = false;
    setInputsEnabled(false);
    startButton_->setEnabled(false);
    cancelButton_->setEnabled(true);
    progressBar_->setRange(0, 0);
    progressBar_->setFormat(tr("正在启动算法..."));
    setState(tr("正在启动"), QStringLiteral("#2563eb"));
    appendLog(tr("使用以下配置启动 FDK：%1")
                  .arg(QDir::toNativeSeparators(QFileInfo(iniPath).absoluteFilePath())));
    elapsedClock_.restart();
    elapsedTimer_->start();
    thread->start();
}

void OnlineReconstructionDialog::cancelReconstruction() {
    if (!running_ || cancellationRequested_) {
        return;
    }
    cancellationRequested_ = true;
    cancelButton_->setEnabled(false);
    setState(tr("正在取消"), QStringLiteral("#b54708"));
    appendLog(tr("已请求取消，正在等待 SDK 工作线程安全停止。"));
    if (worker_) {
        worker_->requestCancellation();
    }
}

void OnlineReconstructionDialog::toggleAdvanced(bool expanded) {
    const int preservedWidth = width();
    if (expanded) {
        collapsedHeight_ = height();
    }
    advancedToggle_->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
    advancedPanel_->setVisible(expanded);
    if (layout()) {
        layout()->activate();
    }
    if (expanded) {
        resize(preservedWidth, std::max(height(), sizeHint().height()));
    } else if (collapsedHeight_ > 0) {
        const int targetHeight = collapsedHeight_;
        // QToolButton and the form layout both post geometry updates while the
        // toggled signal is being handled. Restore the window after those
        // updates, otherwise their final size hint can grow it again.
        QTimer::singleShot(0, this, [this, preservedWidth, targetHeight]() {
            resize(preservedWidth, targetHeight);
        });
    }
}

quint64 OnlineReconstructionDialog::estimatedOutputBytes() const {
    return static_cast<quint64>(sizeXSpin_->value())
        * static_cast<quint64>(sizeYSpin_->value())
        * static_cast<quint64>(sizeZSpin_->value())
        * static_cast<quint64>(sizeof(float));
}

void OnlineReconstructionDialog::updateVolumeEstimate() {
    estimateLabel_->setText(
        tr("预计 float32 输出大小：%1（不含算法工作内存）。").arg(formatBytes(estimatedOutputBytes())));
}

void OnlineReconstructionDialog::updateElapsedDisplay() {
    const qint64 seconds = elapsedClock_.isValid() ? elapsedClock_.elapsed() / 1000 : 0;
    elapsedLabel_->setText(
        tr("耗时 %1:%2")
            .arg(seconds / 60, 2, 10, QLatin1Char('0'))
            .arg(seconds % 60, 2, 10, QLatin1Char('0')));
}

void OnlineReconstructionDialog::onProgress(int state, quint32 expected, quint32 received, quint32 processed) {
    frameCountsLabel_->setText(
        tr("预计 %1  |  已接收 %2  |  已处理 %3").arg(expected).arg(received).arg(processed));
    if (expected > 0) {
        progressBar_->setRange(0, static_cast<int>(std::min<quint32>(expected, static_cast<quint32>(INT_MAX))));
        progressBar_->setValue(static_cast<int>(std::min<quint32>(processed, static_cast<quint32>(INT_MAX))));
        progressBar_->setFormat(tr("已处理 %1 / %2 帧").arg(processed).arg(expected));
    }

    switch (state) {
    case 1:
        if (!cancellationRequested_) {
            setState(tr("运行中"), QStringLiteral("#2563eb"));
        }
        break;
    case 2:
        setState(tr("正在获取结果"), QStringLiteral("#2563eb"));
        progressBar_->setRange(0, 0);
        progressBar_->setFormat(tr("正在将体数据复制到 UI 内存..."));
        break;
    case 3:
        setState(tr("已取消"), QStringLiteral("#b54708"));
        break;
    case 4:
        setState(tr("失败"), QStringLiteral("#b42318"));
        break;
    default:
        break;
    }
}

void OnlineReconstructionDialog::onSucceeded(ReconstructionResultPtr result) {
    elapsedTimer_->stop();
    updateElapsedDisplay();
    progressBar_->setRange(0, 100);
    progressBar_->setValue(100);
    progressBar_->setFormat(tr("重建完成"));
    setState(tr("已完成"), QStringLiteral("#067647"));
    const quint64 voxels = result && result->voxelsZyx ? static_cast<quint64>(result->voxelsZyx->size()) : 0;
    appendLog(tr("重建成功：%1 个体素已返回主界面，总耗时 %2 ms。")
                  .arg(voxels)
                  .arg(result ? result->elapsedMs : 0));
    emit reconstructionCompleted(std::move(result));
}

void OnlineReconstructionDialog::onFailed(const QString& message) {
    elapsedTimer_->stop();
    updateElapsedDisplay();
    progressBar_->setRange(0, 100);
    progressBar_->setValue(0);
    progressBar_->setFormat(tr("重建失败"));
    setState(tr("失败"), QStringLiteral("#b42318"));
    appendLog(tr("ERROR: %1").arg(message));
    QMessageBox::critical(this, tr("FDK 重建失败"), message);
}

void OnlineReconstructionDialog::onCancelled() {
    elapsedTimer_->stop();
    updateElapsedDisplay();
    progressBar_->setRange(0, 100);
    progressBar_->setValue(0);
    progressBar_->setFormat(tr("重建已取消"));
    setState(tr("已取消"), QStringLiteral("#b54708"));
    appendLog(tr("重建任务已取消。"));
}

void OnlineReconstructionDialog::onWorkerThreadFinished() {
    running_ = false;
    cancellationRequested_ = false;
    worker_ = nullptr;
    workerThread_ = nullptr;
    setInputsEnabled(true);
    startButton_->setEnabled(true);
    cancelButton_->setEnabled(false);
    if (closeWhenFinished_) {
        closeWhenFinished_ = false;
        close();
    }
}

void OnlineReconstructionDialog::setInputsEnabled(bool enabled) {
    configPathEdit_->setEnabled(enabled);
    browseButton_->setEnabled(enabled);
    sizeXSpin_->setEnabled(enabled);
    sizeYSpin_->setEnabled(enabled);
    sizeZSpin_->setEnabled(enabled);
    fovXSpin_->setEnabled(enabled);
    fovYSpin_->setEnabled(enabled);
    fovZSpin_->setEnabled(enabled);
    advancedToggle_->setEnabled(enabled);
    binningSpin_->setEnabled(enabled);
    batchViewsSpin_->setEnabled(enabled);
    latencySpin_->setEnabled(enabled);
}

void OnlineReconstructionDialog::appendLog(const QString& message) {
    logEdit_->append(QStringLiteral("[%1] %2")
                         .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")), message.toHtmlEscaped()));
}

void OnlineReconstructionDialog::setState(const QString& text, const QString& color) {
    stateLabel_->setText(text);
    stateLabel_->setStyleSheet(
        QStringLiteral("background:%1; color:white; border-radius:9px; padding:3px 10px; font-weight:600;").arg(color));
}

void OnlineReconstructionDialog::closeEvent(QCloseEvent* event) {
    if (!running_) {
        event->accept();
        return;
    }

    const auto choice = QMessageBox::question(
        this,
        tr("取消重建？"),
        tr("重建任务仍在运行。是否取消任务，并在 SDK 安全停止后关闭窗口？"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (choice == QMessageBox::Yes) {
        closeWhenFinished_ = true;
        cancelReconstruction();
    }
    event->ignore();
}

} // namespace fdkui
