#include "ReconstructPage.h"
#include <QGridLayout>
#include <QMetaObject>
#include <QWidget>
#include <QDir>
#include <QToolButton>
#include <QVTKOpenGLNativeWidget.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>


ReconstructPage::ReconstructPage(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
}

void ReconstructPage::buildUi()
{
    m_viewGrid = new QGridLayout(this);
    m_viewGrid->setContentsMargins(6, 6, 6, 6);
    m_viewGrid->setHorizontalSpacing(6);
    m_viewGrid->setVerticalSpacing(6);

    viewAxialContainer_ = createViewportContainer(m_axial, ViewportId::Axial);
    viewSagittalContainer_ = createViewportContainer(m_sagittal, ViewportId::Sagittal);
    viewCoronalContainer_ = createViewportContainer(m_coronal, ViewportId::Coronal);
    view3DContainer_ = createViewportContainer(m_view3d, ViewportId::View3D);

    setViewportLayout();
}

void ReconstructPage::initWithData(const Dataset& dataset)
{
	// 这里的reset是为了提前断开与旧数据和状态的关联，确保在后续赋新值时不会有旧数据触发的回调干扰界面更新。通过先reset，再赋新值，可以保证界面在整个过程中保持一致性和稳定性。
	m_lifeToken.reset();
    m_axial.reset();
    m_coronal.reset();
    m_sagittal.reset();
    m_view3d.reset();

    const auto session = dataset.getSession();
    m_dataMgr = session->dataMgr;
    m_sharedState = dataset.getState();
    m_stateBroadcaster = dataset.getBroadcaster();
    m_lifeToken = std::make_shared<int>(1);

	if (!m_dataMgr || !m_sharedState) {
        return;
    }

    m_axial.setMode(VizMode::SliceTop_down);
	m_coronal.setMode(VizMode::SliceFront_back);
    m_sagittal.setMode(VizMode::SliceLeft_right);
    m_view3d.setMode(m_current3DMode);

    m_axial.setBuild(m_dataMgr, m_sharedState, m_stateBroadcaster);
    m_coronal.setBuild(m_dataMgr, m_sharedState, m_stateBroadcaster);
    m_sagittal.setBuild(m_dataMgr, m_sharedState, m_stateBroadcaster);
	m_view3d.setBuild(m_dataMgr, m_sharedState, m_stateBroadcaster);

	//加这个的目的是为了在数据准备好或者spacing改变时刷新界面，之前的版本是直接在外面调用refreshViews()，但这样可能会有时序问题，导致界面没有及时更新。通过设置观察者，当数据准备好或者spacing改变时自动调用refreshViews()，可以确保界面始终与数据状态保持同步。
    m_stateBroadcaster->SetObserver(m_lifeToken, [this](UpdateFlags flags) {
        const bool needsRefresh =
            HasFlag(flags, UpdateFlags::All) || HasFlag(flags, UpdateFlags::DataReady);

        QMetaObject::invokeMethod(this, [this]() {
            refreshViews();
            }, Qt::QueuedConnection);
        });
    
    m_axial.start();
    m_coronal.start();
    m_sagittal.start();
    m_view3d.start();

    refreshViews();
}

void ReconstructPage::setPrimary3DMode(VizMode mode)
{
    if (mode != VizMode::CompositeVolume
        && mode != VizMode::CompositeIsoSurface) {
        return;
    }

    m_current3DMode = mode;
	m_view3d.setMode(m_current3DMode);  

    if (!m_view3d.getService() || !m_view3d.getContext()) {
        return;
    }

    m_view3d.getService()->SetVizMode(m_current3DMode);
    m_view3d.getContext()->SetCameraStyleByVizMode(m_current3DMode);

    request3DRebuildFromCurrentImage();

    m_view3d.getService()->SetPendingUpdatesProcessed();
    m_view3d.getContext()->SetRendered();
}

void ReconstructPage::refreshViews()
{
    m_axial.refresh();
    m_coronal.refresh();
    m_sagittal.refresh();
    m_view3d.refresh();
}


bool ReconstructPage::saveSliceStackAsync(
    const QString& outputDir,
    VizMode sliceMode,
    const double& angel,
    std::function<void(bool)> onComplete)
{
    const QString dir = outputDir.trimmed();
    if (dir.isEmpty()) {
        return false;   
    }
    
    const QByteArray localPath = QDir::toNativeSeparators(dir).toLocal8Bit();

    switch (sliceMode) {
    case VizMode::SliceTop_down:
        m_axial.getService()->SetSliceImagesSavedAsync(localPath.constData(),
            angel,
            std::move(onComplete));
        break;
    case VizMode::SliceFront_back:
        m_coronal.getService()->SetSliceImagesSavedAsync(localPath.constData(),
            angel,
            std::move(onComplete));
        break;
    case VizMode::SliceLeft_right:
        m_sagittal.getService()->SetSliceImagesSavedAsync(localPath.constData(),
            angel,
            std::move(onComplete));
        break;
    default:
        return false;
    }
    return true;
}

