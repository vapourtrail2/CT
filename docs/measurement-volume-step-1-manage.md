# 第 1.3 步：让 VolumeManager 保存多个体积

## 先说清楚这一步要解决什么

1.1 解决了“把当前体积保存下来”，1.2 解决了“把保存的体积重新发布给 SDK”。你提供的日志已经显示 A/B 发布后四视图版本一致；这说明可以继续建立前端体积管理。像素内容和 mask 的显示效果仍以你实际观察的验收结果为准。

现在 `MainWindow` 只有 `originalSnapshot_` 和 `croppedSnapshot_` 两个槽，每次裁切都会覆盖后者。要保留多个结果，就需要一个集合：

```text
当前：A 槽 + B 槽，下一次裁切覆盖 B。

改后：
VolumeManager
├─ 1：原始体积 A
├─ 2：裁切体积 B，来源 1
└─ 3：裁切体积 C，来源 2

activeVolumeId：当前 SDK 使用的体积身份。
```

**本步只做体积记录、来源关系和激活 ID，不接场景树，也不改测量算法。** 后面每体积的测量集合就在这些记录上增加。一个体积可以有很多测量对象，这一步不会把体积限制成只能拥有一个对象。

这里有四个不同动作：

| 动作 | 谁负责 | 做什么 |
|---|---|---|
| 保存快照 | 已有捕获链路 | 从 SDK 复制图像和 mask |
| 登记体积 | 新增 VolumeManager | 为快照分配稳定 ID，保存名称和来源 |
| 发布数据 | 已验证的 1.2 链路 | 将指定记录的图像重新发布给 SDK |
| 确认激活 | 调用发布流程的一方 | 发布和四视图同步完成后，记录当前体积 ID |

**VolumeManager 只管理记录，不操作 SDK、不刷新窗口、不管理定时器。** 已经跑通的 1.2 暂时保留，本步仅调整它读取哪份快照，以及成功后记录哪个 ID；不再添加 Test 类型的 SessionManager 公共接口。

下面是手动修改教程，业务源码未自动修改。示例按当前类名和函数名编写，尚未编译验证。

## 1. 新增 VolumeManager.h

在现有 `c_ui → context` 筛选器中添加头文件，磁盘位置填 `C:\CT\MVVCVTK\c_ui\context`。文件名为 `VolumeManager.h`。

```cpp
#pragma once

#include "MVVCVTK/SPI/Data/TrustedImageState.h"
#include <QString>
#include <cstdint>
#include <map>

using VolumeId = std::uint64_t;

struct VolumeRecord {
    VolumeId id = 0;
    QString name;
    TrustedImageSnapshot image;
    VolumeId sourceVolumeId = 0;
};

class VolumeManager {
public:
    VolumeId addVolume(QString name, TrustedImageSnapshot image, VolumeId sourceVolumeId = 0);
    const VolumeRecord& getVolume(VolumeId id) const;
    const std::map<VolumeId, VolumeRecord>& getVolumes() const;

    VolumeId getActiveVolumeId() const;
    void setActiveVolumeId(VolumeId id);

private:
    std::map<VolumeId, VolumeRecord> volumes_;
    VolumeId nextVolumeId_ = 1;
    VolumeId activeVolumeId_ = 0;
};
```

`id` 是前端身份，`sourceVolumeId` 是来源体积，0 表示无来源。`activeVolumeId_ = 0` 表示当前没有已确认的激活体积。

`image` 保存 1.1 已经冻结的快照。登记时只接管 shared_ptr，不再深拷贝体素；发布时仍按 1.2 复制工作数据，避免改写归档。

这里使用 map 直接按 ID 查记录，ID 不与行号或 SDK 数据版本绑定。后面即使增加树节点排序，也不会改变对象身份。

## 2. 新增 VolumeManager.cpp

在相同筛选器、相同磁盘目录添加 `VolumeManager.cpp`：

