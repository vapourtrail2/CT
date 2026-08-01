#include "RenderPanel.h"

#include <QDir>
#include <QFileInfo>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QMetaObject>
#include <QImage>
#include <QPainter>
#include <QResizeEvent>
#include <QStandardPaths>
#include <QThread>
#include <QVBoxLayout>
#include <algorithm>
#include <utility>
#include <cmath>

#include "c_ui/contextarea/WorkSpaceUIState.h"

static inline double clamp01(double v)
{
    if (v < 0.0) return 0.0;
    if (v > 1.0) return 1.0;
    return v;
}

RenderPanel::RenderPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(6, 6, 6, 6);
    v->setSpacing(8);   

    // 直方图屏蔽，不能用死数据
    // 直方图区域
    auto* histGroup = new QGroupBox(QStringLiteral("直方图"), this);
    histGroup->setStyleSheet(
        "QGroupBox{color:#ddd; border:1px solid #333; margin-top:8px;}"
        "QGroupBox::title{subcontrol-origin: margin; left:8px;}"
    );
    auto* hv = new QVBoxLayout(histGroup);

    histLabel_ = new QLabel(QStringLiteral("(未加载)"), histGroup);        
    histLabel_->setFixedHeight(160);
    histLabel_->setMinimumWidth(0);
    histLabel_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    histLabel_->setAlignment(Qt::AlignCenter);
    histLabel_->setStyleSheet("QLabel{background:#111; border:1px solid #444; color:#888;}");
    hv->addWidget(histLabel_);

    v->addWidget(histGroup);
        
    //调节区域
    auto* isoGroup = new QGroupBox(QStringLiteral("调节"), this);
    isoGroup->setStyleSheet(
        "QGroupBox{color:#ddd; border:1px solid #333; margin-top:8px;}"
        "QGroupBox::title{subcontrol-origin: margin; left:8px;}"
    );
    auto* iv = new QVBoxLayout(isoGroup);

    isoValueLabel_ = new QLabel(QStringLiteral("阈值: -"), isoGroup);
    isoSlider_ = new QSlider(Qt::Horizontal, isoGroup);
    isoSlider_->setRange(0, 1000);
    
    iv->addWidget(isoValueLabel_);
    iv->addWidget(isoSlider_);
    v->addWidget(isoGroup);

    //等待更新
  

    auto* wlGroup = new QGroupBox(QStringLiteral("设置"), this);
    wlGroup->setStyleSheet(
        "QGroupBox{color:#ddd; border:1px solid #333; margin-top:8px;}"
        "QGroupBox::title{subcontrol-origin: margin; left:8px;}"
    );
    auto* wv = new QVBoxLayout(wlGroup);

    renderMode_ = new QComboBox(wlGroup);
    renderMode_->addItem(QStringLiteral("等值面渲染"), static_cast<int>(VizMode::CompositeIsoSurface));
    renderMode_->addItem(QStringLiteral("体渲染"), static_cast<int>(VizMode::CompositeVolume));

    auto* renderModeRow = new QHBoxLayout();
    renderModeRow->addWidget(new QLabel(QStringLiteral("3D 模型"), wlGroup));
    renderModeRow->addWidget(renderMode_, 1);
    wv->addLayout(renderModeRow);

    mprPlanesToggle_ = new QCheckBox(QStringLiteral("显示 MPR 平面"), wlGroup);
    wv->addWidget(mprPlanesToggle_);

    crosshairToggle_ = new QCheckBox(QStringLiteral("十字线"), wlGroup);
    wv->addWidget(crosshairToggle_);

    //标量尺控件
    rulerAxesToggle_ = new QCheckBox(QStringLiteral("标量尺"),wlGroup);
    wv->addWidget(rulerAxesToggle_);

    mprPlanesToggle_->setChecked(true);
    crosshairToggle_->setChecked(true);
    rulerAxesToggle_->setChecked(true);

    windowWidthLabel_ = new QLabel(QStringLiteral("窗宽: -"), wlGroup);
    windowWidthSlider_ = new QSlider(Qt::Horizontal, wlGroup);
    windowWidthSlider_->setRange(0, 1000);
    wv->addWidget(windowWidthLabel_);
    wv->addWidget(windowWidthSlider_);

    windowCenterLabel_ = new QLabel(QStringLiteral("窗位: -"), wlGroup);
    windowCenterSlider_ = new QSlider(Qt::Horizontal, wlGroup);
    windowCenterSlider_->setRange(0, 1000);
    wv->addWidget(windowCenterLabel_);
    wv->addWidget(windowCenterSlider_);

    v->addWidget(wlGroup);
    //待更新
    isoSlider_->setEnabled(false);
    windowWidthSlider_->setEnabled(false);
    windowCenterSlider_->setEnabled(false);

    isoValueLabel_->setText(
        QStringLiteral("阈值: 待接入数据范围"));

    windowWidthLabel_->setText(
        QStringLiteral("窗宽: 待接入"));

    windowCenterLabel_->setText(
        QStringLiteral("窗位: 待接入"));

    /*auto sliderToIso = [this](int value) {
        const double t = static_cast<double>(value) / 1000.0;
        return rangeMin_ + (rangeMax_ - rangeMin_) * t;
    };

    auto updateWindowLevelLabels = [this](double ww, double wc) {
        windowWidthLabel_->setText(QStringLiteral("窗宽: %1").arg(ww, 0, 'f', 2));
        windowCenterLabel_->setText(QStringLiteral("窗位: %1").arg(wc, 0, 'f', 2));
    };

    auto pushWindowLevel = [this, updateWindowLevelLabels]() {
        if (!state_ || updatingUi_) {
            return;
        }

        const double ww = sliderToWindowWidth(windowWidthSlider_->value());
        const double wc = sliderToWindowCenter(windowCenterSlider_->value());
        updateWindowLevelLabels(ww, wc);
        state_->SetWindowLevel(ww, wc);
    };

    connect(isoSlider_, &QSlider::sliderPressed, this, [this]() {
        if (!state_ || updatingUi_) {
            return;
        }
        state_->SetInteracting(true);
    });

    connect(isoSlider_, &QSlider::valueChanged, this, [this, sliderToIso](int value) {
        if (!state_ || updatingUi_) {
            return;
        }

        const double iso = sliderToIso(value);
        isoValueLabel_->setText(QStringLiteral("阈值: %1").arg(iso, 0, 'f', 2));
        state_->SetIsoValue(iso);
    });

    connect(isoSlider_, &QSlider::sliderReleased, this, [this, sliderToIso]() {
        if (!state_ || updatingUi_) {
            return;
        }
        state_->SetInteracting(false);
    });

    connect(windowWidthSlider_, &QSlider::sliderPressed, this, [this]() {
        if (!state_ || updatingUi_) {
            return;
        }
        state_->SetInteracting(true);
    });

    connect(windowCenterSlider_, &QSlider::sliderPressed, this, [this]() {
        if (!state_ || updatingUi_) {
            return;
        }
        state_->SetInteracting(true);
    });

    connect(windowWidthSlider_, &QSlider::valueChanged, this, [this, pushWindowLevel](int) {
        pushWindowLevel();
    });

    connect(windowCenterSlider_, &QSlider::valueChanged, this, [this, pushWindowLevel](int) {
        pushWindowLevel();
    });

    auto finishWindowLevelInteraction = [this]() {
        if (!state_ || updatingUi_) {
            return;
        }
        state_->SetInteracting(false);
    };

    connect(windowWidthSlider_, &QSlider::sliderReleased, this, finishWindowLevelInteraction);

    connect(windowCenterSlider_, &QSlider::sliderReleased, this, finishWindowLevelInteraction);*/

    connect(
        renderMode_,
        qOverload<int>(&QComboBox::currentIndexChanged),
        this,
        [this](int index) {
            if (updatingUi_ || index < 0) {
                return;
            }

            const auto mode =
                static_cast<VizMode>(
                    renderMode_->itemData(index).toInt());

            rulerAxesToggle_->setChecked(false);

            if (workSpaceUISpace_) {
                workSpaceUISpace_->setPrimary3DMode(mode);
            }
        });

    connect(
        mprPlanesToggle_,
        &QCheckBox::toggled,
        this,
        [this](bool checked) {
            if (updatingUi_) {
                return;
            }

            HostVisibilityParams visibility;
            visibility.isPlanes3DVisible = checked;

            emit visibilityRequested(
                std::move(visibility));
        });

    connect(
        crosshairToggle_,
        &QCheckBox::toggled,
        this,
        [this](bool checked) {
            if (updatingUi_) {
                return;
            }

            HostVisibilityParams visibility;
            visibility.isCrosshairVisible = checked;

            emit visibilityRequested(
                std::move(visibility));
        });

    connect(
        rulerAxesToggle_,
        &QCheckBox::toggled,
        this,
        [this](bool checked) {
            if (updatingUi_) {
                return;
            }

            HostVisibilityParams visibility;
            visibility.isRulerVisible = checked;

            emit visibilityRequested(
                std::move(visibility));
        });
}

