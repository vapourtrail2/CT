#pragma once

#include "App/AppTypes.h"
#include "Host/Types/HostSessionTypes.h"

#include <QPointer>
#include <QVTKOpenGLNativeWidget.h>
#include <QWidget>

class QGridLayout;

class ViewportGather final : public QWidget
{
    Q_OBJECT

public:
    explicit ViewportGather(QWidget* parent = nullptr);

    HostSessionConfig getHostConfig() const;

    // 目前只保存首次构建时的 3D 模式。
    // 动态切换下一步通过 HostViewSetRequest 实现。
    void setPrimary3DMode(VizMode mode);

private:
    enum class ViewportId {
        None,
        Axial,
        Sagittal,
        Coronal,
        View3D
    };

    void buildUi();

    QWidget* createViewportContainer(
        QPointer<QVTKOpenGLNativeWidget>& vtkWidget,
        ViewportId id);

    void switchViewMaximized(ViewportId id);
    void setViewportLayout();

private:
    QGridLayout* m_viewGrid = nullptr;

    QPointer<QVTKOpenGLNativeWidget> m_axial;
    QPointer<QVTKOpenGLNativeWidget> m_coronal;
    QPointer<QVTKOpenGLNativeWidget> m_sagittal;
    QPointer<QVTKOpenGLNativeWidget> m_view3d;

    QPointer<QWidget> viewAxialContainer_;
    QPointer<QWidget> viewSagittalContainer_;
    QPointer<QWidget> viewCoronalContainer_;
    QPointer<QWidget> view3DContainer_;

    VizMode m_current3DMode =
        VizMode::CompositeIsoSurface;

    ViewportId m_maximizedViewport = ViewportId::None;
};