#include "VolumeHostBridge.h"
#include "MVVCVTK/SPI/App/Services/FeatureViewService.h"
#include "MVVCVTK/SPI/Data/DataPayloads.h"
#include <vtkSmartPointer.h>
#include <vtkImageData.h>
#include <exception>
#include <memory>
#include <utility>
#include <iostream>

namespace {
std::string GetScopeText(const DataEntityId& scope)
{
    constexpr char digits[] = "0123456789abcdef";
    std::string text(32, '0');
    for (std::size_t i = 0; i < scope.bytes.size(); ++i) {
        text[i * 2] = digits[scope.bytes[i] >> 4];
        text[i * 2 + 1] = digits[scope.bytes[i] & 15];
    }
    return text;
}
std::shared_ptr<const ImageGrid3DPayload> CopyImagePayload(const ImageGrid3DPayload& source)
{
    const auto& original = source.GetValues();
    if (!original) return {};
    auto bytes = std::make_shared<const std::vector<std::uint8_t>>(
        original->begin(), original->end());
    return std::make_shared<const ImageGrid3DPayload>(source.GetGeometry(),
        source.GetValueType(), source.GetComponentCount(), std::move(bytes),
        source.GetValidityMask(), source.GetScalarRange(), source.GetMetadata());
}
}

std::string_view VolumeHostBridge::GetFeatureId() const noexcept
{
	return "ct.volume-host-bridge";
}

bool VolumeHostBridge::AttachHost(const HostFeatureContext& context)
{
	captruedImage_.reset();
	lastError_.clear();

	if (!context.data) {
		lastError_ = "data port is unvailable";
		return false;
	}

	try
	{
		const auto source = context.data->GetPrimaryImage();
		if (!source || !source->image || !source->data || !source->binding) {
			lastError_ = "Current volume image is unavailable.";
			return false;
		}

		const auto payload = std::dynamic_pointer_cast<const ImageGrid3DPayload>(source->data->payload);
		const auto copied = payload ? CopyImagePayload(*payload) : nullptr;
		const auto prepared = copied ? VtkPreparedDataView::BuildDataView(copied) : nullptr;
		if (!prepared || !prepared->image) {
			lastError_ = "Independent archive preparation failed.";
			return false;
		}
		// 只复制值标识供日志使用；不保存原 graph、别名 data、租约或原像素/mask owner。
		auto archiveData = std::make_shared<DataRevision>();
		archiveData->self = source->data->self;
		archiveData->type = copied->GetDataType();
		archiveData->payload = copied;
		archiveData->provenance = source->data->provenance;
		auto capturedptr = std::make_shared<VtkImageGridView>(*prepared->image);
		capturedptr->data = std::move(archiveData);
		capturedptr->binding = source->binding;
		captruedImage_= capturedptr;
		return true;

	}
	catch (const std::exception& e)
	{
		std::cerr << "volumehostbridge error is" << e.what();
		lastError_= e.what();
		captruedImage_.reset();
		return false;
	}
}

bool VolumeHostBridge::DetachHost()
{
	captruedImage_.reset();
	return true;
}

bool VolumeHostBridge::OnHostTick()
{
	return true;
}

VtkImageGridSnapshot VolumeHostBridge::GetCapturedImage() const
{
	return captruedImage_;
}

const std::string& VolumeHostBridge::GetLastError() const noexcept
{
	return lastError_;
}

//publish
std::string_view VolumePublishBridge::GetFeatureId() const noexcept
{
    return "ct.volume-publish-test";
}

FeatureDataContract VolumePublishBridge::GetDataContract() const
{
    return {{}, {{"archived-volume", DataTypes::imageGrid3D, {DataFacets::scalarGrid3D}}}};
}

bool VolumePublishBridge::AttachHost(const HostFeatureContext& context)
{
    data_ = context.data;
    views_ = context.views;
    return data_ && views_;
}

bool VolumePublishBridge::DetachHost()
{
    // 账本随 Host 持有的 bridge 存活；不依赖外层 SessionManager 的析构次序。
    // Host Stop 先归还当前 owned primary，再调用此处；阻塞时保留端口和责任记录。
    std::string error;
    if (!ReleasePublications(true, error)) return false;
    data_.reset();
    views_.reset();
    return true;
}

bool VolumePublishBridge::OnHostTick()
{
    return true;
}