RenderPanel::~RenderPanel() = default;

void RenderPanel::setWorkSpaceUIState(WorkSpaceUIState* state)
{
    workSpaceUISpace_ = state;
}

//void RenderPanel::setAnalysisService(const std::shared_ptr<VolumeAnalysisService>& analysis)
//{
//    analysis_ = analysis;
//}

//void RenderPanel::setSharedState(const std::shared_ptr<SharedInteractionState>& state,
//                                 const std::shared_ptr<SharedStateBroadcaster>& broadcaster)
//{
//    state_ = state;
//	broadcaster_ = broadcaster;
//    lifeToken_ = std::make_shared<int>(1);
//
//    if (!state_) {
//        updatingUi_ = true;
//        isoValueLabel_->setText("阈值: -");
//        windowWidthLabel_->setText("窗宽: -");
//        windowCenterLabel_->setText("窗位: -");
//        isoSlider_->setValue(0);
//        windowWidthSlider_->setValue(0);
//        windowCenterSlider_->setValue(0);
//        renderMode_->setCurrentIndex(0);
//        mprPlanesToggle_->setChecked(true);
//        crosshairToggle_->setChecked(true);
//		rulerAxesToggle_->setChecked(false);
//        updatingUi_ = false;
//
//        histPixmap_ = QPixmap();
//        histLabel_->setPixmap(QPixmap());
//        histLabel_->setText(QStringLiteral("(未加载)"));
//        return;
//    }
//
//    state_->SetElementVisible(VisFlags::Ruler, false);
//
//
//
//    broadcaster_->SetObserver(lifeToken_, [this](UpdateFlags flags) {
//        if (QThread::currentThread() == thread()) {
//            syncFromState(flags);
//            return;
//        }
//
//        QMetaObject::invokeMethod(
//            this,
//            [this, flags]() {
//                syncFromState(flags);
//            },
//            Qt::QueuedConnection);
//    });
//
//    syncFromState(UpdateFlags::All);
//}

