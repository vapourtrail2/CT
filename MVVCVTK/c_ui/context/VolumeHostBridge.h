#pragma once

#include "MVVCVTK/SPI/Host/HostFeature.h"
#include "string"
#include <cstddef>
#include <utility>
#include <vector>

class VolumeHostBridge final : public HostFeature {
public:
	std::string_view GetFeatureId() const noexcept override;
    bool AttachHost(const HostFeatureContext& context) override;
    bool DetachHost() override;
	bool OnHostTick() override;

	VtkImageGridSnapshot GetCapturedImage() const;
	const std::string& GetLastError() const noexcept;

private:
	VtkImageGridSnapshot captruedImage_;
	std::string lastError_;
};

class VolumePublishBridge final : public HostFeature {
public:
    std::string_view GetFeatureId() const noexcept override;
    FeatureDataContract GetDataContract() const override;
    bool AttachHost(const HostFeatureContext& context) override;
    bool DetachHost() override;
    bool OnHostTick() override;

    // 返回发布快照。这里只验证数据发布，不代表四视图已同步。
    VtkImageGridSnapshot Publish(
        const VtkImageGridSnapshot& archive,
        const DataEntityId& scopeId, const DataRevisionRef& revision,
        std::string& error);

    DataEntityId CreateDataEntityId();
    DataLifetimeState GetPublicationLifetime(const DataEntityId& scopeId) const;
    bool ReleasePublication(const DataEntityId& scopeId, std::string& error);
    bool ReleasePublications(bool all, std::string& error);
    std::size_t GetPublicationCount() const;

    bool RequestViewRenders();

private:
    std::shared_ptr<TrustedDataPort> data_;
    std::shared_ptr<FeatureViewDirectory> views_;
    std::vector<std::pair<DataEntityId, DataRevisionRef>> publications_;
};
