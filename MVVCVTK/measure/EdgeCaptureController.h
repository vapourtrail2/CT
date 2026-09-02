#pragma once

#include "MVVCVTK/SPI/Interaction/InteractionTypes.h"
#include "measure/MeasurementTypes.h"
#include "measure/ZcEdgeAlgorithm.h"

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <vtkSmartPointer.h>

class vtkProp;
class vtkRenderer;

namespace measure {

class MeasureViewAdapter;

class EdgeCaptureController final {
public:
    using StatusCallback = std::function<void(const std::string&)>;

    EdgeCaptureController(
        MeasureViewAdapter* adapter,
        vtkRenderer* renderer,
        MeasureView view);
    ~EdgeCaptureController();

    InteractionResult Send(const InteractionEvent& event);

    void SetEnabled(bool enabled);
    bool IsEnabled() const;
    void SetView(MeasureView view);
    void SetStatusCallback(StatusCallback callback);
    void Refresh();

private:
    struct ViewState {
        bool initialized = false;
        int fixedIndex = 0;
        double minU = 0.0;
        double maxU = 0.0;
        double minV = 0.0;
        double maxV = 0.0;
        std::optional<ZcMeasuredLine> result;
    };

    struct ImageGeometry {
        int extent[6]{};
        int fixedAxis = 2;
        int uAxis = 0;
        int vAxis = 1;
        int fixedIndex = 0;
        int width = 0;
        int height = 0;
    };

    enum class DragMode {
        None,
        Move,
        Resize
    };

    ViewState& CurrentState();
    const ViewState& CurrentState() const;
    bool GetImageGeometry(ImageGeometry& geometry) const;
    bool EnsureDefaultRoi();
    std::optional<std::array<double, 3>> DisplayToContinuousIndex(int x, int y) const;
    Point3 IndexToPhysical(double u, double v, int fixedIndex) const;
    Point3 PhysicalToWorld(const Point3& physical) const;
    std::array<double, 2> IndexToDisplay(double u, double v, int fixedIndex) const;
    bool BuildGraySlice(const ImageGeometry& geometry, ZcGrayImage& gray) const;
    bool RunMeasurement();
    void AddPath(
        const std::vector<Point3>& worldPath,
        double red,
        double green,
        double blue,
        double width);
    void AddHandles(const std::vector<Point3>& worldPoints);
    void RemoveProps();
    void RequestRender();
    void Report(const std::string& message) const;

    MeasureViewAdapter* m_adapter = nullptr;
    vtkRenderer* m_renderer = nullptr;
    MeasureView m_view = MeasureView::Axial;
    bool m_enabled = false;
    bool m_consumingLeftButton = false;
    DragMode m_dragMode = DragMode::None;
    bool m_resizeUHigh = false;
    bool m_resizeVHigh = false;
    double m_pressU = 0.0;
    double m_pressV = 0.0;
    ViewState m_dragStart;
    std::array<ViewState, 3> m_viewStates;
    std::vector<vtkSmartPointer<vtkProp>> m_props;
    StatusCallback m_statusCallback;
    ZcEdgeAlgorithm m_algorithm;
};

} // namespace measure
