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

enum class EdgeCaptureShape {
    Line,
    Circle
};

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
    void SetShape(EdgeCaptureShape shape);
    void SetView(MeasureView view);
    void SetStatusCallback(StatusCallback callback);
    void Refresh();

private:
    // 圆环以图像索引为单位；每个视图独立保存，切换工具不覆盖直线框。
    struct CircleState {
        bool initialized = false;
        int fixedIndex = 0;
        double centerU = 0.0;
        double centerV = 0.0;
        double innerRadius = 0.0;
        double outerRadius = 0.0;
        std::optional<ZcMeasuredCircle> result;
    };

    enum class CircleDragMode { None, Move, InnerRadius, OuterRadius };
    bool EnsureDefaultCircle();
    InteractionResult SendCircle(const InteractionEvent& event);
    void RefreshCircle();
    bool RunCircleMeasurement();

    struct ViewState { // 记录这张二维切片上的抓边框(矩形)
        bool initialized = false;
        int fixedIndex = 0;
        double minU = 0.0;
        double maxU = 0.0;
        double minV = 0.0;
        double maxV = 0.0;//Z这一层，初始化 true x最大最小和y最小最大
        std::optional<ZcMeasuredLine> result;//抓到的直线 没有为空
    };

    struct ImageGeometry {//在三维体中，取哪个二维切片
        int extent[6]{};// 三维图像的索引范围 [0][1] X最小最大索引 .. etc
        int fixedAxis = 2;//固定Z轴
        int fixedIndex = 0;//Z轴索引
        int uAxis = 0;//x,y平面 如固定住 ，就是只移动这个两个轴 框只改变这俩值
        int vAxis = 1;//
        int width = 0;// 二维切片宽度，单位：像素
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
    EdgeCaptureShape m_shape = EdgeCaptureShape::Line;
    CircleDragMode m_circleDragMode = CircleDragMode::None;
    CircleState m_circleDragStart;
    std::array<CircleState, 3> m_circleStates;
    bool m_consumingLeftButton = false;
    DragMode m_dragMode = DragMode::None;
    bool m_resizeUHigh = false;
    bool m_resizeVHigh = false;
    double m_pressU = 0.0;
    double m_pressV = 0.0;
    ViewState m_dragStart;
    std::array<ViewState, 3> m_viewStates;//每个视图各保留一份矩形状态 CurrentState根据当前视图取一个状态
    std::vector<vtkSmartPointer<vtkProp>> m_props;
    StatusCallback m_statusCallback;
    ZcEdgeAlgorithm m_algorithm;
};

} // namespace measure
