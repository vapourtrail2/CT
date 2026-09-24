#include "measure/EdgeCaptureController.h"

#include "measure/MeasureViewAdapter.h"
#include "measure/MeasurementGeometry.h"
#include "measure/MeasurementView.h"

#include <vtkActor.h>
#include <vtkCellArray.h>
#include <vtkImageData.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>

namespace measure {
namespace {

std::size_t ViewIndex(MeasureView view)
{
    return static_cast<std::size_t>(view);
}

void SetIndexAxes(MeasureView view, int& fixedAxis, int& uAxis, int& vAxis)
{
    switch (view) {
    case MeasureView::Axial:
        fixedAxis = 2;
        uAxis = 0;
        vAxis = 1;
        break;
    case MeasureView::Coronal:
        fixedAxis = 1;
        uAxis = 0;
        vAxis = 2;
        break;
    case MeasureView::Sagittal:
        fixedAxis = 0;
        uAxis = 1;
        vAxis = 2;
        break;
    }
}

double Clamp(double value, double minimum, double maximum)
{
    return std::max(minimum, std::min(value, maximum));
}

} 

EdgeCaptureController::EdgeCaptureController(
    MeasureViewAdapter* adapter,
    vtkRenderer* renderer,
    MeasureView view)
    : m_adapter(adapter)
    , m_renderer(renderer)
    , m_view(view)
{
}

EdgeCaptureController::~EdgeCaptureController()
{
    RemoveProps();
}

InteractionResult EdgeCaptureController::Send(const InteractionEvent& event)
{
    if (!m_enabled || !m_renderer || !m_adapter) {
        return {};
    }

    if (m_shape != EdgeCaptureShape::Line) {
        return SendCircle(event);
    }

    if (event.eventKind == InteractionEventKind::PrimaryPress) {
        m_consumingLeftButton = true;
        m_dragMode = DragMode::None;
        if (!EnsureDefaultRoi()) {
            return { true, true };
        }

        const auto index = DisplayToContinuousIndex(event.x, event.y);
        if (!index) {
            return { true, true };
        }

        ImageGeometry geometry;
        if (!GetImageGeometry(geometry)) {
            return { true, true };
        }
        const double u = (*index)[geometry.uAxis];
        const double v = (*index)[geometry.vAxis];
        auto& state = CurrentState();

        constexpr double handleRadius = 14.0;
        constexpr double handleRadiusSquared = handleRadius * handleRadius;
        const std::array<std::array<double, 2>, 4> corners = {
            IndexToDisplay(state.minU, state.minV, state.fixedIndex),
            IndexToDisplay(state.maxU, state.minV, state.fixedIndex),
            IndexToDisplay(state.maxU, state.maxV, state.fixedIndex),
            IndexToDisplay(state.minU, state.maxV, state.fixedIndex)
        };
        for (std::size_t corner = 0; corner < corners.size(); ++corner) {
            const double dx = corners[corner][0] - event.x;
            const double dy = corners[corner][1] - event.y;
            if (dx * dx + dy * dy <= handleRadiusSquared) {
                m_dragMode = DragMode::Resize;
                m_resizeUHigh = corner == 1 || corner == 2;
                m_resizeVHigh = corner >= 2;
                break;
            }
        }

        if (m_dragMode == DragMode::None
            && u >= state.minU && u <= state.maxU
            && v >= state.minV && v <= state.maxV) {
            m_dragMode = DragMode::Move;
        }

        // 记录按下时的位置和矩形状态
        if (m_dragMode != DragMode::None) {
            m_pressU = u;
            m_pressV = v;
            m_dragStart = state;
            state.result.reset();
            Refresh();
        }
        return { true, true };
    }

    //拖动
    if (event.eventKind == InteractionEventKind::PointerMove
        && m_consumingLeftButton
        && m_dragMode != DragMode::None) {
        const auto index = DisplayToContinuousIndex(event.x, event.y);
        ImageGeometry geometry;
        if (!index || !GetImageGeometry(geometry)) {
            return { true, true };
        }

        const double uMinimum = geometry.extent[2 * geometry.uAxis];
        const double uMaximum = geometry.extent[2 * geometry.uAxis + 1];
        const double vMinimum = geometry.extent[2 * geometry.vAxis];
        const double vMaximum = geometry.extent[2 * geometry.vAxis + 1];
        const double u = (*index)[geometry.uAxis];
        const double v = (*index)[geometry.vAxis];
        auto& state = CurrentState();

        if (m_dragMode == DragMode::Move) {
            const double width = m_dragStart.maxU - m_dragStart.minU;
            const double height = m_dragStart.maxV - m_dragStart.minV;
            const double newMinU = Clamp(
                m_dragStart.minU + u - m_pressU,
                uMinimum,
                uMaximum - width);
            const double newMinV = Clamp(
                m_dragStart.minV + v - m_pressV,
                vMinimum,
                vMaximum - height);
            state.minU = newMinU;
            state.maxU = newMinU + width;
            state.minV = newMinV;
            state.maxV = newMinV + height;
        }
        else {
            constexpr double minimumSize = 4.0;
            if (m_resizeUHigh) {
                state.maxU = Clamp(u, state.minU + minimumSize, uMaximum);
            }
            else {
                state.minU = Clamp(u, uMinimum, state.maxU - minimumSize);
            }
            if (m_resizeVHigh) {
                state.maxV = Clamp(v, state.minV + minimumSize, vMaximum);
            }
            else {
                state.minV = Clamp(v, vMinimum, state.maxV - minimumSize);
            }
        }
        state.result.reset();
        Refresh();
        return { true, true };
    }

    //松开
    if (event.eventKind == InteractionEventKind::PrimaryRelease
        && m_consumingLeftButton) {
        const bool shouldMeasure = m_dragMode != DragMode::None;
        m_consumingLeftButton = false;
        m_dragMode = DragMode::None;
        if (shouldMeasure) {
            RunMeasurement();
        }
        return { true, true };
    }

    return {};
}

void EdgeCaptureController::SetEnabled(bool enabled)
{
    m_enabled = enabled;
    m_consumingLeftButton = false;
    m_dragMode = DragMode::None;
    m_circleDragMode = CircleDragMode::None;
    if (enabled && m_shape != EdgeCaptureShape::Line) {
        if (EnsureDefaultCircle()) {
            Report(m_shape == EdgeCaptureShape::Arc ? "拖动中心移动弧框，拖动端点调整弧度、内外手柄调整半径，松开抓弧。" : "拖动圆心移动圆环，拖动内外圆控制点调整半径，松开鼠标抓圆。");
        }
    }
    else if (enabled && EnsureDefaultRoi()) {
        Report("拖动矩形或四角控制点，松开鼠标后执行抓边。");
    }
    Refresh();
}

bool EdgeCaptureController::IsEnabled() const
{
    return m_enabled;
}

void EdgeCaptureController::SetShape(EdgeCaptureShape shape)
{
    if (m_shape == shape) {
        return;
    }
    m_shape = shape;
    m_consumingLeftButton = false;
    m_dragMode = DragMode::None;
    m_circleDragMode = CircleDragMode::None;
    if (m_enabled) {
        SetEnabled(true);
    }
}

void EdgeCaptureController::SetView(MeasureView view)
{
    m_view = view;
    m_consumingLeftButton = false;
    m_dragMode = DragMode::None;
    m_circleDragMode = CircleDragMode::None;
    if (m_enabled) {
        if (m_shape != EdgeCaptureShape::Line) {
            EnsureDefaultCircle();
        }
        else {
            EnsureDefaultRoi();
        }
    }
    Refresh();
}

void EdgeCaptureController::SetStatusCallback(StatusCallback callback)
{
    m_statusCallback = std::move(callback);
}

void EdgeCaptureController::Refresh()//根据数据画矩形
{
    RemoveProps();
    if (!m_enabled) {
        RequestRender();
        return;
    }
    if (m_shape != EdgeCaptureShape::Line) {
        RefreshCircle();
        RequestRender();
        return;
    }
    const auto& state = CurrentState();
    if (!state.initialized || !m_renderer || !m_adapter) {
        RequestRender();
        return;
    }

    const std::vector<Point3> physicalCorners = {
        IndexToPhysical(state.minU, state.minV, state.fixedIndex),
        IndexToPhysical(state.maxU, state.minV, state.fixedIndex),
        IndexToPhysical(state.maxU, state.maxV, state.fixedIndex),
        IndexToPhysical(state.minU, state.maxV, state.fixedIndex)
    };
    std::vector<Point3> worldCorners;
    worldCorners.reserve(5);
    for (const auto& point : physicalCorners) {
        worldCorners.push_back(PhysicalToWorld(point));
    }
    worldCorners.push_back(worldCorners.front());
    AddPath(worldCorners, 1.0, 0.75, 0.05, 2.0);//画黄色矩形

    std::vector<Point3> handlePoints(worldCorners.begin(), worldCorners.end() - 1);
    AddHandles(handlePoints);//画四个角的控制点

    if (state.result) {//有结果了就画绿线
        ImageGeometry geometry;
        if (GetImageGeometry(geometry)) {
            const auto toIndex = [&geometry](double x, double y) {
                const double u = geometry.extent[2 * geometry.uAxis] + x;
                const double v = geometry.extent[2 * geometry.vAxis + 1] - y;
                return std::array<double, 2>{ u, v };
            };
            const auto p1 = toIndex(state.result->x1, state.result->y1);
            const auto p2 = toIndex(state.result->x2, state.result->y2);
            AddPath({
                PhysicalToWorld(IndexToPhysical(p1[0], p1[1], state.fixedIndex)),
                PhysicalToWorld(IndexToPhysical(p2[0], p2[1], state.fixedIndex))
                }, 0.1, 1.0, 0.35, 3.0);
        }
    }
    RequestRender();
}

EdgeCaptureController::ViewState& EdgeCaptureController::CurrentState()
{
    return m_viewStates[ViewIndex(m_view)];
}

const EdgeCaptureController::ViewState& EdgeCaptureController::CurrentState() const
{
    return m_viewStates[ViewIndex(m_view)];
}

bool EdgeCaptureController::GetImageGeometry(ImageGeometry& geometry) const
{
    if (!m_adapter) {
        return false;
    }
    const auto snapshot = m_adapter->GetImageSnapshot();
    if (!snapshot || !snapshot->image) {
        return false;
    }
    snapshot->image->GetExtent(geometry.extent);
    SetIndexAxes(m_view, geometry.fixedAxis, geometry.uAxis, geometry.vAxis);
    geometry.width = geometry.extent[2 * geometry.uAxis + 1]
        - geometry.extent[2 * geometry.uAxis] + 1;
    geometry.height = geometry.extent[2 * geometry.vAxis + 1]
        - geometry.extent[2 * geometry.vAxis] + 1;

    const auto cursor = m_adapter->GetCursorWorld();
    const auto physical = m_adapter->GetModelPoint({
        cursor[0], cursor[1], cursor[2]
    });
    double index[3] = { 0.0, 0.0, 0.0 };
    snapshot->image->TransformPhysicalPointToContinuousIndex(
        physical.data(), index);
    geometry.fixedIndex = static_cast<int>(std::lround(index[geometry.fixedAxis]));
    geometry.fixedIndex = std::max(
        geometry.extent[2 * geometry.fixedAxis],
        std::min(
            geometry.fixedIndex,
            geometry.extent[2 * geometry.fixedAxis + 1]));
    return geometry.width > 1 && geometry.height > 1;
}

bool EdgeCaptureController::EnsureDefaultRoi()//准备矩形数据
{
    auto& state = CurrentState();//矩形数据存在控制器自己的ViewState里面
    if (state.initialized) {
        return true;
    }
    ImageGeometry geometry;
    if (!GetImageGeometry(geometry)) {
        Report("无法建立抓边矩形：当前切片不可用。");
        return false;
    }
    const double uMinimum = geometry.extent[2 * geometry.uAxis];
    const double uMaximum = geometry.extent[2 * geometry.uAxis + 1];
    const double vMinimum = geometry.extent[2 * geometry.vAxis];
    const double vMaximum = geometry.extent[2 * geometry.vAxis + 1];
    const double uSpan = uMaximum - uMinimum;
    const double vSpan = vMaximum - vMinimum;
    state.fixedIndex = geometry.fixedIndex;
    state.minU = uMinimum + uSpan * 0.30;
    state.maxU = uMinimum + uSpan * 0.70;
    state.minV = vMinimum + vSpan * 0.30;
    state.maxV = vMinimum + vSpan * 0.70;
    state.initialized = true;
    return true;
}

std::optional<std::array<double, 3>>
EdgeCaptureController::DisplayToContinuousIndex(int x, int y) const
{
    if (!m_renderer || !m_adapter) {
        return std::nullopt;
    }
    const auto physical = m_adapter->GetDisplayModel(x, y);
    if (!physical) {
        return std::nullopt;
    }
    double index[3] = { 0.0, 0.0, 0.0 };
    const auto snapshot = m_adapter->GetImageSnapshot();
    if (!snapshot || !snapshot->image) {
        return std::nullopt;
    }
    snapshot->image->TransformPhysicalPointToContinuousIndex(
        physical->data(), index);

    int extent[6];
    snapshot->image->GetExtent(extent);
    constexpr double margin = 0.5;
    for (int axis = 0; axis < 3; ++axis) {
        if (index[axis] < extent[2 * axis] - margin
            || index[axis] > extent[2 * axis + 1] + margin) {
            return std::nullopt;
        }
    }
    return std::array<double, 3>{ index[0], index[1], index[2] };
}

Point3 EdgeCaptureController::IndexToPhysical(
    double u,
    double v,
    int fixedIndex) const
{
    ImageGeometry geometry;
    if (!GetImageGeometry(geometry)) {
        return {};
    }
    double index[3] = { 0.0, 0.0, 0.0 };
    index[geometry.fixedAxis] = fixedIndex;
    index[geometry.uAxis] = u;
    index[geometry.vAxis] = v;
    double physical[3] = { 0.0, 0.0, 0.0 };
    const auto snapshot = m_adapter->GetImageSnapshot();
    snapshot->image->TransformContinuousIndexToPhysicalPoint(index, physical);
    return { physical[0], physical[1], physical[2] };
}

Point3 EdgeCaptureController::PhysicalToWorld(const Point3& physical) const
{
    const Point3 target = m_adapter->GetWorldPoint(physical);
    const Point3 normal = m_adapter->GetWorldVector(
        GetSliceViewDescriptor(m_view).normal);
    constexpr double safeOffset = 0.02;
    return {
        target[0] + normal[0] * safeOffset,
        target[1] + normal[1] * safeOffset,
        target[2] + normal[2] * safeOffset
    };
}

std::array<double, 2> EdgeCaptureController::IndexToDisplay(
    double u,
    double v,
    int fixedIndex) const
{
    const Point3 world = PhysicalToWorld(IndexToPhysical(u, v, fixedIndex));
    m_renderer->SetWorldPoint(world[0], world[1], world[2], 1.0);
    m_renderer->WorldToDisplay();
    const double* display = m_renderer->GetDisplayPoint();
    return { display[0], display[1] };
}

bool EdgeCaptureController::BuildGraySlice(
    const ImageGeometry& geometry,
    ZcGrayImage& gray) const
{
    const auto snapshot = m_adapter->GetImageSnapshot();
    if (!snapshot || !snapshot->image) {
        return false;
    }
    gray.width = geometry.width;
    gray.height = geometry.height;
    gray.widthStep = (gray.width + 3) & ~3;
    gray.pixels.assign(
        static_cast<std::size_t>(gray.widthStep) * gray.height,
        0);

    const auto windowLevel = m_adapter->GetWindowLevel();
    const double safeWindow = std::max(windowLevel.windowWidth, 1e-6);
    const double windowMinimum = windowLevel.windowCenter - safeWindow * 0.5;
    for (int row = 0; row < gray.height; ++row) {
        // IplImage 原点在左上；VTK 当前视图的 +V 方向在屏幕向上。
        const int vIndex = geometry.extent[2 * geometry.vAxis + 1] - row;
        for (int column = 0; column < gray.width; ++column) {
            int index[3] = { 0, 0, 0 };
            index[geometry.fixedAxis] = geometry.fixedIndex;
            index[geometry.uAxis] = geometry.extent[2 * geometry.uAxis] + column;
            index[geometry.vAxis] = vIndex;
            const bool valid = !snapshot->validityMask
                || snapshot->validityMask->GetScalarComponentAsDouble(
                    index[0], index[1], index[2], 0) != 0.0;
            if (!valid) {
                continue;
            }
            const double value = snapshot->image->GetScalarComponentAsDouble(
                index[0], index[1], index[2], 0);
            const double normalized = Clamp(
                (value - windowMinimum) / safeWindow,
                0.0,
                1.0);
            gray.pixels[static_cast<std::size_t>(row) * gray.widthStep + column]
                = static_cast<std::uint8_t>(normalized * 255.0 + 0.5);
        }
    }
    return true;
}

bool EdgeCaptureController::RunMeasurement()
{
    //当前图像
    ImageGeometry geometry;
    auto& state = CurrentState();
    if (!state.initialized || !GetImageGeometry(geometry)) {
        Report("抓边失败：当前切片不可用。");
        return false;
    }
    geometry.fixedIndex = state.fixedIndex;

    // 将当前VTK切片转换成8位灰度图
    ZcGrayImage gray;
    if (!BuildGraySlice(geometry, gray)) {
        Report("抓边失败：无法生成 8 位灰度切片。");
        return false;
    }
    // 将矩形转换为DLL使用的图像像素坐标
    ZcRectFrame frame;
    frame.startX = state.minU - geometry.extent[2 * geometry.uAxis];
    frame.startY = geometry.extent[2 * geometry.vAxis + 1] - state.maxV;
    frame.width = state.maxU - state.minU;
    frame.height = state.maxV - state.minV;
    frame.cosAngle = 1.0;
    frame.sinAngle = 0.0;

    //  调用自己的 DLL 封装类
    ZcMeasuredLine line;
    std::string error;
    if (!m_algorithm.MeasureLineByRect(gray, frame, line, error)) {
        state.result.reset();
        Report("抓边失败：" + error);
        Refresh();
        return false;
    }

    const auto finite = [](double value) { return std::isfinite(value); };
    const double tolerance = 2.0;
    const auto pointInsideFrame = [&frame, tolerance](double x, double y) {
        return x >= frame.startX - tolerance
            && x <= frame.startX + frame.width + tolerance
            && y >= frame.startY - tolerance
            && y <= frame.startY + frame.height + tolerance;
    };
    if (!finite(line.x1) || !finite(line.y1)
        || !finite(line.x2) || !finite(line.y2)
        || !pointInsideFrame(line.x1, line.y1)
        || !pointInsideFrame(line.x2, line.y2)) {
        state.result.reset();
        Report("DLL 已返回抓边点，但点坐标不在框选区域；请查看日志确认 ZC_GetMeasuredPoints 是否返回整图像素坐标。");
        Refresh();
        return false;
    }

    state.result = line;
    Report("抓边成功，共获得 " + std::to_string(line.measuredPointsCount) + " 个测量点。");
    Refresh();
    return true;
}

EdgeCaptureController::CircleState& EdgeCaptureController::CurrentCircleState()
{
    return m_shape == EdgeCaptureShape::Arc ? m_arcStates[ViewIndex(m_view)] : m_circleStates[ViewIndex(m_view)];
}

bool EdgeCaptureController::EnsureDefaultCircle()
{
    auto& circle = CurrentCircleState();
    if (circle.initialized) {
        return true;
    }
    ImageGeometry geometry;
    if (!GetImageGeometry(geometry)
        || std::min(geometry.width, geometry.height) < 13) {
        Report("无法建立抓圆框：当前切片不可用或尺寸过小。");
        return false;
    }
    const double uMin = geometry.extent[2 * geometry.uAxis];
    const double uMax = geometry.extent[2 * geometry.uAxis + 1];
    const double vMin = geometry.extent[2 * geometry.vAxis];
    const double vMax = geometry.extent[2 * geometry.vAxis + 1];
    circle.fixedIndex = geometry.fixedIndex;
    circle.centerU = (uMin + uMax) * 0.5;
    circle.centerV = (vMin + vMax) * 0.5;
    circle.outerRadius = std::max(6.0, std::min(uMax - uMin, vMax - vMin) * 0.22);
    circle.innerRadius = std::max(1.0,
        std::min(circle.outerRadius * 0.75, circle.outerRadius - 4.0));
    circle.initialized = true;
    return true;
}

InteractionResult EdgeCaptureController::SendCircle(const InteractionEvent& event)
{
    if (event.eventKind == InteractionEventKind::PrimaryRelease
        && m_consumingLeftButton) {
        const bool shouldMeasure = m_circleDragMode != CircleDragMode::None;
        m_consumingLeftButton = false;
        m_circleDragMode = CircleDragMode::None;
        if (shouldMeasure) {
            RunCircleMeasurement();
        }
        return { true, true };
    }

    const bool press = event.eventKind == InteractionEventKind::PrimaryPress;
    const bool drag = event.eventKind == InteractionEventKind::PointerMove
        && m_consumingLeftButton;
    if (!press && !drag) {
        return {};
    }
    if (press) {
        m_consumingLeftButton = true;
        m_circleDragMode = CircleDragMode::None;
    }
    if (!EnsureDefaultCircle()) {
        return { true, true };
    }
    const auto index = DisplayToContinuousIndex(event.x, event.y);
    ImageGeometry geometry;
    if (!index || !GetImageGeometry(geometry)) {
        return { true, true };
    }
    const double u = (*index)[geometry.uAxis];
    const double v = (*index)[geometry.vAxis];
    auto& circle = CurrentCircleState();

    if (press) {
        // 在屏幕坐标内命中控制点；缩放视图后仍保持相同的拾取距离。
        double nearestSquared = 14.0 * 14.0;
        const auto hit = [&](double handleU, double handleV, CircleDragMode mode) {
            const auto display = IndexToDisplay(handleU, handleV, circle.fixedIndex);
            const double dx = display[0] - event.x;
            const double dy = display[1] - event.y;
            const double distanceSquared = dx * dx + dy * dy;
            if (distanceSquared <= nearestSquared) {
                nearestSquared = distanceSquared;
                m_circleDragMode = mode;
            }
        };
        hit(circle.centerU, circle.centerV, CircleDragMode::Move);
        //鼠标控制点。圆和圆弧
        if (m_shape == EdgeCaptureShape::Arc) {
            const double mid = (circle.startAngle + circle.endAngle) * 0.5;
            const double radius = (circle.innerRadius + circle.outerRadius) * 0.5;
            hit(circle.centerU + radius * std::cos(circle.startAngle), circle.centerV - radius * std::sin(circle.startAngle), CircleDragMode::StartAngle);
            hit(circle.centerU + radius * std::cos(circle.endAngle), circle.centerV - radius * std::sin(circle.endAngle), CircleDragMode::EndAngle);
            hit(circle.centerU + circle.innerRadius * std::cos(mid), circle.centerV - circle.innerRadius * std::sin(mid), CircleDragMode::InnerRadius);
            hit(circle.centerU + circle.outerRadius * std::cos(mid), circle.centerV - circle.outerRadius * std::sin(mid), CircleDragMode::OuterRadius);
        }
        else for (int axis = 0; axis < 4; ++axis) {
            const double du = axis == 0 ? 1.0 : axis == 2 ? -1.0 : 0.0;
            const double dv = axis == 1 ? 1.0 : axis == 3 ? -1.0 : 0.0;
            hit(circle.centerU + du * circle.innerRadius,
                circle.centerV + dv * circle.innerRadius, CircleDragMode::InnerRadius);
            hit(circle.centerU + du * circle.outerRadius,
                circle.centerV + dv * circle.outerRadius, CircleDragMode::OuterRadius);
        }
        if (m_circleDragMode == CircleDragMode::None
            && std::hypot(u - circle.centerU, v - circle.centerV) <= circle.outerRadius) {
            m_circleDragMode = CircleDragMode::Move;
        }
        m_pressU = u;
        m_pressV = v;
        m_circleDragStart = circle;
        return { true, true };
    }

    if (m_circleDragMode == CircleDragMode::None) {
        return { true, true };
    }
    const double uMin = geometry.extent[2 * geometry.uAxis];
    const double uMax = geometry.extent[2 * geometry.uAxis + 1];
    const double vMin = geometry.extent[2 * geometry.vAxis];
    const double vMax = geometry.extent[2 * geometry.vAxis + 1];
    circle.arcResult.reset();
    if (m_circleDragMode == CircleDragMode::StartAngle || m_circleDragMode == CircleDragMode::EndAngle) {
        constexpr double pi = 3.141592653589793;
        const double a = std::atan2(circle.centerV - v, u - circle.centerU);
        const double before = std::atan2(circle.centerV - m_pressV, m_pressU - circle.centerU);
        const double delta = std::remainder(a - before, 2 * pi);
        if (m_circleDragMode == CircleDragMode::StartAngle)
            circle.startAngle = Clamp(m_circleDragStart.startAngle + delta, circle.endAngle - 2*pi + 0.05, circle.endAngle - 0.05);
        else
            circle.endAngle = Clamp(m_circleDragStart.endAngle + delta, circle.startAngle + 0.05, circle.startAngle + 2*pi - 0.05);
    }
    else if (m_circleDragMode == CircleDragMode::Move) {
        circle.result.reset();
        circle.centerU = Clamp(m_circleDragStart.centerU + u - m_pressU,
            uMin + circle.outerRadius, uMax - circle.outerRadius);
        circle.centerV = Clamp(m_circleDragStart.centerV + v - m_pressV,
            vMin + circle.outerRadius, vMax - circle.outerRadius);
    }
    else {
        const double delta = std::hypot(u - circle.centerU, v - circle.centerV)
            - std::hypot(m_pressU - circle.centerU, m_pressV - circle.centerV);
        circle.result.reset();
        if (m_circleDragMode == CircleDragMode::InnerRadius) {
            circle.innerRadius = Clamp(m_circleDragStart.innerRadius + delta,
                1.0, circle.outerRadius - 4.0);
        }
        else {
            const double maximum = std::min({ circle.centerU - uMin, uMax - circle.centerU,
                circle.centerV - vMin, vMax - circle.centerV });
            circle.outerRadius = Clamp(m_circleDragStart.outerRadius + delta,
                circle.innerRadius + 4.0, maximum);
        }
    }
    Refresh();
    return { true, true };
}

void EdgeCaptureController::RefreshCircle()
{
    const auto& circle = CurrentCircleState();
    if (!circle.initialized || !m_renderer || !m_adapter) {
        return;
    }
    const auto worldPoint = [&](double u, double v) {
        return PhysicalToWorld(IndexToPhysical(u, v, circle.fixedIndex));
    };
    if (m_shape == EdgeCaptureShape::Arc) {
        const auto polar = [&](double radius, double angle) {
            return worldPoint(circle.centerU + radius * std::cos(angle), circle.centerV - radius * std::sin(angle));
        };
        std::vector<Point3> handles{worldPoint(circle.centerU, circle.centerV)};
        const double mid = (circle.startAngle + circle.endAngle) * 0.5;
        for (double radius : {circle.innerRadius, circle.outerRadius}) {
            std::vector<Point3> path;
            for (int i=0; i<=180; ++i) path.push_back(polar(radius, circle.startAngle + (circle.endAngle-circle.startAngle)*i/180.0));
            AddPath(path, 1, 0.75, 0.05, 2);
            handles.push_back(polar(radius, mid));
        }
        for (double angle : {circle.startAngle, circle.endAngle}) {
            AddPath({polar(circle.innerRadius, angle), polar(circle.outerRadius, angle)}, 1, 0.75, 0.05, 2);
            handles.push_back(polar((circle.innerRadius+circle.outerRadius)*0.5, angle));
        }
        if (circle.arcResult) {
            ImageGeometry g;
            if (GetImageGeometry(g)) {
                std::vector<Point3> path;
                for (const auto& p : circle.arcResult->path)
                    path.push_back(worldPoint(g.extent[2*g.uAxis]+p[0], g.extent[2*g.vAxis+1]-p[1]));
                AddPath(path, 0.1, 1, 0.2, 2.5);
            }
        }
        AddHandles(handles);
        return;
    }
    std::vector<Point3> handles{ worldPoint(circle.centerU, circle.centerV) };
    constexpr int segments = 180;
    constexpr double twoPi = 6.28318530717958647692;
    for (const double radius : { circle.innerRadius, circle.outerRadius }) {
        std::vector<Point3> path;
        path.reserve(segments + 1);
        for (int i = 0; i < segments; ++i) {
            const double angle = twoPi * i / segments;
            path.push_back(worldPoint(circle.centerU + radius * std::cos(angle),
                circle.centerV + radius * std::sin(angle)));
        }
        path.push_back(path.front());
        AddPath(path, 1.0, 0.75, 0.05, 2.0);
        handles.push_back(worldPoint(circle.centerU + radius, circle.centerV));
        handles.push_back(worldPoint(circle.centerU - radius, circle.centerV));
        handles.push_back(worldPoint(circle.centerU, circle.centerV + radius));
        handles.push_back(worldPoint(circle.centerU, circle.centerV - radius));
    }
    if (circle.result) {
        ImageGeometry geometry;
        if (GetImageGeometry(geometry)) {
            const auto& result = *circle.result;
            const double centerU = geometry.extent[2 * geometry.uAxis] + result.x;
            const double centerV = geometry.extent[2 * geometry.vAxis + 1] - result.y;
            std::vector<Point3> path;
            path.reserve(segments + 1);
            for (int i = 0; i < segments; ++i) {
                const double angle = twoPi * i / segments;
                path.push_back(worldPoint(centerU + result.radius * std::cos(angle),
                    centerV + result.radius * std::sin(angle)));
            }
            path.push_back(path.front());
            AddPath(path, 0.1, 1.0, 0.2, 2.5);
        }
    }
    AddHandles(handles);
}

bool EdgeCaptureController::RunCircleMeasurement()
{
    auto& state = CurrentCircleState();
    state.result.reset();
    state.arcResult.reset();
    ImageGeometry geometry;
    if (!state.initialized || !GetImageGeometry(geometry)) {
        Report("抓圆失败：当前切片不可用。");
        Refresh();
        return false;
    }
    geometry.fixedIndex = state.fixedIndex;
    ZcGrayImage gray;
    if (!BuildGraySlice(geometry, gray)) {
        Report("抓圆失败：无法生成灰度切片。");
        Refresh();
        return false;
    }
    const ZcCircleRingFrame frame{
        state.centerU - geometry.extent[2 * geometry.uAxis],
        geometry.extent[2 * geometry.vAxis + 1] - state.centerV,
        state.innerRadius, state.outerRadius};
    if (m_shape == EdgeCaptureShape::Arc) {
        ZcArcRingFrame arcFrame;
        static_cast<ZcCircleRingFrame&>(arcFrame) = frame;
        arcFrame.startAngle = state.startAngle;
        arcFrame.endAngle = state.endAngle;
        ZcMeasuredArc arc;
        std::string error;
        if (!m_algorithm.MeasureArcByArcRing(gray, arcFrame, arc, error)) {
            Report("抓弧失败：" + error); Refresh(); return false;
        }
        state.arcResult = std::move(arc);
        Report("抓弧成功，测量点 " + std::to_string(state.arcResult->measuredPointsCount) + " 个。");
        Refresh(); return true;
    }
    ZcMeasuredCircle result;
    std::string error;
    if (!m_algorithm.MeasureCircleByCircleRing(gray, frame, result, error)) {
        Report("抓圆失败：" + error);
        Refresh();
        return false;
    }
    state.result = result;
    Report("抓圆成功：半径 " + std::to_string(result.radius) + " 像素，测量点 "
        + std::to_string(result.measuredPointsCount) + " 个。");
    Refresh();
    return true;
}

void EdgeCaptureController::AddPath(
    const std::vector<Point3>& worldPath,
    double red,
    double green,
    double blue,
    double width)
{
    if (!m_renderer || worldPath.size() < 2) {
        return;
    }
    auto points = vtkSmartPointer<vtkPoints>::New();
    auto lines = vtkSmartPointer<vtkCellArray>::New();
    for (const auto& point : worldPath) {
        points->InsertNextPoint(point.data());
    }
    lines->InsertNextCell(static_cast<int>(worldPath.size()));
    for (vtkIdType id = 0; id < static_cast<vtkIdType>(worldPath.size()); ++id) {
        lines->InsertCellPoint(id);
    }
    auto polyData = vtkSmartPointer<vtkPolyData>::New();
    polyData->SetPoints(points);
    polyData->SetLines(lines);
    auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputData(polyData);
    auto actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);
    actor->PickableOff();
    actor->GetProperty()->LightingOff();
    actor->GetProperty()->SetColor(red, green, blue);
    actor->GetProperty()->SetLineWidth(width);
    m_renderer->AddActor(actor);
    m_props.push_back(actor);
}