//void RenderPanel::syncFromState(UpdateFlags flags)
//{
//    if (!state_) {
//        return;
//    }
//
//    updatingUi_ = true;
//
//    if (HasFlag(flags, UpdateFlags::DataReady) || flags == UpdateFlags::All) {
//        const auto range = state_->GetDataRange();
//        rangeMin_ = range[0];
//        rangeMax_ = range[1];
//        if (rangeMax_ <= rangeMin_) {
//            rangeMax_ = rangeMin_ + 1.0;
//        }
//        rebuildHistogramPixmap();
//    }
//
//    /*if (HasFlag(flags, UpdateFlags::RenderMode) || flags == UpdateFlags::All) {
//        const int mode = static_cast<int>(state_->GetPrimary3DMode());
//        const int index = renderMode_->findData(mode);
//        if (index >= 0) {
//            renderMode_->setCurrentIndex(index);
//        }
//    }*/
//
//    if (HasFlag(flags, UpdateFlags::Visibility) || flags == UpdateFlags::All) {
//        const std::uint32_t mask = state_->GetVisibilityMask();
//        mprPlanesToggle_->setChecked((mask & VisFlags::Planes3D) != 0);
//        crosshairToggle_->setChecked((mask & VisFlags::Crosshair) != 0);
//		rulerAxesToggle_->setChecked((mask & VisFlags::Ruler) != 0);
//    }
//
//    if (HasFlag(flags, UpdateFlags::IsoValue) || flags == UpdateFlags::All) {
//        const double iso = state_->GetIsoValue();
//        double t = 0.0;
//        if (std::abs(rangeMax_ - rangeMin_) > 1e-12) {
//            t = (iso - rangeMin_) / (rangeMax_ - rangeMin_);
//        }
//        t = clamp01(t);
//        isoSlider_->setValue(static_cast<int>(std::round(t * 1000.0)));
//        isoValueLabel_->setText(QStringLiteral("阈值: %1").arg(iso, 0, 'f', 2));
//    }
//
//    if (HasFlag(flags, UpdateFlags::WindowLevel)
//        || HasFlag(flags, UpdateFlags::DataReady)
//        || flags == UpdateFlags::All) {
//        const auto wl = state_->GetWindowLevel();
//        windowWidthSlider_->setValue(windowWidthToSlider(wl.windowWidth));
//        windowCenterSlider_->setValue(windowCenterToSlider(wl.windowCenter));
//        windowWidthLabel_->setText(QStringLiteral("窗宽: %1").arg(wl.windowWidth, 0, 'f', 2));
//        windowCenterLabel_->setText(QStringLiteral("窗位: %1").arg(wl.windowCenter, 0, 'f', 2));
//    }
//
//    updatingUi_ = false;
//}

