#pragma once

#include "App/AppInterfaces.h"
#include "Interaction/InteractionRouter.h"
#include <QEvent>
#include <QObject>
#include <QPointer>
#include <QVTKOpenGLNativeWidget.h>
#include <vtkAngleWidget.h>
#include <vtkCallbackCommand.h>
#include <vtkCellPicker.h>
#include <vtkDistanceWidget.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkInteractorStyleImage.h>
#include <vtkInteractorStyleUser.h>
#include <vtkInteractorStyleTrackballActor.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkPropPicker.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkAxesActor.h>
#include <vtkOrientationMarkerWidget.h>

class QtRenderContext : public AbstractRenderContext
{
public:
    QtRenderContext();
    ~QtRenderContext() override;

    void SetQtWidget(QVTKOpenGLNativeWidget* widget);
    void SetStarted() override;
    void SetCameraStyleByVizMode(VizMode mode) override;
	/*   void SetToolMode(ToolMode mode);*/ //测量工具需要重写
    void SetServiceBound(std::shared_ptr<AbstractAppService> service) override;
    void ToggleOrientationAxes(bool show);
    void SetInteractorInitialized() override;//补上

protected:
    void SetVTKEventHandled(vtkObject* caller, long unsigned int eventId, void* callData) override;

private:
    void SetupObservers();
    void TeardownObservers();
    void BuildInteractionRouter();
    void DetachRendererFromWindow();
    bool HandleQtInputEvent(QEvent* event);
    void InstallQtEventFilter();
    void RemoveQtEventFilter();

    std::shared_ptr<AbstractInteractiveService> m_interactiveService;
    InteractionRouter m_interactionRouter;

    QPointer<QVTKOpenGLNativeWidget> m_widget;
    QObject* m_qtEventFilter = nullptr;
    vtkSmartPointer<vtkCallbackCommand> m_eventCallback;
    vtkSmartPointer<vtkRenderWindowInteractor> m_interactor;
    vtkSmartPointer<vtkPropPicker> m_picker;
    vtkSmartPointer<vtkCellPicker> m_slicePicker;
    vtkSmartPointer<vtkDistanceWidget> m_distanceWidget;
    vtkSmartPointer<vtkAngleWidget> m_angleWidget;
    vtkSmartPointer<vtkOrientationMarkerWidget> m_axesWidget;

    VizMode m_currentMode = VizMode::Volume;
    ToolMode m_toolMode = ToolMode::Navigation;


    int m_timerId = -1;
    bool m_observerInstalled = false;
};