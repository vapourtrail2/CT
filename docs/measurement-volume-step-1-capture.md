# 第 1 步：保存完整体积快照（1.1）

已按当前手写实现完成静态复核，具体修正及下一步见 [第 1.2 步教程](/C:/CT/docs/measurement-volume-step-1-publish.md)。先看其中第 0 节：加载时须保存到 A 槽，而不是与裁切产物共用 B 槽；同时补齐捕获入口的失败检查。该后续教程沿用当前代码的全局 `VolumeHostBridge` 和 `capturedVolumeSnapshot()` 命名，不需要再按本文示例改名。

## 这次做什么

按总方案的实施顺序，第 1 步是验证“完整快照保存与发布”。先拆成两个验收点：

1. **1.1：保存原始体积 A 和裁切体积 B 的独立快照。本文完成这一小步。**
2. 1.2：重新发布 A/B，确认四视图数据和 mask 同步，再验证能继续裁切。

本次先让数据读取、复制和保存这条链路独立可验收，暂不写发布，不接场景树，也不改测量算法。教程中的业务代码由你手动添加；本文件不代表已经修改、编译或运行了这些代码。

目标链路：

```text
加载完成 / 裁切完成
→ SessionManager::captureVolumeSnapshot()
→ 临时 AttachFeature(VolumeHostBridge)
→ GetImageSnapshot()
→ DeepCopy 图像和 mask
→ 返回独立快照
→ DetachFeature()
→ CTViewer 暂存 A/B，供下一小步切换验证
```

调用必须位于 Qt/Host owner 线程。先使用小体积数据，深拷贝会增加耗时和内存占用；后续有明确不可变共享契约时再优化。

## 1. 新增 VolumeHostBridge.h

直接使用现有的 `c_ui → context` 筛选器，不需要新建 `scene` 文件夹或筛选器。

在 Visual Studio 中右键 `context` → **添加 → 新建项**，创建头文件 `VolumeHostBridge.h`。新建窗口的“位置”填写：

```text
C:\CT\MVVCVTK\c_ui\context
```

这样它与 `SessionManager.h/.cpp` 放在同一个磁盘目录。头文件内容如下：

```cpp
#pragma once

#include "MVVCVTK/SPI/Host/HostFeature.h"

#include <string>

namespace scene {

class VolumeHostBridge final : public HostFeature {
public:
    std::string_view GetFeatureId() const noexcept override;
    bool AttachHost(const HostFeatureContext& context) override;
    bool DetachHost() override;
    bool OnHostTick() override;

    TrustedImageSnapshot GetCapturedImage() const;
    const std::string& GetLastError() const noexcept;

private:
    TrustedImageSnapshot capturedImage_;
    std::string lastError_;
};

} // namespace scene
```

它的作用和当前 `MeasureHostBridge` 类似：让前端经过 SDK 允许的 HostFeature 边界获取数据。本次不缓存数据端口、renderer 或 interactor。

代码中的 `namespace scene` 是 C++ 命名空间，与文件夹、VS 筛选器无关，可以保留；它不要求创建名为 `scene` 的目录。

## 2. 新增 VolumeHostBridge.cpp

同样右键现有的 `context` 筛选器 → **添加 → 新建项**，创建 `VolumeHostBridge.cpp`，位置仍填写 `C:\CT\MVVCVTK\c_ui\context`：

```cpp
#include "c_ui/context/VolumeHostBridge.h"

#include <vtkImageData.h>
#include <vtkSmartPointer.h>

#include <exception>
#include <memory>

namespace scene {

std::string_view VolumeHostBridge::GetFeatureId() const noexcept
{
    return "ct.volume-host-bridge";
}

bool VolumeHostBridge::AttachHost(const HostFeatureContext& context)
{
    capturedImage_.reset();
    lastError_.clear();

    if (!context.data) {
        lastError_ = "Host data port is unavailable.";
        return false;
    }

    try {
        const auto source = context.data->GetImageSnapshot();
        if (!source || !source->image) {
            lastError_ = "Current volume image is unavailable.";
            return false;
        }

        // 先复制元信息。此时 image/mask 的智能指针仍指向来源数据。
        auto captured = std::make_shared<TrustedImageState>(*source);

        // 再替换成独立图像，避免后续编辑改写归档内容。
        auto imageCopy = vtkSmartPointer<vtkImageData>::New();
        imageCopy->DeepCopy(source->image);
        captured->image = imageCopy;

        // 裁切产物的有效区域可能由 mask 表达，必须一并保存。
        if (source->validityMask) {
            auto maskCopy = vtkSmartPointer<vtkImageData>::New();
            maskCopy->DeepCopy(source->validityMask);
            captured->validityMask = maskCopy;
        }
        else {
            captured->validityMask = nullptr;
        }

        capturedImage_ = captured;
        return true;
    }
    catch (const std::exception& error) {
        lastError_ = error.what();
        capturedImage_.reset();
        return false;
    }
}

bool VolumeHostBridge::DetachHost()
{
    capturedImage_.reset();
    return true;
}

bool VolumeHostBridge::OnHostTick()
{
    return true;
}

TrustedImageSnapshot VolumeHostBridge::GetCapturedImage() const
{
    return capturedImage_;
}

const std::string& VolumeHostBridge::GetLastError() const noexcept
{
    return lastError_;
}

} // namespace scene
```

