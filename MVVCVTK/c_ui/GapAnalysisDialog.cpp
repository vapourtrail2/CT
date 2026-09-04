#include "c_ui/GapAnalysisDialog.h"

#include "c_ui/context/SessionManager.h"

#include <cmath>
#include <utility>

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace
{
QDoubleSpinBox* createDoubleSpin(
    QWidget* parent,
    double minimum,
    double maximum,
    int decimals,
    double step)
{
    auto* spin = new QDoubleSpinBox(parent);
    spin->setRange(minimum, maximum);
    spin->setDecimals(decimals);
    spin->setSingleStep(step);
    spin->setKeyboardTracking(false);
    spin->setMinimumWidth(150);
    return spin;
}

QGroupBox* createSection(const QString& title, QWidget* parent)
{
    auto* group = new QGroupBox(title, parent);
    group->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    return group;
}
}

GapAnalysisDialog::GapAnalysisDialog(
    SessionManager& sessionManager,
    QWidget* parent)
    : QDialog(parent)
    , sessionManager_(sessionManager)
{
    buildUi();
    initializeFromCurrentView();
    refreshState();
}

void GapAnalysisDialog::buildUi()
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(QStringLiteral("孔隙分析"));
    setModal(false);
    resize(820, 500);
    setMinimumSize(720, 440);
    setStyleSheet(QStringLiteral(
        "QDialog{background:#2b2b2b; color:#eeeeee;}"
        "QWidget{color:#eeeeee; font-size:13px;}"
        "QGroupBox{border:1px solid #666666; margin-top:10px; padding:10px 8px 8px 8px;}"
        "QGroupBox::title{subcontrol-origin:margin; left:8px; padding:0 4px;}"
        "QComboBox,QDoubleSpinBox{background:#202020; border:1px solid #666666; padding:4px;}"
        "QComboBox:disabled{color:#d0d0d0; background:#333333;}"
        "QPushButton{background:#444444; border:1px solid #777777; padding:6px 14px;}"
        "QPushButton:hover{background:#555555;}"
        "QPushButton:disabled{color:#777777; background:#333333;}"));

    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(10, 10, 10, 10);
    root->setSpacing(10);

    auto* leftPanel = new QWidget(this);
    auto* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(8);

    auto* algorithmGroup = createSection(QStringLiteral("分析"), leftPanel);
    auto* algorithmForm = new QFormLayout(algorithmGroup);
    algorithmForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    auto* algorithmCombo = new QComboBox(algorithmGroup);
    algorithmCombo->addItem(QStringLiteral("DefX"));
    algorithmCombo->setEnabled(false);
    auto* analysisModeCombo = new QComboBox(algorithmGroup);
    analysisModeCombo->addItem(QStringLiteral("孔隙"));
    analysisModeCombo->setEnabled(false);
    algorithmForm->addRow(QStringLiteral("算法"), algorithmCombo);
    algorithmForm->addRow(QStringLiteral("分析模式"), analysisModeCombo);
    leftLayout->addWidget(algorithmGroup);

    auto* materialGroup = createSection(QStringLiteral("材料定义"), leftPanel);
    auto* materialForm = new QFormLayout(materialGroup);
    materialForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    isoModeCombo_ = new QComboBox(materialGroup);
    isoModeCombo_->addItem(
        QStringLiteral("数据范围比例"),
        static_cast<int>(GapIsoMode::DataRangeRatio));
    isoModeCombo_->addItem(
        QStringLiteral("绝对阈值"),
        static_cast<int>(GapIsoMode::AbsoluteValue));
    isoModeCombo_->setCurrentIndex(1);

    ratioSpin_ = createDoubleSpin(materialGroup, 0.0, 1.0, 6, 0.01);
    ratioSpin_->setValue(0.5);
    ratioSpin_->setSuffix(QStringLiteral("  (0-1)"));
    absoluteIsoSpin_ = createDoubleSpin(
        materialGroup, -1.0e12, 1.0e12, 6, 1.0);

    isoValueStack_ = new QStackedWidget(materialGroup);
    isoValueStack_->addWidget(ratioSpin_);
    isoValueStack_->addWidget(absoluteIsoSpin_);
    isoValueStack_->setCurrentIndex(isoModeCombo_->currentIndex());

    backgroundMeanSpin_ = createDoubleSpin(
        materialGroup, -1.0e12, 1.0e12, 6, 1.0);
    materialMeanSpin_ = createDoubleSpin(
        materialGroup, -1.0e12, 1.0e12, 6, 1.0);

    materialForm->addRow(QStringLiteral("阈值模式"), isoModeCombo_);
    materialForm->addRow(QStringLiteral("等值面阈值"), isoValueStack_);
    materialForm->addRow(QStringLiteral("背景均值"), backgroundMeanSpin_);
    materialForm->addRow(QStringLiteral("材料均值"), materialMeanSpin_);
    leftLayout->addWidget(materialGroup);

    auto* calculationGroup = createSection(QStringLiteral("分析参数"), leftPanel);
    auto* calculationLayout = new QVBoxLayout(calculationGroup);
    edgeDistanceCalculationCheck_ = new QCheckBox(
        QStringLiteral("边距离计算"), calculationGroup);
    edgeDistanceCalculationCheck_->setToolTip(
        QStringLiteral("当前 SDK 未开放边距离计算参数"));
    calculationLayout->addWidget(edgeDistanceCalculationCheck_);
    leftLayout->addWidget(calculationGroup);
    leftLayout->addStretch(1);

    auto* rightPanel = new QWidget(this);
    auto* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(8);

    filterResultsGroup_ = createSection(QStringLiteral("过滤结果"), rightPanel);
    filterResultsGroup_->setCheckable(true);
    filterResultsGroup_->setChecked(true);
    auto* filterForm = new QFormLayout(filterResultsGroup_);
    filterForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    minVolumeSpin_ = createDoubleSpin(
        filterResultsGroup_, 0.0, 1.0e12, 6, 0.1);
    minVolumeSpin_->setSuffix(QStringLiteral(" mm³"));
    filterForm->addRow(QStringLiteral("最小孔隙体积"), minVolumeSpin_);
    rightLayout->addWidget(filterResultsGroup_);

    edgeDistanceFilterGroup_ = createSection(
        QStringLiteral("边距离过滤"), rightPanel);
    auto* edgeFilterLayout = new QVBoxLayout(edgeDistanceFilterGroup_);
    auto* edgeFilterNote = new QLabel(
        QStringLiteral(
            "当前版本SDK尚未开放边距离过滤参数，"
            ),
        edgeDistanceFilterGroup_);
    edgeFilterNote->setWordWrap(true);
    edgeFilterNote->setStyleSheet(QStringLiteral(
        "color:#bcbcbc; padding:6px; background:#242424;"));
    edgeFilterLayout->addWidget(edgeFilterNote);
    rightLayout->addWidget(edgeDistanceFilterGroup_);
    rightLayout->addStretch(1);

    statusLabel_ = new QLabel(
        QStringLiteral("请确认参数后开始分析。"), rightPanel);
    statusLabel_->setWordWrap(true);
    statusLabel_->setMinimumHeight(42);
    statusLabel_->setStyleSheet(QStringLiteral(
        "color:#e6d66a; padding:6px; background:#202020; border:1px solid #555555;"));
    rightLayout->addWidget(statusLabel_);

    auto* actionLayout = new QHBoxLayout();
    actionLayout->setSpacing(8);
    startButton_ = new QPushButton(QStringLiteral("开始分析"), rightPanel);
    overlayButton_ = new QPushButton(QStringLiteral("显示/隐藏"), rightPanel);
    exitButton_ = new QPushButton(QStringLiteral("退出孔隙"), rightPanel);
    auto* closeButton = new QPushButton(QStringLiteral("关闭"), rightPanel);
    for (auto* button : {
        startButton_, overlayButton_, exitButton_, closeButton }) {
        button->setAutoDefault(false);
        actionLayout->addWidget(button);
    }
    rightLayout->addLayout(actionLayout);

    root->addWidget(leftPanel, 1);
    root->addWidget(rightPanel, 1);

    connect(
        isoModeCombo_,
        qOverload<int>(&QComboBox::currentIndexChanged),
        isoValueStack_,
        &QStackedWidget::setCurrentIndex);
    connect(
        filterResultsGroup_,
        &QGroupBox::toggled,
        this,
        [this]() { updateEdgeDistanceFilterVisibility(); });
    connect(
        edgeDistanceCalculationCheck_,
        &QCheckBox::toggled,
        this,
        [this]() { updateEdgeDistanceFilterVisibility(); });
    connect(startButton_, &QPushButton::clicked, this, [this]() {
        startAnalysis();
        });
    connect(overlayButton_, &QPushButton::clicked, this, [this]() {
        toggleOverlay();
        });
    connect(exitButton_, &QPushButton::clicked, this, [this]() {
        exitAnalysis();
        });
    connect(closeButton, &QPushButton::clicked, this, &QDialog::close);
    connect(
        &sessionManager_,
        &SessionManager::sessionChanged,
        this,
        [this](SessionManager::State state) {
            if (state == SessionManager::State::Ready) {
                initializeFromCurrentView();
            }
            refreshState();
        });

    stateTimer_ = new QTimer(this);
    stateTimer_->setInterval(100);
    connect(stateTimer_, &QTimer::timeout, this, [this]() {
        refreshState();
        });
    stateTimer_->start();

    updateEdgeDistanceFilterVisibility();
}

