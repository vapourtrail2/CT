#pragma once
#include "AppController.h"      // AppSession 定义在这
#include <vtkImageData.h>
#include <QString>
#include <array>
#include <memory>

// 打开的数据（Model 的外观）：包住 core 的 AppSession，
// 对 UI 只暴露“只读视图” + 状态/广播句柄，避免 UI 直接摸 DataManager。
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

    // 只读视图：UI 想知道的“静态信息”，从这里问，不要自己去翻 DataManager
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

    // 过渡期：还没迁的地方（viewport / renderPanel）先用原始 session
    std::shared_ptr<AppSession> getSession() const { 
        return session_; 
    }

private:
    std::shared_ptr<AppSession> session_;
};