//void RenderPanel::rebuildHistogramPixmap() //直方图
//{
//    histPixmap_ = QPixmap();
//    histLabel_->setPixmap(QPixmap());
//    if (!analysis_) {
//        histLabel_->setText(QStringLiteral("(未加载)"));
//        return;
//    }
//    vtkSmartPointer<vtkTable> table = analysis_->GetHistogramData(512);
//    if (!table || table->GetNumberOfRows() <= 0) {
//        histLabel_->setText(QStringLiteral("(未加载)"));
//        return;
//    }
//    vtkDataArray* values = vtkDataArray::SafeDownCast(table->GetColumnByName("LogFrequency"));
//    if (!values) {
//        values = vtkDataArray::SafeDownCast(table->GetColumnByName("Frequency"));
//    }
//    if (!values || values->GetNumberOfTuples() <= 0) {
//        histLabel_->setText(QStringLiteral("(未加载)"));
//        return;
//    }
//    const int imageWidth = 512;
//    const int imageHeight = 160;
//    const int baseline = imageHeight - 12;
//    const int topPadding = 8;
//    QImage image(imageWidth, imageHeight, QImage::Format_ARGB32_Premultiplied);
//    image.fill(QColor(17, 17, 17));
//    QPainter painter(&image);
//    painter.fillRect(QRect(0, 0, imageWidth, imageHeight), QColor(17, 17, 17));
//    painter.setPen(QColor(68, 68, 68));
//    painter.drawRect(0, 0, imageWidth - 1, imageHeight - 1);
//    painter.drawLine(0, baseline, imageWidth - 1, baseline);
//    double maxValue = 0.0;
//    for (vtkIdType i = 0; i < values->GetNumberOfTuples(); ++i) {
//        maxValue = std::max(maxValue, values->GetComponent(i, 0));
//    }
//    if (maxValue <= 0.0) {
//        painter.end();
//        histLabel_->setText(QStringLiteral("(直方图为空)"));
//        return;
//    }
//    const vtkIdType count = values->GetNumberOfTuples();
//	for (vtkIdType i = 0; i < count; ++i) {
//        const double value = values->GetComponent(i, 0);
//        const double t = value / maxValue;
//        const int x0 = static_cast<int>((static_cast<double>(i) * imageWidth) / count);
//        const int x1 = static_cast<int>((static_cast<double>(i + 1) * imageWidth) / count);
//        const int barWidth = std::max(1, x1 - x0);
//        const int barHeight = static_cast<int>(std::round(t * (baseline - topPadding)));
//        painter.fillRect(QRect(x0, baseline - barHeight, barWidth, barHeight), QColor(150, 150, 150));
//    }
//    painter.end();
//    histPixmap_ = QPixmap::fromImage(image);
//    applyHistogramPixmap();
//}

