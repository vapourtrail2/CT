#include "VolumeHostBridge.h"
#include "MVVCVTK/SPI/App/Services/FeatureViewService.h"
#include <vtkSmartPointer.h>
#include <vtkImageData.h>
#include <exception>
#include <memory>
#include <utility>

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
		const auto source = context.data->GetImageSnapshot();
		if (!source || !source->image) {
			lastError_ = "Current volume image is unavailable.";
			return false;
		}

		//复制数据 
		auto capturedptr = std::make_shared<TrustedImageState>(*source);
		auto copyimage = vtkSmartPointer<vtkImageData>::New();
		copyimage->DeepCopy(source->image);
		capturedptr->image = copyimage;

		if (source->validityMask) {
			auto maskcopyed = vtkSmartPointer<vtkImageData>::New();
			maskcopyed->DeepCopy(source->validityMask);
			capturedptr->validityMask = maskcopyed;
		}
		else
		{
			capturedptr->validityMask = nullptr;
		}
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

TrustedImageSnapshot VolumeHostBridge::GetCapturedImage() const
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

bool VolumePublishBridge::AttachHost(const HostFeatureContext& context)
{
    data_ = context.data;
    views_ = context.views;
    return data_ && views_;
}

bool VolumePublishBridge::DetachHost()
{
    data_.reset();
    views_.reset();
    return true;
}

bool VolumePublishBridge::OnHostTick()
{
    return true;
}

TrustedImageSnapshot VolumePublishBridge::Publish(
    const TrustedImageSnapshot& archive,
    std::string& error)
{
    error.clear();
    if (!data_ || !archive || !archive->image) {
        error = "Publish data port or archive is unavailable.";
        return {};
    }

    try {
        // expected 必须是现在的 SDK 输入，不能使用归档 A/B 代替。
        const auto expected = data_->GetImageSnapshot();
        if (!expected || !expected->image) {
            error = "Current SDK input is unavailable.";
            return {};
        }

        TrustedImageState working = *archive;
        auto image = vtkSmartPointer<vtkImageData>::New();
        image->DeepCopy(archive->image);
        working.image = image;

        if (archive->validityMask) {
            auto mask = vtkSmartPointer<vtkImageData>::New();
            mask->DeepCopy(archive->validityMask);
            working.validityMask = mask;
        }
        else {
            working.validityMask = nullptr;
        }

        TrustedImageSnapshot published;
        if (!data_->SetImageState(std::move(working), expected, published)) {
            error = "SDK SetImageState rejected the candidate.";
            return {};
        }

        if (!published || !published->image) {
            error = "SDK returned no published image.";
            return {};
        }
        if (published->version == expected->version) {
            error = "Published input version did not advance.";
            return {};
        }
        return published;
    }
    catch (const std::exception& e) {
        error = e.what();
        return {};
    }
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


