#include "c_ui/viewport/ViewportGather.h"

#include <QEvent> 
#include <QGridLayout>
#include <QToolButton>
#include <QTimer>
#include <qdebug.h>

#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkSmartPointer.h>

#include <string>
#include <utility>

void AddHostView(
        HostSessionConfig& config,
        QVTKOpenGLNativeWidget* vtkWidget,
        std::string id,
        HostRenderViewRole role,
        HostRenderMode mode,
        bool isAxesVisible)
        {
        if (!vtkWidget || !vtkWidget->renderWindow()) {
            return;
        }

        HostRenderViewConfig view;

        view.id = std::move(id);
        view.role = role;
        view.window.title = view.id;
        view.window.isAxesVisible = isAxesVisible;
        view.window.viewInit.viewMode = mode;
        view.renderWindow = vtkWidget->renderWindow();
        view.isEventLoopEnabled = false; // Qt已经有 QApplication 事件循环，不能让 core 再启动一个。

        config.renderViews.push_back(std::move(view));
}


ViewportGather::ViewportGather(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
}

void ViewportGather::buildUi()
{
    m_viewGrid = new QGridLayout(this);
    m_viewGrid->setContentsMargins(6, 6, 6, 6);
    m_viewGrid->setHorizontalSpacing(6);
    m_viewGrid->setVerticalSpacing(6);

    viewAxialContainer_ =
        createViewportContainer(m_axial, ViewportId::Axial);

    viewSagittalContainer_ =
        createViewportContainer(m_sagittal, ViewportId::Sagittal);

    viewCoronalContainer_ =
        createViewportContainer(m_coronal, ViewportId::Coronal);

    view3DContainer_ =
        createViewportContainer(m_view3d, ViewportId::View3D);

    setViewportLayout();

    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setSingleShot(true);//表示定时器每次只触发一次，不会永久循环。

    connect(
        m_refreshTimer,
        &QTimer::timeout,
        this,
        &ViewportGather::refreshAllViewports);

    m_axial->installEventFilter(this);
    m_coronal->installEventFilter(this);
    m_sagittal->installEventFilter(this);
    m_view3d->installEventFilter(this);

	//qDebug() << this->objectName();

	//改成状态发生变化时才刷新视图，避免每次都刷新
 //   auto* refreshTimer = new QTimer(this);
 //   connect(
 //       refreshTimer,
 //       &QTimer::timeout,
 //       this,
 //       [this]() {
	//			m_axial->renderWindow()->Render();
 //               m_axial->update();

	//			m_coronal->renderWindow()->Render();
 //               m_coronal->update();
 //           
	//			m_sagittal->renderWindow()->Render();
 //               m_sagittal->update();

	//			m_view3d->renderWindow()->Render();
 //               m_view3d->update();
 //       });
	//refreshTimer->start(33);// 每隔33ms 发出timeout()信号 主动让视图刷新
}

HostSessionConfig ViewportGather::getHostConfig() const
{
    HostSessionConfig config;

    const HostRenderMode primaryMode =
        m_current3DMode == VizMode::CompositeVolume
        ? HostRenderMode::CompositeVolume
        : HostRenderMode::CompositeIsoSurface;

    AddHostView(
        config,
        m_view3d.data(),
        "primary-3d",
        HostRenderViewRole::Primary3D,
        primaryMode,
        true);

    AddHostView(
        config,
        m_axial.data(),
        "slice-top-down",
        HostRenderViewRole::TopDownSlice,
        HostRenderMode::SliceTopDown,
        false);

    AddHostView(
        config,
        m_coronal.data(),
        "slice-front-back",
        HostRenderViewRole::FrontBackSlice,
        HostRenderMode::SliceFrontBack,
        false);

    AddHostView(
        config,
        m_sagittal.data(),
        "slice-left-right",
        HostRenderViewRole::LeftRightSlice,
        HostRenderMode::SliceLeftRight,
        false);

    return config;
}

void ViewportGather::setPrimary3DMode(VizMode mode)
{
    if (mode != VizMode::CompositeVolume
        && mode != VizMode::CompositeIsoSurface) {
        return;
    }

    m_current3DMode = mode;
}

