#pragma once

#include "MVVCVTK/SPI/Host/HostFeature.h"
#include "string"

class VolumeHostBridge final : public HostFeature {
public:
	std::string_view GetFeatureId() const noexcept override;
    bool AttachHost(const HostFeatureContext& context) override;
    bool DetachHost() override;
	bool OnHostTick() override;

	TrustedImageSnapshot GetCapturedImage() const;
	const std::string& GetLastError() const noexcept;

private:
	TrustedImageSnapshot captruedImage_;
	std::string lastError_;
};

class VolumePublishBridge final : public HostFeature {
public:
    std::string_view GetFeatureId() const noexcept override;
    bool AttachHost(const HostFeatureContext& context) override;
    bool DetachHost() override;
    bool OnHostTick() override;

    // 返回发布快照。这里只验证数据发布，不代表四视图已同步。
    TrustedImageSnapshot Publish(
        const TrustedImageSnapshot& archive,
        std::string& error);

    bool RequestViewRenders();

private:
    std::shared_ptr<TrustedFeatureDataPort> data_;
    std::shared_ptr<FeatureViewDirectory> views_;
};