这里最关键的是：

```cpp
auto captured = std::make_shared<TrustedImageState>(*source);
```

这一句复制结构体和智能指针，不复制体素。真正复制体素和 mask 的是后面的两次 `DeepCopy()`。原始体积通常没有 mask，裁切结果则需要检查实际是否带有 mask。

保留归档的来源 `version`，本次只用于诊断；它不是未来的 VolumeId，也不能被当作重新发布后的版本。

## 3. 确认两个文件已加入 VS 项目

按照前两节通过 VS“添加 → 新建项”创建后，VS 会维护 `.vcxproj` 和 `.filters`，不需要再手动编辑这些文件，也不需要新建筛选器。

确认两个文件都出现在现有 `c_ui → context` 下，实际完整路径分别为：

```text
C:\CT\MVVCVTK\c_ui\context\VolumeHostBridge.h
C:\CT\MVVCVTK\c_ui\context\VolumeHostBridge.cpp
```

筛选器只是显示分组，不保证文件保存到了同名目录。可以在文件属性中检查“完整路径”。如果文件已经在磁盘上创建过，使用“添加 → 现有项”加入即可，不要重复创建。

这两个类文件没有 `Q_OBJECT`，不需要 MOC。

## 4. SessionManager 增加一个入口

### 4.1 头文件

打开 `C:\CT\MVVCVTK\c_ui\context\SessionManager.h`。

显式加入：

```cpp
#include "MVVCVTK/SPI/Data/TrustedImageState.h"
```

在 `public:` 中、现有 `getMeasureHostData()` 附近增加：

```cpp
TrustedImageSnapshot captureVolumeSnapshot(QString* errorOut = nullptr);
`

### 4.2 实现文件

打开 `C:\CT\MVVCVTK\c_ui\context\SessionManager.cpp`，增加 include：

```cpp
#include "c_ui/context/VolumeHostBridge.h"
#include <QThread>
```

在 `getMeasureHostData()` 前后新增下面的完整函数，不替换原函数：

```cpp
TrustedImageSnapshot SessionManager::captureVolumeSnapshot(QString* errorOut)
{
    if (errorOut) {
        errorOut->clear();
    }

    if (QThread::currentThread() != thread()) {
        setError(errorOut, QStringLiteral("Capture must run on the owner thread."));
        return {};
    }

    if (!hostSession_ || state_ != State::Ready || cropBuildPending_) {
        setError(errorOut, QStringLiteral("Volume is not ready for capture."));
        return {};
    }

    auto bridge = std::make_shared<scene::VolumeHostBridge>();
    if (!hostSession_->AttachFeature(bridge)) {
        const auto& detail = bridge->GetLastError();
        setError(errorOut, detail.empty()
            ? QStringLiteral("Failed to attach volume capture feature.")
            : QString::fromStdString(detail));
        return {};
    }

    // 先复制 shared_ptr，让快照的生命周期脱离 bridge。
    const auto captured = bridge->GetCapturedImage();

    if (!hostSession_->DetachFeature(*bridge)) {
        setError(errorOut, QStringLiteral("Failed to detach volume capture feature."));
        return {};
    }

    if (!captured || !captured->image) {
        setError(errorOut, QStringLiteral("Captured volume is empty."));
        return {};
    }

    return captured;
}
```

`setError()` 是你当前实现文件中已有的辅助函数。先保存 `captured` 再 Detach，因为 `DetachHost()` 会释放 bridge 自己的引用；调用者持有的 shared_ptr 仍使复制后的图像存活。

## 5. 暂存 A/B，并加入两个验收调用点

这是第 1 步的验证接线，之后由 VolumeManager 接管，不作为最终多体积管理结构。

### 5.1 MainWindow.h

打开 `C:\CT\MVVCVTK\c_ui\MainWindow.h`，显式加入：

```cpp
#include "MVVCVTK/SPI/Data/TrustedImageState.h"
```

在 `CTViewer` 的 `private:` 成员区增加：

```cpp
TrustedImageSnapshot originalSnapshotForTest_;
TrustedImageSnapshot croppedSnapshotForTest_;
```