void GapAnalysisDialog::initializeFromCurrentView()
{
    HostViewTarget target;
    target.isViewRoleUsed = true;
    target.viewRole = HostRenderViewRole::Primary3D;

    const auto state = sessionManager_.getRenderViewState(target);
    if (!state) {
        backgroundMeanSpin_->setValue(0.0);
        materialMeanSpin_->setValue(1.0);
        absoluteIsoSpin_->setValue(0.0);
        return;
    }

    const double scalarMin = state->scalarRange[0];
    const double scalarMax = state->scalarRange[1];
    if (std::isfinite(scalarMin)
        && std::isfinite(scalarMax)
        && scalarMin <= scalarMax) {
        backgroundMeanSpin_->setValue(scalarMin);
        materialMeanSpin_->setValue(scalarMax);
    }
    if (std::isfinite(state->isoThreshold)) {
        absoluteIsoSpin_->setValue(state->isoThreshold);
    }
}

void GapAnalysisDialog::updateEdgeDistanceFilterVisibility()
{
    edgeDistanceFilterGroup_->setVisible(
        filterResultsGroup_->isChecked()
        && edgeDistanceCalculationCheck_->isChecked());
}

GapHostStartParams GapAnalysisDialog::buildStartParams() const
{
    GapHostStartParams params;
    params.surface.isoMode = static_cast<GapIsoMode>(
        isoModeCombo_->currentData().toInt());
    params.surface.dataRangeRatio = ratioSpin_->value();
    params.surface.absoluteIsoValue = absoluteIsoSpin_->value();
    params.surface.backgroundMean = static_cast<float>(
        backgroundMeanSpin_->value());
    params.surface.materialMean = static_cast<float>(
        materialMeanSpin_->value());
    params.voidParams.isFilterEnabled = filterResultsGroup_->isChecked();
    params.voidParams.minVolumeMM3 = minVolumeSpin_->value();
    return params;
}