//void RenderPanel::applyHistogramPixmap()
//{
//    if (histPixmap_.isNull()) {
//        return;
//    }
//    const QSize targetSize = histLabel_->contentsRect().size();
//    if (targetSize.width() <= 0 || targetSize.height() <= 0) {
//        return;
//    }
//    histLabel_->setText(QStringLiteral());
//    histLabel_->setPixmap(
//		histPixmap_.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation)
//    );
//}

double RenderPanel::currentScalarSpan() const
{
    return std::max(rangeMax_ - rangeMin_, 1e-6);
}

double RenderPanel::currentWindowWidthMin() const
{
    return std::max(currentScalarSpan() * 0.01, 1e-6);
}

double RenderPanel::currentWindowWidthMax() const
{
    return currentScalarSpan() * 2.0;
}

double RenderPanel::currentWindowCenterMin() const
{
    return rangeMin_;
}

double RenderPanel::currentWindowCenterMax() const
{
    return rangeMax_;
}

double RenderPanel::sliderToWindowWidth(int value) const
{
    const double t = static_cast<double>(value) / 1000.0;
    const double minWidth = currentWindowWidthMin();
    const double maxWidth = currentWindowWidthMax();
    if (maxWidth <= minWidth) {
        return minWidth;
    }

    const double logMin = std::log(minWidth);
    const double logMax = std::log(maxWidth);
    return std::exp(logMin + (logMax - logMin) * t);
}

double RenderPanel::sliderToWindowCenter(int value) const
{
    const double t = static_cast<double>(value) / 1000.0;
    const double minCenter = currentWindowCenterMin();
    const double maxCenter = currentWindowCenterMax();
    return minCenter + (maxCenter - minCenter) * t;
}

int RenderPanel::windowWidthToSlider(double value) const
{
    const double minWidth = currentWindowWidthMin();
    const double maxWidth = currentWindowWidthMax();
    if (maxWidth <= minWidth) {
        return 0;
    }

    const double clamped = std::max(minWidth, std::min(value, maxWidth));
    const double logMin = std::log(minWidth);
    const double logMax = std::log(maxWidth);
    const double t = (std::log(clamped) - logMin) / (logMax - logMin);
    return static_cast<int>(std::round(clamp01(t) * 1000.0));
}

int RenderPanel::windowCenterToSlider(double value) const
{
    const double minCenter = currentWindowCenterMin();
    const double maxCenter = currentWindowCenterMax();
    const double span = maxCenter - minCenter;
    const double t = (span <= 1e-12) ? 0.0 : (value - minCenter) / span;
    return static_cast<int>(std::round(clamp01(t) * 1000.0));
}

void RenderPanel::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);
    if (!histPixmap_.isNull()) {
        applyHistogramPixmap();
    }
}

void RenderPanel::rebuildHistogramPixmap()
{
    // 占用
}

void RenderPanel::applyHistogramPixmap()
{
    // 新版 Host 暂未提供直方图数据接口。
}



