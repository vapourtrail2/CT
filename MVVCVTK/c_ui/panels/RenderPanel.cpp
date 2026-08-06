#include "RenderPanel.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <utility>

RenderPanel::RenderPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(6, 6, 6, 6);
    v->setSpacing(8);

    auto* wlGroup = new QGroupBox(QStringLiteral("设置"), this);
    wlGroup->setStyleSheet(
        "QGroupBox{color:#ddd; border:1px solid #333; margin-top:8px;}"
        "QGroupBox::title{subcontrol-origin: margin; left:8px;}"
    );
    wlGroup->setFixedHeight(250);
    auto* wv = new QVBoxLayout(wlGroup);

    renderMode_ = new QComboBox(wlGroup);
    renderMode_->addItem(
        QStringLiteral("等值面渲染"),
        static_cast<int>(VizMode::CompositeIsoSurface));
    renderMode_->addItem(
        QStringLiteral("体渲染"),
        static_cast<int>(VizMode::CompositeVolume));

    auto* renderModeRow = new QHBoxLayout();
    renderModeRow->addWidget(
        new QLabel(QStringLiteral("3D 模型"), wlGroup));
    renderModeRow->addWidget(renderMode_, 1);
    wv->addLayout(renderModeRow);

    mprPlanesToggle_ = new QCheckBox(
        QStringLiteral("显示 MPR 平面"), wlGroup);
    wv->addWidget(mprPlanesToggle_);

    crosshairToggle_ = new QCheckBox(
        QStringLiteral("十字线"), wlGroup);
    wv->addWidget(crosshairToggle_);

    rulerAxesToggle_ = new QCheckBox(
        QStringLiteral("标量尺"), wlGroup);
    wv->addWidget(rulerAxesToggle_);

    mprPlanesToggle_->setChecked(false);
    crosshairToggle_->setChecked(true);
    rulerAxesToggle_->setChecked(false);

    windowWidthLabel_ = new QLabel(
        QStringLiteral("窗宽: 待接入"), wlGroup);
    windowWidthSlider_ = new QSlider(Qt::Horizontal, wlGroup);
    windowWidthSlider_->setRange(0, 1000);
    windowWidthSlider_->setEnabled(false);
    wv->addWidget(windowWidthLabel_);
    wv->addWidget(windowWidthSlider_);

    windowCenterLabel_ = new QLabel(
        QStringLiteral("窗位: 待接入"), wlGroup);
    windowCenterSlider_ = new QSlider(Qt::Horizontal, wlGroup);
    windowCenterSlider_->setRange(0, 1000);
    windowCenterSlider_->setEnabled(false);
    wv->addWidget(windowCenterLabel_);
    wv->addWidget(windowCenterSlider_);

    isoValueLabel_ = new QLabel(
        QStringLiteral("阈值: 待接入数据范围"), wlGroup);
    isoSlider_ = new QSlider(Qt::Horizontal, wlGroup);
    isoSlider_->setRange(0, 1000);
    isoSlider_->setEnabled(false);
    wv->addWidget(isoValueLabel_);
    wv->addWidget(isoSlider_);

    v->addWidget(wlGroup);

    setConnect();
}

RenderPanel::~RenderPanel() = default;

void RenderPanel::setConnect() {
    connect(
        renderMode_,
        qOverload<int>(&QComboBox::currentIndexChanged),
        this,
        [this](int index) {
            if (index < 0) {
                return;
            }

            const auto mode = static_cast<VizMode>(renderMode_->itemData(index).toInt());

            if (mode == VizMode::CompositeVolume) {
                emit primary3DModeRequested(
                    HostRenderMode::CompositeVolume);
                return;
            }

            if (mode == VizMode::CompositeIsoSurface) {
                emit primary3DModeRequested(
                    HostRenderMode::CompositeIsoSurface);
                return;
            }
        });

    connect(
        mprPlanesToggle_,
        &QCheckBox::toggled,
        this,
        [this](bool checked) {
            HostVisibilityParams visibility;
            visibility.isPlanes3DVisible = checked;

            emit visibilityRequested(std::move(visibility));
        });

    connect(
        crosshairToggle_,
        &QCheckBox::toggled,
        this,
        [this](bool checked) {
            HostVisibilityParams visibility;
            visibility.isCrosshairVisible = checked;

            emit visibilityRequested(std::move(visibility));
        });

    connect(
        rulerAxesToggle_,
        &QCheckBox::toggled,
        this,
        [this](bool checked) {
            HostVisibilityParams visibility;
            visibility.isRulerVisible = checked;

            emit visibilityRequested(std::move(visibility));
        });
}