void GapAnalysisDialog::startAnalysis()
{
    if (backgroundMeanSpin_->value() > materialMeanSpin_->value()) {
        setStatus(QStringLiteral("背景均值不能大于材料均值。"), true);
        return;
    }

    const QPointer<GapAnalysisDialog> guard(this);
    QString error;
    const bool isAccepted = sessionManager_.startGap(
        buildStartParams(),
        [guard](bool isDisplayed) {
            if (!guard) {
                return;
            }
            guard->setStatus(
                isDisplayed
                ? QStringLiteral("孔隙分析完成，结果已挂载到视图。")
                : QStringLiteral("孔隙分析未能显示，请检查 Gap 运行时、参数和目标视图。"),
                !isDisplayed);
        },
        &error);

    if (!isAccepted) {
        setStatus(error, true);
        return;
    }

    setStatus(QStringLiteral("孔隙分析已开始，请稍候……"));
    refreshState();
}

void GapAnalysisDialog::toggleOverlay()
{
    QString error;
    if (!sessionManager_.toggleGapOverlay(&error)) {
        setStatus(error, true);
        return;
    }
    setStatus(QStringLiteral("已切换孔隙结果的显示状态。"));
}

void GapAnalysisDialog::exitAnalysis()
{
    QString error;
    if (!sessionManager_.exitGap(&error)) {
        setStatus(error, true);
        return;
    }
    setStatus(QStringLiteral("正在退出孔隙分析……"));
    refreshState();
}

void GapAnalysisDialog::refreshState()
{
    const GapHostState state = sessionManager_.getGapState();
    const int currentState = static_cast<int>(state.analysisState);

    if (currentState != lastAnalysisState_) {
        if (state.analysisState == GapAnalysisState::Running) {
            setStatus(QStringLiteral("孔隙分析正在运行……"));
        }
        else if (state.analysisState == GapAnalysisState::Failed) {
            setStatus(
                QStringLiteral("孔隙分析失败，请检查 Gap 运行时和输入参数。"),
                true);
        }
        else if (state.analysisState == GapAnalysisState::Succeeded) {
            setStatus(QStringLiteral("分析完成，正在确认视图挂载结果……"));
        }
        else if (lastAnalysisState_ >= 0
            && state.analysisState == GapAnalysisState::Idle) {
            setStatus(QStringLiteral("孔隙分析已退出。"));
        }
        lastAnalysisState_ = currentState;
    }

    const bool isReady =
        sessionManager_.getState() == SessionManager::State::Ready;
    startButton_->setEnabled(
        isReady
        && state.analysisState != GapAnalysisState::Running
        && !state.isViewActive
        && !state.isExitPending);
    overlayButton_->setEnabled(
        state.isViewActive && !state.isExitPending);
    exitButton_->setEnabled(
        state.isViewActive && !state.isExitPending);
}

void GapAnalysisDialog::setStatus(
    const QString& message,
    bool isError)
{
    statusLabel_->setText(message);
    statusLabel_->setStyleSheet(QStringLiteral(
        "color:%1; padding:6px; background:#202020; border:1px solid #555555;")
        .arg(isError ? QStringLiteral("#ff8a80") : QStringLiteral("#e6d66a")));
}