```cpp
#include "VolumeManager.h"
#include <utility>

VolumeId VolumeManager::addVolume(QString name, TrustedImageSnapshot image, VolumeId sourceVolumeId)
{
    const VolumeId id = nextVolumeId_;
    volumes_.emplace(id, VolumeRecord{id, std::move(name), std::move(image), sourceVolumeId});
    ++nextVolumeId_;
    return id;
}

const VolumeRecord& VolumeManager::getVolume(VolumeId id) const
{
    return volumes_.at(id);
}

const std::map<VolumeId, VolumeRecord>& VolumeManager::getVolumes() const
{
    return volumes_;
}

VolumeId VolumeManager::getActiveVolumeId() const
{
    return activeVolumeId_;
}

void VolumeManager::setActiveVolumeId(VolumeId id)
{
    activeVolumeId_ = id;
}
```

`addVolume()` 只保存一条新记录，不偷偷激活它。是否激活由调用方在正确的完成位置决定。

管理器内部不重复检查已确认的快照和自己产生的 ID。调用契约是：先捕获成功再登记，只用已登记 ID 取得记录；`getVolume()` 的 `at()` 会暴露错误 ID，而不是吞掉错误并返回一个空记录。UI 尚未选择对象、SDK 捕获失败等真实入口状态仍在调用处处理。

通过 VS 添加新建项后，无需手动写项目 XML。这两个文件没有 `Q_OBJECT`，不需要 MOC。先编译一次，确认类已正确加入项目。

## 3. 让 AppContext 持有管理器

打开 `C:\CT\MVVCVTK\c_ui\context\AppContext.h`。

增加 include：

```cpp
#include "c_ui/context/VolumeManager.h"
```

在 `public:` 增加：

```cpp
VolumeManager& getVolumeManager()
{
    return volumeManager_;
}

const VolumeManager& getVolumeManager() const
{
    return volumeManager_;
}
```

在 `private:` 成员区增加：

```cpp
VolumeManager volumeManager_;
```

这和当前 `Commands`、`SessionManager` 的放置方式一致。以后主界面通过 `context_.getVolumeManager()` 访问同一个集合。

## 4. MainWindow 用 ID 代替两个快照槽

打开 `C:\CT\MVVCVTK\c_ui\MainWindow.h`。

删除两个旧成员：

```cpp
TrustedImageSnapshot originalSnapshot_;
TrustedImageSnapshot croppedSnapshot_;
```

改成三个身份成员：

```cpp
VolumeId originalVolumeId_ = 0;
VolumeId latestCroppedVolumeId_ = 0;
VolumeId cropSourceVolumeId_ = 0;
```

它们分别是本轮加载的原始体积、最近一个裁切产物，以及正在进行的裁切的来源。**这里只保存入口用的 ID，所有体积图像都在 VolumeManager 中。** 最近产物 ID 改变，不会删除之前的产物。

把现有函数声明：

```cpp
void switchVolumeForTest(bool useoriginal);
```

改成：

```cpp
void switchVolumeForTest(VolumeId volumeId);
```

`AppContext.h` 已包含新类型，MainWindow 现有 include 链能够取得 VolumeId，不用再引入新分组或工厂类。

## 5. 加载完成后登记原始体积

打开 `C:\CT\MVVCVTK\c_ui\MainWindow.cpp`，找到 `handleLoadFinished()`。

只替换加载成功后的 `//test` 保存与日志段，保留前面的失败处理及后面的对话框关闭、显隐、刷新逻辑：

```cpp
auto& volumes = context_.getVolumeManager();
volumes.setActiveVolumeId(0);
originalVolumeId_ = 0;
latestCroppedVolumeId_ = 0;
cropSourceVolumeId_ = 0;

QString error;
const auto image = context_.getSessionManager().capturedVolumeSnapshot(&error);

if (image) {
    originalVolumeId_ = volumes.addVolume(QStringLiteral("原始体积"), image);
    volumes.setActiveVolumeId(originalVolumeId_);
    qDebug() << "[Volume] loaded id=" << originalVolumeId_ << "count=" << volumes.getVolumes().size();
}
else {
    qWarning() << "[Volume] capture failed:" << error;
}
```

这里 SDK 已经通过加载完成回调确认输入，因此捕获成功后登记并记录该体积身份。捕获失败时不把旧体积误标成新加载数据，也不新增空记录。

