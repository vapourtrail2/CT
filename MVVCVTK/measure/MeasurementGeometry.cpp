#include "measure/MeasurementGeometry.h"

#include <algorithm>
#include <cmath>

namespace measure::geometry {
namespace {
constexpr double kPi = 3.14159265358979323846;

struct Circle2D {
    double centerU = 0.0;
    double centerV = 0.0;
    double radius = 0.0;
};

std::array<double, 2> ToPlane(const Point3& point, const MeasurementPlane& plane)
{
    const Point3 relative = Subtract(point, plane.origin);
    return { Dot(relative, plane.u), Dot(relative, plane.v) };
}

std::optional<Circle2D> Circumcircle2D(
    const std::array<double, 2>& a,
    const std::array<double, 2>& b,
    const std::array<double, 2>& c,
    double tolerance)
{
    const double determinant = 2.0 * (
        a[0] * (b[1] - c[1])
        + b[0] * (c[1] - a[1])
        + c[0] * (a[1] - b[1]));

    const double scale = std::max({
        1.0,
        std::hypot(a[0] - b[0], a[1] - b[1]),
        std::hypot(a[0] - c[0], a[1] - c[1]),
        std::hypot(b[0] - c[0], b[1] - c[1]) });
    if (std::abs(determinant) <= tolerance * scale * scale) {
        return std::nullopt;
    }

    const double a2 = a[0] * a[0] + a[1] * a[1];
    const double b2 = b[0] * b[0] + b[1] * b[1];
    const double c2 = c[0] * c[0] + c[1] * c[1];

    Circle2D circle;
    circle.centerU = (
        a2 * (b[1] - c[1])
        + b2 * (c[1] - a[1])
        + c2 * (a[1] - b[1])) / determinant;
    circle.centerV = (
        a2 * (c[0] - b[0])
        + b2 * (a[0] - c[0])
        + c2 * (b[0] - a[0])) / determinant;
    circle.radius = std::hypot(a[0] - circle.centerU, a[1] - circle.centerV);
    if (!std::isfinite(circle.radius) || circle.radius <= tolerance) {
        return std::nullopt;
    }
    return circle;
}

double NormalizePositive(double angle)
{
    const double full = 2.0 * kPi;
    angle = std::fmod(angle, full);
    return angle < 0.0 ? angle + full : angle;
}
}

Point3 Add(const Point3& a, const Point3& b)
{
    return { a[0] + b[0], a[1] + b[1], a[2] + b[2] };
}

Point3 Subtract(const Point3& a, const Point3& b)
{
    return { a[0] - b[0], a[1] - b[1], a[2] - b[2] };
}

Point3 Scale(const Point3& value, double scale)
{
    return { value[0] * scale, value[1] * scale, value[2] * scale };
}

double Dot(const Point3& a, const Point3& b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

Point3 Cross(const Point3& a, const Point3& b)
{
    return {
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0]
    };
}

double Norm(const Point3& value)
{
    return std::sqrt(Dot(value, value));
}

std::optional<Point3> Normalized(const Point3& value)
{
    const double length = Norm(value);
    if (!std::isfinite(length) || length <= 1e-12) {
        return std::nullopt;
    }
    return Scale(value, 1.0 / length);
}

std::optional<LineResult> ComputeLine(const std::vector<Point3>& points)
{
    if (points.size() != 2) {
        return std::nullopt;
    }
    const double length = Norm(Subtract(points[1], points[0]));
    if (!std::isfinite(length) || length <= 1e-12) {
        return std::nullopt;
    }
    return LineResult{ length };
}

std::optional<CircleResult> ComputeCircle(
    const std::vector<Point3>& points,
    const MeasurementPlane& plane)
{
    if (points.size() != 3) {
        return std::nullopt;
    }
    const auto circle = Circumcircle2D(
        ToPlane(points[0], plane),
        ToPlane(points[1], plane),
        ToPlane(points[2], plane),
        std::max(plane.tolerance, 1e-9));
    if (!circle) {
        return std::nullopt;
    }

    CircleResult result;
    result.center = Add(
        plane.origin,
        Add(Scale(plane.u, circle->centerU), Scale(plane.v, circle->centerV)));
    result.radius = circle->radius;
    result.diameter = 2.0 * circle->radius;
    result.circumference = 2.0 * kPi * circle->radius;
    return result;
}

std::optional<ArcResult> ComputeArc(
    const std::vector<Point3>& points,
    const MeasurementPlane& plane)
{
    const auto circle = ComputeCircle(points, plane);
    if (!circle) {
        return std::nullopt;
    }

    const auto center2 = ToPlane(circle->center, plane);
    auto angleOf = [&](const Point3& point) {
        const auto p = ToPlane(point, plane);
        return std::atan2(p[1] - center2[1], p[0] - center2[0]);
    };

    const double start = angleOf(points[0]);
    const double through = angleOf(points[1]);
    const double end = angleOf(points[2]);
    const double ccwEnd = NormalizePositive(end - start);
    const double ccwThrough = NormalizePositive(through - start);

    double sweep = ccwEnd;
    if (ccwThrough > ccwEnd + 1e-9) {
        sweep = ccwEnd - 2.0 * kPi;
    }
    if (std::abs(sweep) <= 1e-9) {
        return std::nullopt;
    }

    ArcResult result;
    result.center = circle->center;
    result.radius = circle->radius;
    result.radiusAngle = sweep;
    result.length = circle->radius * std::abs(sweep);
    return result;
}

Point3 CirclePoint(
    const Point3& center,
    const MeasurementPlane& plane,
    double radius,
    double angleRadians)
{
    return Add(
        center,
        Add(
            Scale(plane.u, radius * std::cos(angleRadians)),
            Scale(plane.v, radius * std::sin(angleRadians))));
}

} // namespace measure::geometry
