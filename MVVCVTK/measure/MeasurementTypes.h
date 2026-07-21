#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

namespace measure {

using Point3 = std::array<double, 3>;

enum class MeasureTool {
    None,
    Line,
    Circle3Point,
    Arc3Point
};

enum class MeasureView {
    Axial,
    Coronal,
    Sagittal
};

struct MeasureRequest {
    MeasureTool tool = MeasureTool::Line;
    MeasureView view = MeasureView::Axial;
};

struct MeasurementPlane {
    Point3 origin{ 0.0, 0.0, 0.0 };
    Point3 normal{ 0.0, 0.0, 1.0 };
    Point3 u{ 1.0, 0.0, 0.0 };
    Point3 v{ 0.0, 1.0, 0.0 };
    double tolerance = 1e-6;
    double sliceTolerance = 0.5;
};

struct LineResult {
    double length = 0.0;
};

struct CircleResult {
    Point3 center{ 0.0, 0.0, 0.0 };
    double radius = 0.0;
    double diameter = 0.0;
    double circumference = 0.0;
};

struct ArcResult {
    Point3 center{ 0.0, 0.0, 0.0 };
    double radius = 0.0;
    double sweepRadians = 0.0;
    double length = 0.0;
};

using MeasurementResult = std::variant<LineResult, CircleResult, ArcResult>;

struct MeasurementEntity {
    std::uint64_t id = 0;
    MeasureTool type = MeasureTool::None;
    MeasureView sourceView = MeasureView::Axial;
    MeasurementPlane plane;
    std::vector<Point3> physicalPoints;
    MeasurementResult result = LineResult{};
};

struct MeasurementDraft {
    MeasureRequest request;
    std::optional<MeasurementPlane> plane;
    std::vector<Point3> physicalPoints;
    std::optional<Point3> previewPoint;
};

} // namespace measure