void EdgeCaptureController::AddHandles(const std::vector<Point3>& worldPoints)
{
    if (!m_renderer || worldPoints.empty()) {
        return;
    }
    auto points = vtkSmartPointer<vtkPoints>::New();
    auto vertices = vtkSmartPointer<vtkCellArray>::New();
    for (const auto& point : worldPoints) {
        const vtkIdType id = points->InsertNextPoint(point.data());
        vertices->InsertNextCell(1, &id);
    }
    auto polyData = vtkSmartPointer<vtkPolyData>::New();
    polyData->SetPoints(points);
    polyData->SetVerts(vertices);
    auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputData(polyData);
    auto actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);
    actor->PickableOff();
    actor->GetProperty()->LightingOff();
    actor->GetProperty()->SetPointSize(10.0);
    actor->GetProperty()->RenderPointsAsSpheresOn();
    actor->GetProperty()->SetColor(1.0, 0.75, 0.05);
    m_renderer->AddActor(actor);
    m_props.push_back(actor);
}

void EdgeCaptureController::RemoveProps()
{
    if (m_renderer) {
        for (const auto& prop : m_props) {
            m_renderer->RemoveViewProp(prop);
        }
    }
    m_props.clear();
}

void EdgeCaptureController::RequestRender()
{
    if (m_adapter) {
        m_adapter->SendRender();
    }
    if (m_renderer && m_renderer->GetRenderWindow()) {
        m_renderer->GetRenderWindow()->Render();
    }
}

void EdgeCaptureController::Report(const std::string& message) const
{
    if (m_statusCallback) {
        m_statusCallback(message);
    }
}

}