bool ReconstructPage::saveTransformedDataAsync(const QString& outputPath, std::function<void(bool)> onComplete)
{
    const QString path = outputPath.trimmed();
    if (path.isEmpty()) {
        return false;
    }

    const QByteArray localPath = QDir::toNativeSeparators(path).toLocal8Bit();

    m_view3d.getService()->SetTransformedDataSavedAsync(
        localPath.constData(),
        std::move(onComplete));

    return true;
}



void ReconstructPage::request3DRebuildFromCurrentImage()
{
    if (!m_dataMgr || !m_sharedState) {
		return;
    }

	auto img = m_dataMgr->GetVtkImage();
    if (!img) {
		return;
    }

    const auto w1 = m_sharedState->GetWindowLevel();
	const auto cursor = m_sharedState->GetCursorWorld();
    const auto rawCurosr = m_sharedState->GetCursorRawWorld();
    const int cursorAxis = m_sharedState->GetCursorAxis();

    double range[2];
    double spacing[3];

	img->GetScalarRange(range);
    img->GetSpacing(spacing);

    m_sharedState->SetReloadDataReady(range[0], range[1],
		{ spacing[0], spacing[1], spacing[2] });

	m_sharedState->SetWindowLevel(w1.windowWidth,w1.windowCenter);
	m_sharedState->SetCursorWorld(cursor[0], cursor[1],cursor[2]);
	m_sharedState->SetCursorRawWorld(rawCurosr[0], rawCurosr[1], rawCurosr[2]);
	m_sharedState->SetCursorAxis(cursorAxis);
}

QWidget* ReconstructPage::createViewportContainer(Viewport& vp, ViewportId id)
{
    auto* container = new QWidget(this);
    auto* layout = new QGridLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* vtkWidget = new QVTKOpenGLNativeWidget(container);
    vp.setAttach(vtkWidget);

    auto* button = new QToolButton(container);
    button->setText(QStringLiteral("□"));
    button->setToolTip(QStringLiteral("放大/还原视口"));
    button->setFixedSize(26, 26);
    button->setStyleSheet(QStringLiteral(
        "QToolButton{background:rgba(30,30,30,180); color:white; border:1px solid #666;}"
        "QToolButton:hover{background:rgba(70,70,70,210);}"));

    connect(button, &QToolButton::clicked, this, [this, id]() {
        switchViewMaximized(id);
        });

    layout->addWidget(vtkWidget, 0, 0);
    layout->addWidget(button, 0, 0, Qt::AlignTop | Qt::AlignRight);

    return container;
}

void ReconstructPage::switchViewMaximized(ViewportId id)
{
    if (m_maximizedViewport == id) {
        m_maximizedViewport = ViewportId::None;
    }
    else {
        m_maximizedViewport = id;
    }

    setViewportLayout();
    refreshViews();
}

void ReconstructPage::setViewportLayout()
{
    if (!m_viewGrid) {
        return;
    }

    auto* axial = viewAxialContainer_.data();
    auto* sagittal = viewSagittalContainer_.data();
    auto* coronal = viewCoronalContainer_.data();
    auto* view3d = view3DContainer_.data();

    auto place = [this](QWidget* w, int row, int col, int rowSpan = 1, int colSpan = 1) {
        if (!w) return;
        m_viewGrid->removeWidget(w);
        m_viewGrid->addWidget(w, row, col, rowSpan, colSpan);
        };

    if (m_maximizedViewport == ViewportId::None) {
        place(axial, 0, 0);
        place(sagittal, 1, 0);
        place(coronal, 0, 1);
        place(view3d, 1, 1);

        if (axial) axial->show();
        if (sagittal) sagittal->show();
        if (coronal) coronal->show();
        if (view3d) view3d->show();
        return;
    }

    QWidget* target = nullptr;
    if (m_maximizedViewport == ViewportId::Axial) target = axial;
    if (m_maximizedViewport == ViewportId::Sagittal) target = sagittal;
    if (m_maximizedViewport == ViewportId::Coronal) target = coronal;
    if (m_maximizedViewport == ViewportId::View3D) target = view3d;

    if (axial) axial->hide();
    if (sagittal) sagittal->hide();
    if (coronal) coronal->hide();
    if (view3d) view3d->hide();

    if (target) {
        place(target, 0, 0, 2, 2);
        target->show();
    }

}
