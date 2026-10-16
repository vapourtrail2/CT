# 第 1.2 步：修正快照保存，再验证 A/B 发布和四视图同步

## 这次的结论与范围

已检查你当前实际写入的源码。图像和 mask 的 `DeepCopy()`、先取得 shared_ptr 再卸载 bridge 的顺序是正确的。你报告已运行并看到 A/B 日志；下面的代码检查发现，日志还不能证明两份快照同时保留。

**先修正 A 被写入 B 槽的问题，再做发布测试。** 本教程按你的实际命名编写：全局 `VolumeHostBridge`、`capturedVolumeSnapshot()`、`originalSnapshot_`、`croppedSnapshot_`。不用改回上一份教程的名字，也不新建筛选器。

这次增加一个受控测试入口，用快捷键在已经保存的 A/B 之间发布切换。它不是完整的生产用切换控制器：不接场景树，不管理长期体积 ID，不恢复裁切历史，不做自动回滚。发布或同步失败后停止本轮编辑并要求重新加载，不能把失败当作已完成切换。

业务源码仍由你手动修改；本次只生成教程。以下新增代码尚未编译、运行，SDK 发布后的渲染同步能力正是这一小步要验证的内容。

## 0. 先修正检查发现的问题

### 0.1 原始 A 现在保存到了 croppedSnapshot_

位置：`C:\CT\MVVCVTK\c_ui\MainWindow.cpp`，`CTViewer::handleLoadFinished()`。

你现在虽然输出了 `A saved`，但赋值和读取的都是 `croppedSnapshot_`。`originalSnapshot_` 被 reset 后一直为空，裁切完成还会再次覆盖 `croppedSnapshot_`。

把加载成功后的 `//test` 整段替换为：

```cpp
originalSnapshot_.reset();
croppedSnapshot_.reset();

QString err;
originalSnapshot_ = context_.getSessionManager().capturedVolumeSnapshot(&err);

if (!originalSnapshot_) {
    qWarning() << "[VolumeCapture] A failed:" << err;
}
else {
    const auto& a = originalSnapshot_;
    qDebug() << "[VolumeCapture] A saved"
             << "version=" << a->version
             << "dims=" << a->dims[0] << a->dims[1] << a->dims[2]
             << "hasMask=" << (a->validityMask != nullptr)
             << "image=" << static_cast<const void*>(a->image.GetPointer());
}
```

保留 `handleCropBuildFinished()` 中保存 `croppedSnapshot_` 的代码。重新运行、加载并应用一次裁切后，在断点里确认 A、B 都非空，而且 image 指针不同。

### 0.2 捕获函数缺少失败分支

位置：`C:\CT\MVVCVTK\c_ui\context\SessionManager.cpp`。

当前线程不匹配时只写错误、不返回；也缺少 Host 状态检查，并忽略了 `AttachFeature()` 的结果。把你现有的 `capturedVolumeSnapshot()` 整个替换为：

```cpp
TrustedImageSnapshot SessionManager::capturedVolumeSnapshot(QString* err)
{
    if (err) err->clear();

    if (QThread::currentThread() != thread()) {
        setError(err, QStringLiteral("Capture must run on the owner thread."));
        return {};
    }

    if (!hostSession_ || state_ != State::Ready || cropBuildPending_) {
        setError(err, QStringLiteral("Volume is not ready for capture."));
        return {};
    }

    auto bridge = std::make_shared<VolumeHostBridge>();
    if (!hostSession_->AttachFeature(bridge)) {
        const auto& detail = bridge->GetLastError();
        setError(err, detail.empty()
            ? QStringLiteral("Failed to attach capture feature.")
            : QString::fromStdString(detail));
        return {};
    }

    const auto captured = bridge->GetCapturedImage();
    if (!hostSession_->DetachFeature(*bridge)) {
        setError(err, QStringLiteral("Failed to detach capture feature."));
        return {};
    }

    if (!captured || !captured->image) {
        setError(err, QStringLiteral("Captured volume is empty."));
        return {};
    }
    return captured;
}
```

文件中的 `#include <Qthread>` 改为标准拼写 `#include <QThread>`。

### 0.3 GetLastError 返回引用

你现在的返回类型是 `const std::string`，会复制字符串；同时声明 `noexcept`，复制分配失败时会终止程序。改为返回已有字符串的 const 引用。

`C:\CT\MVVCVTK\c_ui\context\VolumeHostBridge.h`：

```cpp
const std::string& GetLastError() const noexcept;
```

`C:\CT\MVVCVTK\c_ui\context\VolumeHostBridge.cpp`：

```cpp
const std::string& VolumeHostBridge::GetLastError() const noexcept
{
    return lastError_;
}
```