### 5.2 原始体积加载成功后保存 A

在 `C:\CT\MVVCVTK\c_ui\MainWindow.cpp` 中确认包含：

```cpp
#include <QDebug>
```

找到 `CTViewer::handleLoadFinished()`，把下面代码放在 `if (!issucc) { ... return; }` 之后，即只有加载成功才执行：

```cpp
// 第 1 步验证：新加载的数据成为本轮 A，清理上一轮的 B。
originalSnapshotForTest_.reset();
croppedSnapshotForTest_.reset();

QString captureError;
originalSnapshotForTest_ =
    context_.getSessionManager().captureVolumeSnapshot(&captureError);

if (!originalSnapshotForTest_) {
    qWarning() << "[VolumeCapture] A failed:" << captureError;
}
else {
    const auto& a = originalSnapshotForTest_;
    qDebug() << "[VolumeCapture] A saved"
             << "version=" << a->version
             << "dims=" << a->dims[0] << a->dims[1] << a->dims[2]
             << "hasMask=" << (a->validityMask != nullptr)
             << "image=" << static_cast<const void*>(a->image.GetPointer());
}
```

保留函数后面原有的加载对话框关闭、显隐和视图刷新逻辑。这次只是保存测试快照，保存失败不提前 return 截断已有的加载界面处理。

### 5.3 裁切成功后保存 B

找到 `CTViewer::handleCropBuildFinished()`。将下面代码放在 `if (!isSuccess) { ... return; }` 之后：

```cpp
QString captureError;
const auto captured =
    context_.getSessionManager().captureVolumeSnapshot(&captureError);

if (!captured) {
    qWarning() << "[VolumeCapture] B failed:" << captureError;
}
else {
    croppedSnapshotForTest_ = captured;
    const auto& b = croppedSnapshotForTest_;
    qDebug() << "[VolumeCapture] B saved"
             << "version=" << b->version
             << "dims=" << b->dims[0] << b->dims[1] << b->dims[2]
             << "hasMask=" << (b->validityMask != nullptr)
             << "image=" << static_cast<const void*>(b->image.GetPointer());
}
```

保留后面的三个二维视图重置、各自窗宽窗位恢复以及刷新逻辑。

当前 `SessionManager::applyCrop()` 在发出 `cropBuildFinished` 前已经将 `cropBuildPending_` 清为 false，因此这个完成槽符合快照入口的状态检查。不要将保存 B 的调用放在点击“应用”后立即执行，也不要在 DLL 或裁切 worker 线程中调用它。

## 6. 编译与验收

### 编译

使用项目原有的 Debug/x64 配置、SDK 与 Qt 配置编译。本文代码按当前本地头文件编写，尚未实际编译；不修改 SDK、core 或全局构建配置。若出现编译问题，先定位首个错误及文件行号，不继续加入发布代码。

### 运行

1. 用小数据加载一个原始体积，在 VS“输出”窗口看到 `[VolumeCapture] A saved`。
2. 在界面中裁切并点击“应用”，等成功完成，看到 `[VolumeCapture] B saved`。
3. 断点检查两个测试成员都不为空，两个 `image.GetPointer()` 不同；进入 bridge 的复制位置时，还应确认归档图像与当次来源图像指针不同，mask 存在时归档与来源 mask 指针也不同。
4. 检查 `dims`、`spacing`、`origin` 和实际 image 的 extent/spacing/origin 与各自来源一致。裁切可能只改变 mask，A/B 的尺寸和图像体素可能相同，这不代表保存失败。
5. 如果 B 带有 mask，检查其 extent、分量以及已知裁切区域的有效性值是否与捕获时 SDK 的 mask 一致；原始 A 没有 mask 是允许的。若 B 没有 mask，先确认产物实际是物化图像还是缺失了必要 mask，不仅凭指针非空判断通过。
6. 在继续编辑前记录 A 以及已保存 B 的若干图像、mask 样本值，后续裁切只编辑尚未应用的预览，再检查归档值保持不变。本验证接线每次成功应用会替换测试槽 B；后续验证多个裁切产物保留由 VolumeManager 承担。
7. 再次加载新数据时，本轮测试槽 A 更新、B 清空。这是测试用途，之后不能用它替代支持多个对象的 VolumeManager。

### 本小步通过条件

- A 和 B 可分别取得并在 bridge 卸载后保持存活。
- 图像和存在的 mask 均已独立复制，元信息完整。
- 后续预览编辑不改写已保存归档；失败时输出明确错误。
- 现有加载与裁切流程仍能正常结束。

**通过 1.1 只证明“保存正确”，还不证明“切换成功”。下一小步再增加发布入口，验证 A/B 恢复、四视图版本和 mask 同步，以及恢复后继续裁切。**
