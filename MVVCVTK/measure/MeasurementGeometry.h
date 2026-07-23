#pragma once

#include "measure/MeasurementTypes.h"
#include <optional>
#include <vector>

namespace measure::geometry {

    Point3 Add(const Point3& a, const Point3& b);
    Point3 Subtract(const Point3& a, const Point3& b);
    Point3 Scale(const Point3& value, double scale);
    double Dot(const Point3& a, const Point3& b);
    Point3 Cross(const Point3& a, const Point3& b);
    double Norm(const Point3& value);
    std::optional<Point3> Normalized(const Point3& value);

    std::optional<LineResult> ComputeLine(const std::vector<Point3>& points);
    std::optional<CircleResult> ComputeCircle(
        const std::vector<Point3>& points,
        const MeasurementPlane& plane);
    std::optional<ArcResult> ComputeArc(
        const std::vector<Point3>& points,
        const MeasurementPlane& plane);

    Point3 CirclePoint(
        const Point3& center,
        const MeasurementPlane& plane,
        double radius,
        double angleRadians);
} 