头文件的 `#include "string"` 改成 `#include <string>`；cpp 中你使用了 `std::cerr`，显式补上 `#include <iostream>`。`captruedImage_` 虽有拼写问题，但声明和使用一致，本步骤不要求改名。

**先编译并重测 A/B 两份保存，再加入下面的发布代码。**

## 1. 在现有文件里增加 VolumePublishBridge

继续放在现有的 `c_ui → context` 筛选器。直接使用已有 `VolumeHostBridge.h/.cpp`，不需要新增项目项。

单独增加发布类，是为了让 `AttachHost()` 只取得操作端口：先确认 Feature 接入成功，再显式执行会改变当前数据的发布。捕获类继续负责读取和复制。

### 1.1 VolumeHostBridge.h 末尾追加

```cpp
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
```

### 1.2 VolumeHostBridge.cpp 增加 include

```cpp
#include "MVVCVTK/SPI/App/Services/FeatureViewService.h"
#include <utility>
```

### 1.3 VolumeHostBridge.cpp 末尾追加

```cpp
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

bool VolumePublishBridge::RequestViewRenders()
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
    if (views.size() != 4) return false; // 针对当前四视图测试配置

    bool ok = true;
    for (const auto& view : views) {
        const auto port = views_->GetFeaturePort(view.id);
        if (!port || !port->SetRenderNeeded()) ok = false;
    }
    return ok;
}
```

这里不手动设置 version。归档的版本是来源信息，`expected` 是当前输入，`published->version` 才是这次发布的结果。若 SDK 拒绝或没有推进版本，保留错误信息，不猜测应将哪个版本字段改成 0 或旧版本强行重试。

`SetRenderNeeded()` 按 SDK 注释只是重绘请求，不保证数据管线已更新。后面的版本检查仍然必须执行。

## 2. SessionManager 增加测试用发布和检查入口

### 2.1 SessionManager.h 的 public 区增加

```cpp
TrustedImageSnapshot publishVolumeSnapshotForTest(
    const TrustedImageSnapshot& archive,
    QString* err = nullptr);

bool areAllRenderViewsAtVersion(
    std::uint64_t expectedVersion,
    QString* report = nullptr);

void failVolumePublishTest(const QString& reason);
```

这些是本步骤新增的前端函数，不是 SDK 已有接口。后续由正式控制器接管测试接线。

### 2.2 SessionManager.cpp 增加 include

```cpp
#include <QDebug>
#include <QStringList>
```

### 2.3 SessionManager.cpp 末尾追加

```cpp
void SessionManager::failVolumePublishTest(const QString& reason)
{
    qWarning() << "[VolumePublish] test stopped:" << reason;
    setState(State::Failed);
    emit isoStateCleared();
}

TrustedImageSnapshot SessionManager::publishVolumeSnapshotForTest(
    const TrustedImageSnapshot& archive,
    QString* err)
{
    if (err) err->clear();

    if (QThread::currentThread() != thread()) {
        setError(err, QStringLiteral("Publish must run on the owner thread."));
        return {};
    }
    if (!hostSession_ || state_ != State::Ready || cropBuildPending_
        || !archive || !archive->image) {
        setError(err, QStringLiteral("Host or archive is not ready."));
        return {};
    }
    if (gapFeature_) {
        setError(err, QStringLiteral(
            "This test requires no Gap feature. Restart and test loading/cropping only."));
        return {};
    }

    auto fail = [this, err](const QString& message) -> TrustedImageSnapshot {
        setError(err, message);
        failVolumePublishTest(message);
        return {};
    };

    // 本步骤只接受空闲四视图；先安全移除当前裁切上下文。
    if (cropFeature_ && cropFeature_->GetState().isPublishing) {
        setError(err, QStringLiteral("Crop is still publishing."));
        return {};
    }

    cropStateTimer_.stop();
    if (cropFeature_) {
        if (cropFeature_->GetState().isActive) {
            CropHostRequest exit;
            exit.action = CropHostAction::Exit;
            if (!cropFeature_->SendRequest(std::move(exit))) {
                return fail(QStringLiteral("Failed to exit crop editing."));
            }
        }
        if (!hostSession_->DetachFeature(*cropFeature_)) {
            return fail(QStringLiteral("Failed to detach crop feature."));
        }
        cropFeature_.reset();
    }
    if (!hostSession_->AttachTimer({})) {
        return fail(QStringLiteral("Failed to detach Host timer."));
    }
    clearCropHistory();

    auto bridge = std::make_shared<VolumePublishBridge>();
    if (!hostSession_->AttachFeature(bridge)) {
        return fail(QStringLiteral("Failed to attach publish feature."));
    }

    std::string detail;
    const auto published = bridge->Publish(archive, detail);
    const bool renderRequested = published && bridge->RequestViewRenders();
    const bool detached = hostSession_->DetachFeature(*bridge);

    if (!detached) {
        return fail(QStringLiteral("Failed to detach publish feature."));
    }
    if (!published) {
        return fail(QString::fromStdString(detail));
    }
    if (!renderRequested) {
        return fail(QStringLiteral("Failed to request renders for all four views."));
    }

    // 每次重新发布后重新建立裁切基线，不沿用旧体积的编辑历史。
    QString cropError;
    if (!resetCropFeature(&cropError)) return fail(cropError);

    setIsoState();
    return published; // 仅发布成功，四视图同步由调用者继续检查
}

bool SessionManager::areAllRenderViewsAtVersion(
    std::uint64_t expectedVersion,
    QString* report)
{
    if (!hostSession_ || config_.renderViews.size() != 4) {
        if (report) *report = QStringLiteral("Four-view Host is unavailable.");
        return false;
    }

    bool all = true;
    QStringList parts;
    for (const auto& config : config_.renderViews) {
        HostViewTarget target;
        target.viewId = config.id;
        const auto state = hostSession_->GetRenderViewState(target);
        const QString id = QString::fromStdString(config.id);

        if (!state) {
            all = false;
            parts << (id + QStringLiteral("=unavailable"));
        }
        else {
            if (state->dataVersion != expectedVersion) all = false;
            parts << (id + QStringLiteral("=")
                + QString::number(static_cast<qulonglong>(state->dataVersion)));
        }
    }
    if (report) *report = parts.join(QStringLiteral(", "));
    return all;
}
```

