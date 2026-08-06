#pragma once

#include "App/AppTypes.h"
#include "Host/Types/HostSessionTypes.h"

#include <QPointer>
#include <QVTKOpenGLNativeWidget.h>
#include <QWidget>

//#include <vtkSmartPointer.h>
//#include <vtkWeakPointer.h>

class QGridLayout;
class QTimer;
class QEvent;
//class vtkCallbackCommand;
//class vtkImageData;
//class vtkImageProperty;
//class vtkObject;

class ViewportGather final : public QWidget
{
    Q_OBJECT

public:
    explicit ViewportGather(QWidget* parent = nullptr);
    ~ViewportGather();

    HostSessionConfig getHostConfig() const;

    void requestRefresh();
    //void resetWindowLevelState();

//signals:
//    void windowLevelStateChanged(
//        double windowWidth,
//        double windowCenter,
//        double scalarMin,
//        double scalarMax);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

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
    void scheduleViewportRefresh();
    void refreshAllViewports();
 /*   void bindWindowLevelSource();
    void clearWindowLevelSource();
    void queueWindowLevelStatePublish();
    void publishWindowLevelState();

    static void onWindowLevelModified(
        vtkObject* caller,
        unsigned long eventId,
        void* clientData,
        void* callData);*/

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
    VizMode m_current3DMode = VizMode::CompositeIsoSurface;
    ViewportId m_maximizedViewport = ViewportId::None;
    QTimer* m_refreshTimer = nullptr;
    int m_refresher = 0;
 //   bool m_hasPublishedWindowLevel = false;
 //   double m_publishedWindowWidth = 0.0;
 //   double m_publishedWindowCenter = 0.0;
 //   double m_publishedScalarMin = 0.0;
 //   double m_publishedScalarMax = 0.0;*/
 //   vtkWeakPointer<vtkImageProperty> m_windowLevelProperty;
 //   vtkWeakPointer<vtkImageData> m_windowLevelImage;
 //   vtkSmartPointer<vtkCallbackCommand> m_windowLevelCallback;
 //   unsigned long m_windowLevelObserverTag = 0;
 //   bool m_windowLevelPublishQueued = false;
};