bool ViewportGather::eventFilter(
    QObject* watched,
    QEvent* event)
{
    const bool isViewport =
        watched == m_axial
        || watched == m_coronal
        || watched == m_sagittal
        || watched == m_view3d;

    if (isViewport && event) {
        switch (event->type()) {
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease:
        case QEvent::MouseMove:
        case QEvent::Wheel:
            scheduleViewportRefresh();
            break;
        default:
            break;
        }
    }

    return QWidget::eventFilter(watched, event);
}

void ViewportGather::scheduleViewportRefresh()
{
    if (!m_refreshTimer->isActive()) {
        m_refreshTimer->start(34);
    }
}

void ViewportGather::refreshAllViewports()
{
    if (m_axial) {
        m_axial->renderWindow()->Render();
        m_axial->update();
    }
    if (m_coronal) {
        m_coronal->renderWindow()->Render();
        m_coronal->update();
    }
    if (m_sagittal) {
        m_sagittal->renderWindow()->Render();
        m_sagittal->update();
    }
    if (m_view3d) {
        m_view3d->renderWindow()->Render();
        m_view3d->update();
    }
}

QWidget* ViewportGather::createViewportContainer(
    QPointer<QVTKOpenGLNativeWidget>& vtkWidget,
    ViewportId id)
{
    auto* container = new QWidget(this);

    auto* layout = new QGridLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    vtkWidget = new QVTKOpenGLNativeWidget(container);
    auto renderWindow =  vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
    vtkWidget->setRenderWindow(renderWindow);

    auto* button = new QToolButton(container);
    button->setText(QStringLiteral("□"));
    button->setToolTip(QStringLiteral("放大/还原视口"));
    button->setFixedSize(26, 26);
    button->setStyleSheet(QStringLiteral(
        "QToolButton{"
        "background:rgba(30,30,30,180);"
        "color:white;"
        "border:1px solid #666;"
        "}"
        "QToolButton:hover{"
        "background:rgba(70,70,70,210);"
        "}"));

    connect(
        button,
        &QToolButton::clicked,
        this,
        [this, id]() {
            switchViewMaximized(id);
        });

    layout->addWidget(vtkWidget, 0, 0);
    layout->addWidget(
        button,
        0,
        0,
        Qt::AlignTop | Qt::AlignRight);

    return container;
}

void ViewportGather::switchViewMaximized(ViewportId id)
{
    if (m_maximizedViewport == id) {
        m_maximizedViewport = ViewportId::None;
    }
    else {
        m_maximizedViewport = id;
    }

    setViewportLayout();

    // 这里只请求Qt重绘，不再直接调用旧Service
    if (m_axial) {
        m_axial->update();
    }

    if (m_coronal) {
        m_coronal->update();
    }

    if (m_sagittal) {
        m_sagittal->update();
    }

    if (m_view3d) {
        m_view3d->update();
    }
}

void ViewportGather::setViewportLayout()
{
    if (!m_viewGrid) {
        return;
    }

    auto* axial = viewAxialContainer_.data();
    auto* sagittal = viewSagittalContainer_.data();
    auto* coronal = viewCoronalContainer_.data();
    auto* view3d = view3DContainer_.data(); 

    auto place = [this](
        QWidget* widget,
        int row,
        int column,
        int rowSpan = 1,
        int columnSpan = 1) {
            if (!widget) {
                return;
            }

            m_viewGrid->removeWidget(widget);
            m_viewGrid->addWidget(
                widget,
                row,
                column,
                rowSpan,
                columnSpan);
        };

    if (m_maximizedViewport == ViewportId::None) {
        place(axial, 0, 0);
        place(sagittal, 1, 0);
        place(coronal, 0, 1);
        place(view3d, 1, 1);

        if (axial) {
            axial->show();
        }

        if (sagittal) {
            sagittal->show();
        }

        if (coronal) {
            coronal->show();
        }

        if (view3d) {
            view3d->show();
        }

        return;
    }

    QWidget* target = nullptr;

    if (m_maximizedViewport == ViewportId::Axial) {
        target = axial;
    }

    if (m_maximizedViewport == ViewportId::Sagittal) {
        target = sagittal;
    }

    if (m_maximizedViewport == ViewportId::Coronal) {
        target = coronal;
    }

    if (m_maximizedViewport == ViewportId::View3D) {
        target = view3d;
    }

    if (axial) {
        axial->hide();
    }

    if (sagittal) {
        sagittal->hide();
    }

    if (coronal) {
        coronal->hide();
    }

    if (view3d) {
        view3d->hide();
    }

    if (target) {
        place(target, 0, 0, 2, 2);
        target->show();
    }
}