本步再次成功加载文件会追加一个新的原始记录，保留已登记记录；两个快捷键的入口 ID 则更新为新一轮加载的数据。正式“新建工作区/清空全部”生命周期在后续统一接入，本步不加入删除和清空接口。

## 6. 点击应用时记住裁切来源

为什么不能在完成回调里直接猜来源？因为裁切是异步操作，结果属于发起时的体积。来源 ID 应当在点击“应用”时确定。

在 `connectAppSignals()` 中找到 `SceneTreePanel::cropApplyRequested` 对应的 lambda。紧跟这行后面插入：

```cpp
auto& sessionManager = context_.getSessionManager();
```

插入内容：

```cpp
cropSourceVolumeId_ = context_.getVolumeManager().getActiveVolumeId();
if (cropSourceVolumeId_ == 0) {
    statusBar()->showMessage(QStringLiteral("当前没有已登记的激活体积。"), 3000);
    return;
}
```

这是需要保留的真实入口判断：没有登记成功的体积，就不能为裁切结果填写来源。

保留原有保存三个窗宽窗位及调用 `applyCrop()` 的代码。在现有 `if (!context_.getSessionManager().applyCrop())` 分支中增加：

```cpp
cropSourceVolumeId_ = 0;
```

任务没有启动，就清掉这次暂存来源。

本步继续按已验证的加载、裁切完成后再开始下一操作的流程验收；正式场景树交互接入时，再统一处理加载与编辑的任务互斥、取消和过期回调，不在管理器中增加一套任务框架。

本轮验收走“加载 → 应用裁切 → 体积快捷键切换”。现有“恢复原始”按钮尚未接入体积身份管理，不用于本轮验收；后续统一通过激活已登记体积表达数据恢复，避免只恢复 SDK 数据却遗漏前端激活身份。

## 7. 裁切完成后新增记录，不覆盖旧结果

找到 `handleCropBuildFinished()`，在函数开头、原有成功失败判断之前加入：

```cpp
const VolumeId sourceVolumeId = cropSourceVolumeId_;
cropSourceVolumeId_ = 0;
```

保留原有 `if (!isSuccess)` 分支。成功后，把当前 `QString captureError;` 到 `[VolumeCapture] B saved` 的整段保存代码替换为：

```cpp
auto& volumes = context_.getVolumeManager();
volumes.setActiveVolumeId(0);

QString error;
const auto image = context_.getSessionManager().capturedVolumeSnapshot(&error);

if (image) {
    const QString name = QStringLiteral("裁切体积 %1").arg(static_cast<qulonglong>(volumes.getVolumes().size() + 1));
    latestCroppedVolumeId_ = volumes.addVolume(name, image, sourceVolumeId);
    volumes.setActiveVolumeId(latestCroppedVolumeId_);
    qDebug() << "[Volume] cropped id=" << latestCroppedVolumeId_ << "source=" << sourceVolumeId << "count=" << volumes.getVolumes().size();
}
else {
    qWarning() << "[Volume] crop capture failed:" << error;
}
```

保留其后原有的三个二维视图重置、独立窗宽窗位恢复和刷新逻辑。命名后缀暂按当前登记数量递增；本步没有删除记录，不把这个显示名称当作身份。

此时有两种不同结果：

```text
在 A 上裁切：登记 B，sourceVolumeId = A。
在 B 上继续裁切：登记 C，sourceVolumeId = B，原来 B 的记录仍在。
```

不要把新快照写回 `getVolume(sourceVolumeId).image`，也不要在切换后用 published 结果覆盖原记录。归档保持稳定，新产物新增记录。

## 8. 原有快捷键改为传 ID

在构造函数中，把两个快捷键 lambda 的调用改成：

```cpp
switchVolumeForTest(originalVolumeId_);
```

```cpp
switchVolumeForTest(latestCroppedVolumeId_);
```

快捷键仍分别是 `Ctrl+Alt+1` 和 `Ctrl+Alt+2`。以后场景树只需传被选体积的 ID，可以激活任意已登记记录，不再需要 `bool useOriginal`。

## 9. 让已经跑通的发布流程读取体积记录

这一步不重写整个 `switchVolumeForTest()`，只改四个位置。

