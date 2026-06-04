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

    viewAxialContainer_ = createViewportContainer(viewAxial_, ViewportId::Axial);
    viewSagittalContainer_ = createViewportContainer(viewSagittal_, ViewportId::Sagittal);
    viewCoronalContainer_ = createViewportContainer(viewCoronal_, ViewportId::Coronal);
    view3DContainer_ = createViewportContainer(viewReserved_, ViewportId::View3D);

    setViewportLayout();
}

void ReconstructPage::initWithData(
    std::shared_ptr<AbstractDataManager> data,
    std::shared_ptr<SharedInteractionState> state,
	std::shared_ptr<SharedStateBroadcaster> broadcaster
 )
{
	// 这里的reset是为了提前断开与旧数据和状态的关联，确保在后续赋新值时不会有旧数据触发的回调干扰界面更新。通过先reset，再赋新值，可以保证界面在整个过程中保持一致性和稳定性。
	m_lifeToken.reset();
    m_ctxAxial.reset();
    m_ctxCoronal.reset();
    m_ctxSagittal.reset();
    m_ctx3D.reset();

	m_svc3D.reset();
	m_svcSagittal.reset();
	m_svcCoronal.reset();
	m_svcAxial.reset();
    
    m_dataMgr = std::move(data);
    m_sharedState = std::move(state);
    m_stateBroadcaster = std::move(broadcaster);
    m_lifeToken = std::make_shared<int>(1);

	if (!m_dataMgr || !m_sharedState) {//openFile() 会先创建 session、发 sessionChanged，但文件还在后台读。此时 GetVtkImage() 很可能是空 不需要!m_datamgr->GetVtkImage() 
        return;
    }

    auto getVtkWidget = [](const QPointer<QWidget>& widget) -> QVTKOpenGLNativeWidget* {
        return qobject_cast<QVTKOpenGLNativeWidget*>(widget.data());
    };

    m_svcAxial = std::make_shared<MedicalVizService>(m_dataMgr, m_sharedState, m_stateBroadcaster);
    m_ctxAxial = std::make_shared<QtRenderContext>();
    m_ctxAxial->SetQtWidget(getVtkWidget(viewAxial_));
    m_ctxAxial->SetServiceBound(m_svcAxial);
    m_svcAxial->SetVizMode(VizMode::SliceTop_down);
    m_ctxAxial->SetCameraStyleByVizMode(VizMode::SliceTop_down);
    m_ctxAxial->ToggleOrientationAxes(true);

    m_svcCoronal = std::make_shared<MedicalVizService>(m_dataMgr, m_sharedState, m_stateBroadcaster);
    m_ctxCoronal = std::make_shared<QtRenderContext>();
    m_ctxCoronal->SetQtWidget(getVtkWidget(viewCoronal_));
    m_ctxCoronal->SetServiceBound(m_svcCoronal);
    m_svcCoronal->SetVizMode(VizMode::SliceFront_back);
    m_ctxCoronal->SetCameraStyleByVizMode(VizMode::SliceFront_back);
    m_ctxCoronal->ToggleOrientationAxes(true);

    m_svcSagittal = std::make_shared<MedicalVizService>(m_dataMgr, m_sharedState, m_stateBroadcaster);
    m_ctxSagittal = std::make_shared<QtRenderContext>();
    m_ctxSagittal->SetQtWidget(getVtkWidget(viewSagittal_));
    m_ctxSagittal->SetServiceBound(m_svcSagittal);
    m_svcSagittal->SetVizMode(VizMode::SliceLeft_right);
    m_ctxSagittal->SetCameraStyleByVizMode(VizMode::SliceLeft_right);
    m_ctxSagittal->ToggleOrientationAxes(true);

    m_svc3D = std::make_shared<MedicalVizService>(m_dataMgr, m_sharedState, m_stateBroadcaster);
    m_ctx3D = std::make_shared<QtRenderContext>();
    m_ctx3D->SetQtWidget(getVtkWidget(viewReserved_));
    m_ctx3D->SetServiceBound(m_svc3D);
    m_ctx3D->ToggleOrientationAxes(true);
    m_svc3D->SetVizMode(m_current3DMode);
	m_ctx3D->SetCameraStyleByVizMode(m_current3DMode);
    
	//加这个的目的是为了在数据准备好或者spacing改变时刷新界面，之前的版本是直接在外面调用refreshViews()，但这样可能会有时序问题，导致界面没有及时更新。通过设置观察者，当数据准备好或者spacing改变时自动调用refreshViews()，可以确保界面始终与数据状态保持同步。
    m_stateBroadcaster->SetObserver(m_lifeToken, [this](UpdateFlags flags) {
        const bool needsRefresh =
            HasFlag(flags, UpdateFlags::All) || HasFlag(flags, UpdateFlags::DataReady);

        if (!needsRefresh) {
			return;
        }

        QMetaObject::invokeMethod(this, [this]() {
            refreshViews();
            }, Qt::QueuedConnection);
        });

    //可能是GetVtkImage()!= nullptr那个问题
	auto img = m_dataMgr->GetVtkImage();
    if (img) {
        int dims[3] = { 0,0,0 };
        img->GetDimensions(dims);
        if (dims[0] > 0 && dims[1] > 0 && dims[2] > 0) {
            double range[2];
            double spacing[3];
            img->GetScalarRange(range);
            img->GetSpacing(spacing);
        }
    }
    
    if (m_ctxAxial) m_ctxAxial->SetStarted();
    if (m_ctxCoronal) m_ctxCoronal->SetStarted();
    if (m_ctxSagittal) m_ctxSagittal->SetStarted();
    if (m_ctx3D) m_ctx3D->SetStarted();

    refreshViews();
}

