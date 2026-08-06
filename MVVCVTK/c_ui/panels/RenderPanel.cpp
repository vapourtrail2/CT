#include "RenderPanel.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <utility>


constexpr int kWindowLevelSliderSteps = 1000;

double valueFromSlider(
    int position,
    double minimum,
    double maximum)
{
    const double ratio =
        position / static_cast<double>(
            kWindowLevelSliderSteps);
    return minimum + ratio * (maximum - minimum);
}

int sliderFromValue(//把实际值转换为 0～1000 的滑块位置
    double value,
    double minimum,
    double maximum)
{
    if (maximum <= minimum) {
        return 0;
    }

    const double ratio = std::clamp(
        (value - minimum) / (maximum - minimum),
        0.0,
        1.0);
    return static_cast<int>(std::lround(
        ratio * kWindowLevelSliderSteps));
}


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
        QStringLiteral("窗宽: "), wlGroup);
    windowWidthSlider_ = new QSlider(Qt::Horizontal, wlGroup);
    windowWidthSlider_->setRange(
        0,
        kWindowLevelSliderSteps);
    windowWidthSlider_->setEnabled(false);
    wv->addWidget(windowWidthLabel_);
    wv->addWidget(windowWidthSlider_);

    windowCenterLabel_ = new QLabel(
        QStringLiteral("窗位: "), wlGroup);
    windowCenterSlider_ = new QSlider(Qt::Horizontal, wlGroup);
    windowCenterSlider_->setRange(
        0,
        kWindowLevelSliderSteps);
    windowCenterSlider_->setEnabled(false);
    wv->addWidget(windowCenterLabel_);
    wv->addWidget(windowCenterSlider_);

    isoValueLabel_ = new QLabel(
        QStringLiteral("阈值: "), wlGroup);
    isoSlider_ = new QSlider(Qt::Horizontal, wlGroup);
    isoSlider_->setRange(0, 1000);
    isoSlider_->setEnabled(false);
    wv->addWidget(isoValueLabel_);
    wv->addWidget(isoSlider_);

    v->addWidget(wlGroup);

    setConnect();
}

RenderPanel::~RenderPanel() = default;

void RenderPanel::setDataState(bool hasData)
{
    hasData_ = hasData;

    if (!hasData_) {
        hasWindowLevelState_ = false;
        windowWidthLabel_->setText(
            QStringLiteral("窗宽: "));
        windowCenterLabel_->setText(
            QStringLiteral("窗位: "));
    }

    const bool isEnabled =
        hasData_ && hasWindowLevelState_;
    windowWidthSlider_->setEnabled(isEnabled);
    windowCenterSlider_->setEnabled(isEnabled);
}

void RenderPanel::setWindowLevelState(
    double windowWidth,
    double windowCenter,
    double scalarMin,
    double scalarMax)
{
    if (!std::isfinite(windowWidth)
        || windowWidth <= 0.0
        || !std::isfinite(windowCenter)
        || !std::isfinite(scalarMin)
        || !std::isfinite(scalarMax)
        || scalarMax < scalarMin) {
        return;
    }

    const double scalarSpan =
        std::max(0.01, scalarMax - scalarMin);

    windowWidth_ = windowWidth;
    windowCenter_ = windowCenter;
    windowWidthMin_ = 0.01;
    windowWidthMax_ = std::max(
        windowWidth,
        scalarSpan * 2.0);
    windowCenterMin_ = std::min(
        scalarMin,
        windowCenter);
    windowCenterMax_ = std::max(
        scalarMax,
        windowCenter);

    if (windowCenterMax_ <= windowCenterMin_) {
        windowCenterMin_ -= 0.5;
        windowCenterMax_ += 0.5;
    }

    const QSignalBlocker widthBlocker(
        windowWidthSlider_);
    const QSignalBlocker centerBlocker(
        windowCenterSlider_);

    windowWidthSlider_->setValue(
        sliderFromValue(
            windowWidth_,
            windowWidthMin_,
            windowWidthMax_));
    windowCenterSlider_->setValue(
        sliderFromValue(
            windowCenter_,
            windowCenterMin_,
            windowCenterMax_));

    windowWidthLabel_->setText(
        QStringLiteral("窗宽: %1")
            .arg(windowWidth_, 0, 'g', 8));
    windowCenterLabel_->setText(
        QStringLiteral("窗位: %1")
            .arg(windowCenter_, 0, 'g', 8));

    hasWindowLevelState_ = true;
    const bool isEnabled =
        hasData_ && hasWindowLevelState_;
    windowWidthSlider_->setEnabled(isEnabled);
    windowCenterSlider_->setEnabled(isEnabled);
}

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

    connect(
        windowWidthSlider_,
        &QSlider::valueChanged,
        this,
        [this](int position) {
            if (!hasWindowLevelState_) {
                return;
            }

            windowWidth_ = valueFromSlider(
                position,
                windowWidthMin_,
                windowWidthMax_);
            windowWidthLabel_->setText(
                QStringLiteral("窗宽: %1")
                    .arg(windowWidth_, 0, 'g', 8));
        });

    connect(
        windowWidthSlider_,
        &QSlider::sliderReleased,
        this,
        &RenderPanel::requestWindowLevelUpdate);

    connect(
        windowCenterSlider_,
        &QSlider::valueChanged,
        this,
        [this](int position) {
            if (!hasWindowLevelState_) {
                return;
            }

            windowCenter_ = valueFromSlider(
                position,
                windowCenterMin_,
                windowCenterMax_);
            windowCenterLabel_->setText(
                QStringLiteral("窗位: %1")
                    .arg(windowCenter_, 0, 'g', 8));
        });

    connect(
        windowCenterSlider_,
        &QSlider::sliderReleased,
        this,
        &RenderPanel::requestWindowLevelUpdate);
}

void RenderPanel::requestWindowLevelUpdate()
{
    if (!hasData_ || !hasWindowLevelState_) {
        return;
    }

    HostWindowLevelParams windowLevel;
    windowLevel.windowWidth = windowWidth_;
    windowLevel.windowCenter = windowCenter_;
    emit windowLevelRequested(windowLevel);
}
