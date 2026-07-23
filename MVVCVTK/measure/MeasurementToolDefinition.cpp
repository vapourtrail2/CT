#include "measure/MeasurementToolDefinition.h"

#include "measure/MeasurementGeometry.h"

#include <cmath>
#include <iomanip>
#include <sstream>
#include <type_traits>

namespace measure {
namespace {
constexpr double kPi = 3.14159265358979323846;

std::optional<MeasurementResult> ComputeLine(
    const std::vector<Point3>& points,
    const MeasurementPlane& plane)
{
    (void)plane;
    const auto result = geometry::ComputeLine(points);
    return result ? std::optional<MeasurementResult>(*result) : std::nullopt;
}

std::optional<MeasurementResult> ComputeCircle(
    const std::vector<Point3>& points,
    const MeasurementPlane& plane)
{
    const auto result = geometry::ComputeCircle(points, plane);
    return result ? std::optional<MeasurementResult>(*result) : std::nullopt;
}

std::optional<MeasurementResult> ComputeArc(
    const std::vector<Point3>& points,
    const MeasurementPlane& plane)
{
    const auto result = geometry::ComputeArc(points, plane);
    return result ? std::optional<MeasurementResult>(*result) : std::nullopt;
}

std::vector<Point3> BuildLinePath(
    const std::vector<Point3>& points,
    const MeasurementPlane& plane,
    const MeasurementResult* result)
{
    (void)plane;
    (void)result;
    return points;
}

std::vector<Point3> BuildCirclePath(
    const std::vector<Point3>& points,
    const MeasurementPlane& plane,
    const MeasurementResult* result)
{
    std::optional<CircleResult> computed;
    if (result) {
        if (const auto* circle = std::get_if<CircleResult>(result)) {
            computed = *circle;
        }
    }
    else if (points.size() == 3) {
        computed = geometry::ComputeCircle(points, plane);
    }

    if (!computed) {
        return points;
    }

    constexpr int samples = 128;
    std::vector<Point3> path;
    path.reserve(samples + 1);
    for (int i = 0; i <= samples; ++i) {
        path.push_back(geometry::CirclePoint(
            computed->center,
            plane,
            computed->radius,
            2.0 * kPi * static_cast<double>(i) / samples));
    }
    return path;
}

std::vector<Point3> BuildArcPath(
    const std::vector<Point3>& points,
    const MeasurementPlane& plane,
    const MeasurementResult* result)
{
    if (points.empty()) {
        return {};
    }

    std::optional<ArcResult> computed;
    if (result) {
        if (const auto* arc = std::get_if<ArcResult>(result)) {
            computed = *arc;
        }
    }
    else if (points.size() == 3) {
        computed = geometry::ComputeArc(points, plane);
    }

    if (!computed) {
        return points;
    }

    const Point3 relative = geometry::Subtract(points.front(), computed->center);
    const double start = std::atan2(
        geometry::Dot(relative, plane.v),
        geometry::Dot(relative, plane.u));

    constexpr int samples = 64;
    std::vector<Point3> path;
    path.reserve(samples + 1);
    for (int i = 0; i <= samples; ++i) {
        const double angle = start
            + computed->radiusAngle * static_cast<double>(i) / samples;
        path.push_back(geometry::CirclePoint(
            computed->center, plane, computed->radius, angle));
    }
    return path;
}

const MeasurementToolDefinition kNone{
    MeasureTool::None,
    0,
    false,
    "请选择测量工具。",
    "No measurement tool selected.",
    "请选择测量工具。",
    nullptr,
    nullptr
};

const MeasurementToolDefinition kLine{
    MeasureTool::Line,
    2,
    true,
    "直线：点击起点和终点，或者按住左键拖动。",
    "The two points are too close.",
    "两个点太近，请重新选择终点。",
    &ComputeLine,// 支持函数 
    &BuildLinePath// 绘制路径函数
};

const MeasurementToolDefinition kCircle{
    MeasureTool::Circle3Point,
    3,
    false,
    "圆：依次点击圆周上的三个点。",
    "The three points are collinear or too close.",
    "三个点过近或接近共线，请重新选择最后一个点。",
    &ComputeCircle,
    &BuildCirclePath
};

const MeasurementToolDefinition kArc{
    MeasureTool::Arc3Point,
    3,
    false,
    "圆弧：依次点击起点、经过点和终点。",
    "The three points are collinear or too close.",
    "三个点过近或接近共线，请重新选择最后一个点。",
    &ComputeArc,
    &BuildArcPath
};
}

const MeasurementToolDefinition& GetMeasurementToolDefinition(MeasureTool tool)
{
    switch (tool) {
    case MeasureTool::Line: return kLine;
    case MeasureTool::Circle3Point: return kCircle;
    case MeasureTool::Arc3Point: return kArc;
    case MeasureTool::None: return kNone;
    }
    return kNone;
}

std::string FormatMeasurementLabel(
    std::uint64_t id,
    const MeasurementResult& result)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(3) << "M" << id << " ";
    std::visit([&stream](const auto& value) {
        using Result = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<Result, LineResult>) {
            stream << "L=" << value.length;
        }
        else if constexpr (std::is_same_v<Result, CircleResult>) {
            stream << "D=" << value.diameter << " R=" << value.radius;
        }
        else {
            stream << "Arc=" << value.length
                << " R=" << value.radius
                << " A=" << std::abs(value.radiusAngle) * 180.0 / kPi << "deg";
        }
    }, result);
    return stream.str();
}

std::string FormatCompletedMeasurement(const MeasurementResult& result)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(3);
    std::visit([&stream](const auto& value) {
        using Result = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<Result, LineResult>) {
            stream << "完成：长度 = " << value.length << " mm";
        }
        else if constexpr (std::is_same_v<Result, CircleResult>) {
            stream << "完成：直径 = " << value.diameter
                << "，半径 = " << value.radius << " mm";
        }
        else {
            stream << "完成：弧长 = " << value.length
                << "，半径 = " << value.radius
                << "，圆心角 = "
                << std::abs(value.radiusAngle) * 180.0 / kPi << "°";
        }
    }, result);
    return stream.str();
}

} // namespace measure