### 9.1 修改函数开头

从函数签名到原来的 `const QString name = ...;`，替换成：

```cpp
void CTViewer::switchVolumeForTest(VolumeId volumeId)
{
    if (volumePublishTestBusy_) return;

    if (volumeId == 0) {
        statusBar()->showMessage(QStringLiteral("请先生成对应的体积。"), 3000);
        return;
    }

    auto& volumes = context_.getVolumeManager();
    const auto& volume = volumes.getVolume(volumeId);
    const auto archive = volume.image;
    const QString name = volume.name;
```

上面是函数开头片段，后面继续保留原函数的发布和定时检查代码，不在这里另加一个右花括号。这里只有“正在切换”和“入口还没有对象”两个既有 UI 判断，内部不重复验证管理器产生的 ID。

### 9.2 发布前暂时清掉确认身份

在现有的下面两行之后：

```cpp
volumePublishTestBusy_ = true;
setEnabled(false);
```

增加：

```cpp
volumes.setActiveVolumeId(0);
```

发布和四视图同步尚未完成时，先不把某个 ID 宣称为当前已确认身份。现有测试失败分支保持不变，后续正式控制器再负责恢复上一个激活记录。

### 9.3 定时检查 lambda 捕获 volumeId

把原来的捕获列表改成：

```cpp
[this, timer, attempts, expectedVersion, name, volumeId]() {
```

现有可以单行表达的版本检查写在一行：

```cpp
const bool synchronized = session.areAllRenderViewsAtVersion(expectedVersion, &report);
```

其余定时器逻辑本步不扩大、不重写。

### 9.4 只在四视图同步成功后确认激活 ID

在现有 `if (synchronized)` 成功分支的第一行增加：

```cpp
context_.getVolumeManager().setActiveVolumeId(volumeId);
```

在该成功分支中增加一条有业务含义的日志：

```cpp
qDebug() << "[Volume] active id=" << volumeId << "name=" << name << "version=" << expectedVersion;
```

不要在刚调用发布函数时就把目标 ID 设为激活，也不要根据 `publishedVersion` 创建新体积记录。切换旧对象与生成新对象是两件事。

本次仍借用已有 1.2 的受控发布检查来验收集合接入；它不是 VolumeManager 的依赖。后续把测试入口换成正式激活流程时，再收拢临时接口和 UI 测试代码。

## 10. 按这条路线验收

1. 先加入 VolumeManager 和 AppContext 持有关系，编译确认新文件及头文件引用正确。
2. 再完成 MainWindow 的替换，确认源码中不再使用 `originalSnapshot_`、`croppedSnapshot_` 或 `bool useOriginal` 选择数据。
3. 重启程序，用小数据加载 A。应登记一个体积，active ID 为 A；对新进程通常是 ID 1。
4. 在 A 上应用裁切得到 B。应登记第二个体积，来源为 A，A 的记录与归档仍在。
5. 用快捷键切回 A、再切回 B。记录数不增加，激活 ID 在成功同步后改变；SDK version 可以继续递增，VolumeId 不变。
6. 在激活的 B 上继续裁切得到 C。记录数变为 3，C 的来源是 B，不是最近加载的 A。原来的 B 不被覆盖。
7. 在断点中检查 `context_.getVolumeManager().getVolumes()`：三条记录各有独立 ID、名称、归档和来源。`getVolume(B的ID)` 仍能取得 B；后续场景树会为它提供直接激活入口。
8. 检查 `getActiveVolumeId()` 与当前发布对象一致，四视图画面和 mask 与 1.2 一样正常。

本步不要求写镜像实现的单元测试，用已有四视图链路和上述身份、来源验收验证实际行为。新增代码尚未编译或运行；你写完后再检查具体编译结果与日志。

## 本步完成后的结构

```text
VolumeManager：保存所有体积记录和激活 ID。
SessionManager + Bridge：继续负责与 SDK 交互。
MainWindow：暂时协调加载、裁切登记和测试切换。
```

**下一小步才给每条 VolumeRecord 加独立 MeasurementSession，并把现有测量工具连接到激活体积。** 不把测量对象放在本步的临时快捷键或两个入口 ID 中。
