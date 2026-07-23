#pragma once

#include "measure/MeasurementTypes.h"

#include <optional>
#include <string>
#include <vector>

namespace measure {

using MeasurementComputeFunction = std::optional<MeasurementResult> (*)(
    const std::vector<Point3>& points,
    const MeasurementPlane& plane);

using MeasurementPathFunction = std::vector<Point3> (*)(
    const std::vector<Point3>& points,
    const MeasurementPlane& plane,
    const MeasurementResult* result);

struct MeasurementToolDefinition {
    MeasureTool tool = MeasureTool::None;
    int requiredPointCount = 0;
    const char* beginMessage = "";
    const char* invalidError = "";
    const char* invalidStatus = "";
    MeasurementComputeFunction compute = nullptr;
    MeasurementPathFunction buildPath = nullptr;
};

const MeasurementToolDefinition& GetMeasurementToolDefinition(MeasureTool tool);
std::string FormatMeasurementLabel(
    std::uint64_t id,
    const MeasurementResult& result);
std::string FormatCompletedMeasurement(const MeasurementResult& result);

} // namespace measure
