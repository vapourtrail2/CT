#pragma once
#include "c_ui/context/SessionManager.h"      
#include <vtkImageData.h>
#include <QString>
#include <array>
#include <memory>

// 打开的数据  包住 core 的 AppSession，
class Dataset
{
public:
    Dataset() = default;
    explicit Dataset(std::shared_ptr<AppSession> session)
        : session_(std::move(session)) {
    }

    bool getValid() const {
        return session_ && session_->dataMgr && session_->sharedState;
    }

    // 状态中心
    std::shared_ptr<SharedInteractionState> getState() const {
        return session_ ? session_->sharedState : nullptr;
    }
    std::shared_ptr<SharedStateBroadcaster> getBroadcaster() const {
        return session_ ? session_->sharedStateBroadcaster : nullptr;
    }

    // 只读视图
    QString getSourcePath() const {
        return session_ ? session_->sourcePath : QString();
    }

    bool getImage() const {
        return session_ && session_->dataMgr && session_->dataMgr->GetVtkImage();
    }

    std::array<int, 3> getDims() const {
        std::array<int, 3> d{ 0, 0, 0 };
        if (getImage()) {
            session_->dataMgr->GetVtkImage()->GetDimensions(d.data());
        }
        return d;
    }

    // 用原始 session
    std::shared_ptr<AppSession> getSession() const { 
        return session_; 
    }

    // 直方图等分析服务（RenderPanel 要用）
    std::shared_ptr<VolumeAnalysisService> getAnalysisService() const {
        return session_ ? session_->analysisService : nullptr;
    }

private:
    std::shared_ptr<AppSession> session_;
};