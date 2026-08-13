#pragma once

#include <array>
#include <cstdint>
#include <optional>

// 打开测量窗口时，从主会话复制一次当前显示状态。
// 测量窗口建立完成后继续使用自己的局部状态，不与主窗口联动。
namespace measure{
    struct MeasurementViewInitState {
        std::optional<std::array<double, 16>> modelMatrix;
        std::optional<std::array<double, 3>> cursorWorld;
        std::optional<std::array<double, 2>> windowLevel;
        std::optional<std::array<double, 3>> background;
        std::optional<std::uint32_t> visibilityMask;
    };
}