void ReconstructPage::refreshViews()
{
    if (m_svcAxial) m_svcAxial->SetPendingUpdatesProcessed();
    if (m_svcCoronal) m_svcCoronal->SetPendingUpdatesProcessed();
    if (m_svcSagittal) m_svcSagittal->SetPendingUpdatesProcessed();
    if (m_svc3D) m_svc3D->SetPendingUpdatesProcessed();

    if (m_ctxAxial) m_ctxAxial->SetRendered();
    if (m_ctxCoronal) m_ctxCoronal->SetRendered();
    if (m_ctxSagittal) m_ctxSagittal->SetRendered();
    if (m_ctx3D) m_ctx3D->SetRendered();
}

//void ReconstructPage::setToolMode(ToolMode mode)
//{
//    if (m_ctxAxial)
//        m_ctxAxial->SetToolMode(mode);
//
//    if (m_ctxCoronal)
//		m_ctxCoronal->SetToolMode(mode);
//
//	if (m_ctxSagittal)
//		m_ctxSagittal->SetToolMode(mode);
//}

void ReconstructPage::setPrimary3DMode(VizMode mode)
{
    if (mode != VizMode::CompositeVolume && mode != VizMode::CompositeIsoSurface) {
        return;
    }

	m_current3DMode = mode;
    
    if (!m_svc3D || !m_ctx3D) {
		return;
    }

    applyPrimary3DMode(mode);
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
        m_svcAxial->SetSliceImagesSavedAsync(localPath.constData(),
            angel,
            std::move(onComplete));
        break;
    case VizMode::SliceFront_back:
        m_svcCoronal->SetSliceImagesSavedAsync(localPath.constData(),
            angel,
            std::move(onComplete));
        break;
    case VizMode::SliceLeft_right:
        m_svcSagittal->SetSliceImagesSavedAsync(localPath.constData(),
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

    if (!m_svc3D) {
        return false;
    }

    const QByteArray localPath = QDir::toNativeSeparators(path).toLocal8Bit();

    m_svc3D->SetTransformedDataSavedAsync(
        localPath.constData(),
        std::move(onComplete));

    return true;
}

void ReconstructPage::applyPrimary3DMode(VizMode mode)
{   
    m_svc3D->SetVizMode(mode);
    m_ctx3D->SetCameraStyleByVizMode(mode);

    request3DRebuildFromCurrentImage();

    m_svc3D->SetPendingUpdatesProcessed();
    m_ctx3D->SetRendered();
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

QWidget* ReconstructPage::createViewportContainer(QPointer<QWidget>& vtkView, ViewportId id)
{
    auto* container = new QWidget(this);
    auto* layout = new QGridLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* vtkWidget = new QVTKOpenGLNativeWidget(container);
    vtkView = vtkWidget;

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