这里没有直接复用 `clearCropFeature()` 来完成第一次卸载，因为它当前忽略了部分 SDK 清理返回值；本测试需要明确区分“卸载失败”和“数据发布失败”。

本步骤要求未开启孔隙分析，不支持一边后台分析一边发布。正式实现再协调 Gap 等功能的退出与重建。测试失败后 `State::Failed` 会阻止普通编辑入口继续处理不确定数据；重新加载小数据开始新一轮。它不是生产级回滚策略。

## 3. 主界面增加两个临时快捷键

### 3.1 MainWindow.h 增加

在 `private:` 函数区增加：

```cpp
void switchVolumeSnapshotForTest(bool useOriginal);
```

在成员区增加：

```cpp
bool volumePublishTestBusy_ = false;
```

保留你现有的 `originalSnapshot_` 和 `croppedSnapshot_`。

### 3.2 MainWindow.cpp 增加 include

```cpp
#include <QShortcut>
#include <QKeySequence>
#include <QTimer>
```

### 3.3 CTViewer 构造函数的 wireConnect() 后追加

```cpp
auto* showA = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Alt+1")), this);
showA->setAutoRepeat(false);
connect(showA, &QShortcut::activated, this, [this]() {
    switchVolumeSnapshotForTest(true);
});

auto* showB = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Alt+2")), this);
showB->setAutoRepeat(false);
connect(showB, &QShortcut::activated, this, [this]() {
    switchVolumeSnapshotForTest(false);
});
```

这是临时测试接线，后续由场景树“激活”操作替换。不要在 `handleLoadFinished()` 中反复创建快捷键。

### 3.4 MainWindow.cpp 追加完整测试函数

```cpp
void CTViewer::switchVolumeSnapshotForTest(bool useOriginal)
{
    if (volumePublishTestBusy_) return;

    if (!originalSnapshot_ || !croppedSnapshot_) {
        QMessageBox::warning(this, QStringLiteral("体积切换测试"),
            QStringLiteral("请先加载 A 并成功应用一次裁切，确认 A/B 都已保存。"));
        return;
    }

    const auto archive = useOriginal ? originalSnapshot_ : croppedSnapshot_;
    const QString name = useOriginal ? QStringLiteral("A") : QStringLiteral("B");

    volumePublishTestBusy_ = true;
    setEnabled(false); // 测试期间不允许发起其他 UI 数据操作，定时器仍工作

    QString error;
    auto& manager = context_.getSessionManager();
    const auto published = manager.publishVolumeSnapshotForTest(archive, &error);
    if (!published) {
        volumePublishTestBusy_ = false;
        setEnabled(true);
        QMessageBox::warning(this, QStringLiteral("发布测试未完成"), error);
        return;
    }

    qDebug() << "[VolumePublish] data published" << name
             << "archiveVersion=" << archive->version
             << "publishedVersion=" << published->version
             << "hasMask=" << (published->validityMask != nullptr);

    workspacePage_->getViewportGather()->requestRefresh();

    const auto expectedVersion = published->version;
    auto attempts = std::make_shared<int>(0);
    auto* timer = new QTimer(this);
    timer->setInterval(50);

    connect(timer, &QTimer::timeout, this,
        [this, timer, attempts, expectedVersion, name]() {
            QString report;
            auto& session = context_.getSessionManager();
            const bool synchronized =
                session.areAllRenderViewsAtVersion(expectedVersion, &report);

            ++(*attempts);
            if (!synchronized && *attempts < 60) return;

            timer->stop();
            timer->deleteLater();
            volumePublishTestBusy_ = false;
            setEnabled(true);

            if (synchronized) {
                qDebug() << "[VolumePublish] four views synchronized"
                         << name << report;
                statusBar()->showMessage(
                    QStringLiteral("已发布 %1，四视图版本一致；请继续检查图像和 mask。")
                        .arg(name), 5000);
            }
            else {
                const QString reason = QStringLiteral(
                    "等待四视图同步超时，期望版本 %1；实际：%2。请重新加载后测试。")
                    .arg(static_cast<qulonglong>(expectedVersion)).arg(report);
                session.failVolumePublishTest(reason);
                QMessageBox::warning(this, QStringLiteral("视图同步未通过"), reason);
            }
        });
    timer->start();
}
```

