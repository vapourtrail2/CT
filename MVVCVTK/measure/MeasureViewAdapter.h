#pragma once

#include "MVVCVTK/API/Host/Types/HostValueTypes.h"
#include "MVVCVTK/SPI/Data/TrustedImageState.h"
#include "MVVCVTK/SPI/Interaction/InteractionTypes.h"
#include "measure/MeasurementTypes.h"
#include "measure/MeasurementViewInitState.h"

#include <QPointer>

#include <array>
#include <functional>
#include <optional>
#include <vector>

#include <vtkSmartPointer.h>

class QVTKOpenGLNativeWidget;
class vtkCallbackCommand;
class vtkGenericOpenGLRenderWindow;
class vtkImageSlice;
class vtkImageSliceMapper;
class vtkMatrix4x4;
class vtkObject;
class vtkRenderer;

namespace measure {

class MeasureViewAdapter final {
public:
    using InputCallback =
        std::function<InteractionResult(const InteractionEvent&)>;

    MeasureViewAdapter();
    ~MeasureViewAdapter();

    MeasureViewAdapter(const MeasureViewAdapter&) = delete;
    MeasureViewAdapter& operator=(const MeasureViewAdapter&) = delete;

    bool Build(
        QVTKOpenGLNativeWidget* widget,
        TrustedImageSnapshot image,
        MeasureView view,
        const MeasurementViewInitState& initialState);
    void Reset();

    bool SetView(MeasureView view);
    void SetInputCallback(InputCallback callback);
    void SendRender();

    bool GetIsReady() const noexcept;
    TrustedImageSnapshot GetImageSnapshot() const;
    vtkRenderer* GetRenderer() const noexcept;
    std::array<double, 3> GetCursorWorld() const noexcept;
    HostWindowLevelParams GetWindowLevel() const noexcept;
    Point3 GetWorldPoint(const Point3& modelPoint) const;
    Point3 GetModelPoint(const Point3& worldPoint) const;
    Point3 GetWorldVector(const Point3& modelVector) const;
    std::optional<Point3> GetDisplayModel(int x, int y) const;

private:
    static Point3 GetMatrixPoint(
        vtkMatrix4x4* matrix,
        const Point3& point,
        double w);
    static InteractionEventKind GetEventKind(unsigned long eventId);
    static void OnInput(
        vtkObject* caller,
        unsigned long eventId,
        void* clientData,
        void* callData);

    bool SetSlice(MeasureView view);
    void SetCamera(MeasureView view);
    bool AttachInput();
    void DetachInput();
    void SendInput(vtkObject* caller, unsigned long eventId);

    QPointer<QVTKOpenGLNativeWidget> m_widget;
    TrustedImageSnapshot m_image;
    vtkSmartPointer<vtkGenericOpenGLRenderWindow> m_renderWindow;
    vtkSmartPointer<vtkRenderer> m_renderer;
    vtkSmartPointer<vtkImageSliceMapper> m_sliceMapper;
    vtkSmartPointer<vtkImageSlice> m_slice;
    vtkSmartPointer<vtkMatrix4x4> m_modelToWorld;
    vtkSmartPointer<vtkMatrix4x4> m_worldToModel;
    vtkSmartPointer<vtkCallbackCommand> m_inputCommand;
    std::vector<unsigned long> m_inputTags;
    InputCallback m_inputCallback;
    std::array<double, 3> m_cursorWorld{};
    HostWindowLevelParams m_windowLevel;
    MeasureView m_view = MeasureView::Axial;
};

} // namespace measure