VtkImageGridSnapshot VolumePublishBridge::Publish(
    const VtkImageGridSnapshot& archive,
    const DataEntityId& scopeId, const DataRevisionRef& revision,
    std::string& error)
{
    error.clear();
    if (!data_ || !archive || !archive->image || !archive->data
        || !GetDataEntityIdValid(scopeId) || !GetDataRevisionRefValid(revision)) {
        error = "Publish data port or archive is unavailable.";
        return {};
    }

    try {
        // 在任何正式提交前登记，包含拒绝、通知异常和随后换绑的情况。
        publications_.emplace_back(scopeId, revision);
        // expected 必须是现在的 SDK 输入，不能使用归档 A/B 代替。
        const auto expected = data_->GetPrimaryImage();
        if (!expected || !expected->image || !expected->data || !expected->binding) {
            error = "Current SDK input is unavailable.";
            return {};
        }

        const auto payload = std::dynamic_pointer_cast<const ImageGrid3DPayload>(archive->data->payload);
        if (!payload || !payload->GetValid()) {
            error = "Archived image payload is unavailable.";
            return {};
        }
        // 发布存储也独立于归档，保证 A/B 常驻时旧发布作用域仍能真正释放。
        DataRevisionDraft output;
        output.entityId = revision.entityId;
        output.type = payload->GetDataType();
        output.payload = CopyImagePayload(*payload);
        output.provenance = archive->data->provenance;
        output.lifetimeScope = scopeId;
        output.lifetimePolicy = DataLifetimePolicy::OwnerRetirement;
        DataTransaction transaction;
        transaction.outputs.push_back(std::move(output));
        transaction.bindings.push_back({std::string(primaryVolumeBinding),
            expected->binding->revision, true, expected->binding->target, revision});
        struct Receipt final : DataCommitReceipt {
            bool committed = false;
            void SetDataCommitted(const DataCommitResult& result) noexcept override {
                committed = result.status != DataCommitStatus::Rejected && result.isActivated;
            }
        } receipt;
        DataCommitResult committed;
        try {
            committed = data_->SetDataCommit(std::move(transaction), &receipt);
        }
        catch (const std::exception& e) {
            if (!receipt.committed) throw;
            std::cerr << "Volume published; notification failed: " << e.what() << '\n';
        }
        if (!receipt.committed && committed.status == DataCommitStatus::Rejected) {
            error = "SDK archive publication rejected: " + committed.message;
            return {};
        }
        const auto published = data_->GetPrimaryImage();
        if (!published || !published->image || !published->data || !published->binding
            || published->data->self != revision
            || published->binding->revision <= expected->binding->revision) {
            error = "Archive was published but is no longer the active input.";
            return {};
        }
        return published;
    }
    catch (const std::exception& e) {
        error = e.what();
        return {};
    }
}

DataEntityId VolumePublishBridge::CreateDataEntityId()
{
    return data_ ? data_->CreateDataEntityId() : DataEntityId{};
}

DataLifetimeState VolumePublishBridge::GetPublicationLifetime(const DataEntityId& scopeId) const
{
    return data_ ? data_->GetDataLifetime(scopeId) : DataLifetimeState{};
}

bool VolumePublishBridge::ReleasePublication(const DataEntityId& scopeId, std::string& error)
{
    if (!data_) { error = "Publication data port is unavailable."; return false; }
    try {
        auto state = data_->GetDataLifetime(scopeId);
        if (state.status == DataLifetimeStatus::Unknown || state.status == DataLifetimeStatus::Released) return true;
        if (state.status == DataLifetimeStatus::Published) {
            DataTransaction transaction;
            transaction.retireScopes.push_back({scopeId, DataLifetimeStatus::Published,
                state.ownedRevisions, true});
            try {
                const auto retired = data_->SetDataCommit(std::move(transaction));
                if (retired.status == DataCommitStatus::Rejected) {
                    error = retired.message;
                    return false;
                }
            }
            catch (const std::exception&) {
                // 通知异常不撤销已发生的退役；重新读取实际状态。
                if (data_->GetDataLifetime(scopeId).status == DataLifetimeStatus::Published) throw;
            }
        }
        try { state = data_->SetDataRelease(scopeId); }
        catch (const std::exception&) {
            state = data_->GetDataLifetime(scopeId);
            if (state.status != DataLifetimeStatus::Released) throw;
        }
        if (state.status != DataLifetimeStatus::Released) {
            error = "Publication resources are still in use.";
            return false;
        }
        return true;
    }
    catch (const std::exception& e) { error = e.what(); return false; }
}

bool VolumePublishBridge::ReleasePublications(bool all, std::string& error)
{
    if (!data_) return publications_.empty();
    const auto current = data_->GetDataBinding(data_->GetDataGraph(), primaryVolumeBinding);
    bool complete = true;
    for (auto item = publications_.begin(); item != publications_.end();) {
        if (!all && current && current->target == item->second) { ++item; continue; }
        if (!ReleasePublication(item->first, error)) { complete = false; ++item; continue; }
        const auto state = GetPublicationLifetime(item->first);
        std::cerr << (state.status == DataLifetimeStatus::Released
            ? "[VolumeScope] released scope=" : "[VolumeScope] uncommitted scope=")
            << GetScopeText(item->first) << " state=" << int(state.status)
            << " remaining=" << (publications_.size() - 1) << '\n';
        item = publications_.erase(item);
    }
    return complete;
}

std::size_t VolumePublishBridge::GetPublicationCount() const
{
    return publications_.size();
}

bool VolumePublishBridge::RequestViewRenders()//找到视图并且刷新界面
{
    if (!views_) return false;

    HostViewTargets targets;
    targets.viewRoles = {
        HostRenderViewRole::Primary3D,
        HostRenderViewRole::TopDownSlice,
        HostRenderViewRole::FrontBackSlice,
        HostRenderViewRole::LeftRightSlice
    };

    const auto views = views_->GetViews(targets);
    if (views.size() != 4)
        return false; // 针对当前四视图测试配置

    bool ok = true;
    for (const auto& view : views) {
        const auto port = views_->GetFeaturePort(view.id);
        if (!port || !port->SetRenderNeeded())
        {
            ok = false;
        }
    }
    return ok;
}