测试期间临时禁用窗口，最多进行 60 次版本检查；没有阻塞事件循环，也没有用 `sleep()` 或 `processEvents()`。同一个窗口只能进行一轮测试。窗口关闭时，以窗口为 context 的定时回调会自动清理。

版本一致仅是必要条件：还必须观察实际画面、mask 和再次裁切行为。测试代码不自动推进正式的激活体积 ID。

## 4. 编译与验收

### 编译前

1. 先完成第 0 节修正，证明 `originalSnapshot_` 不再为空，A/B 分别保存。
2. 再加入发布类、SessionManager 测试入口和 MainWindow 快捷键。
3. 使用现有 Debug/x64 配置编译。新代码基于本地 SDK 声明编写，本文没有替你修改源码或执行编译；若有编译错误，先看首个错误，不继续加入 VolumeManager。

### 运行顺序

1. 重启程序，用小体积数据测试，本轮只执行加载和裁切，不打开孔隙或其他分析。
2. 加载 A，应用一次效果明显的裁切得到 B，先看到两份保存日志，断点确认 A/B 都非空且指针不同。
3. 等裁切界面刷新结束，按 `Ctrl+Alt+1`。先看到 `data published A`，再看到 `four views synchronized A`。
4. 在三个二维视图浏览多个切片，确认 A 的原始内容恢复；三维也应恢复原始形状。
5. 按 `Ctrl+Alt+2`。确认发布 B、四视图版本一致，二维有效区域与三维裁切形状恢复；不是只有三维改变而二维仍停在 A。
6. 再按 A、B 往返。每次都要检查新发布版本、四个实际版本和画面。归档自己的来源版本保持不变是正常现象。
7. A/B 往返通过后，在当前体积重新启动裁切，确认可编辑、应用和得到新产物。当前测试接线会把最新裁切产物写进槽 B，这一轮之后槽 B 不再代表最初那个裁切结果。

### mask 和归档检查

- 在已知裁切掉及保留的体素位置，检查 B 的归档 mask、发布后的 mask 和实际显示是否一致；尺寸相同不代表没有裁切。
- 在发布函数处断点检查工作图像和存在的 mask 与归档指针不同。
- 记录 A/B 归档在已知位置的图像和 mask 值，往返发布及重新编辑后确认归档没有被改写。
- 版本号一致不保证 mask 被渲染管线正确使用，不能只凭日志宣布验收通过。

### 根据失败位置判断

| 现象 | 下一步检查 |
|---|---|
| A 为空，快捷键拒绝执行 | 第 0.1 节的赋值和读取成员是否全部改成 originalSnapshot_ |
| Attach/Detach 或 timer 清理失败 | 先解决对应 SDK 生命周期返回值，不继续重复发布 |
| SetImageState 返回 false | 数据发布入口尚未验证，保留错误与断点中的 expected/candidate，不能靠改版本号绕过 |
| data published 成功，四视图版本超时 | 发布与视图管线同步存在未解决问题，提供期望版本和四个实际版本；不要仅增加 Render 调用后声称成功 |
| 版本一致但裁切形状或二维有效区域不对 | 检查 mask 保存、发布及各渲染策略对 mask 的使用 |
| 往返显示正确，但不能再次裁切 | 检查新裁切 Feature 的基线、输入版本和重新挂载流程 |

若这一测试路径不支持完整发布或同步，需要 SDK 提供包含数据提交和视图更新的稳定操作。本文使用的是本地已声明的接口，未假设它们已经通过运行验证。

## 5. 本步骤通过以后

第 1.2 步通过，再写第 2 步：将两个临时快照槽换为 `VolumeManager` 的多个体积对象，增加稳定 VolumeId 和每体积测量集合，正式处理激活、失败恢复和 UI 操作。临时快捷键、整窗禁用及测试用 State::Failed 处理不会直接当作最终交互设计保留。
