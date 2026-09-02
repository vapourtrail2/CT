#pragma once

#include "MVVCVTK/API/Host/Types/HostViewTypes.h"
#include "MVVCVTK/SPI/Host/HostFeature.h"

#include <array>
#include <optional>

namespace measure {

struct MeasureHostData final {
    TrustedImageSnapshot image;
    std::array<double, 16> modelToWorld{};
};

// 测量模块只在此边界读取 SDK SPI，并立即复制稳定矩阵值；
// 弹窗后续不保留 Host 端口或 View identity。
class MeasureHostBridge final : public HostFeature {
public:
    explicit MeasureHostBridge(HostViewTarget sourceView);

    std::string_view GetFeatureId() const noexcept override;
    bool AttachHost(const HostFeatureContext& context) override;
    bool DetachHost() override;
    bool OnHostTick() override;

    const std::optional<MeasureHostData>& GetHostData() const noexcept;

private:
    HostViewTarget m_sourceView;
    std::optional<MeasureHostData> m_hostData;
};

} // namespace measure
