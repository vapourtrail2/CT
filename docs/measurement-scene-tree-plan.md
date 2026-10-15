# 多体积管理、主视图测量与通用场景树方案

## Summary

本次改造以“体积对象”为核心，将现有单份当前数据的结构调整为：**前端保存多个体积，主视图一次激活一个体积，每个体积独立管理自己的测量结果。**

加载数据生成原始体积 A。在 A 上成功应用裁切后，生成独立体积 B，并记录 B 来源于 A。A 的测量继续归属于 A，B 从空测量集合开始，不继承、不移动、不重新计算 A 的测量。裁切预览和操作记录不代表新体积，只有成功生成裁切产物才新增体积对象。

```text
场景
├─ 裁切体积 B
│  ├─ 抓边结果 1
│  └─ 抓边结果 2
└─ 原始体积 A
   ├─ 测量线段 1
   └─ 测量线段 2
```

场景树按所属体积组织测量，单击用于选择和查看属性，激活体积才切换主视图中的数据。测量与裁切编辑互斥，关闭工具或切换体积不会删除完成对象。

手工线、圆、圆弧，以及确认后的抓边结果，统一进入所属体积的测量集合，共用显隐、重命名、删除、撤销和重做。抓边拖动只更新预览，确认后生成正式对象。暂不增加独立点工具。

主要工作在前端完成，通过现有 SDK 的 Feature 接口读取、发布图像状态并接入主视图。**必须先验证完整体积切换及视图同步，再展开场景树和测量交互改造。** 本文为基于当前代码与 SDK 头文件的设计，尚未修改业务源码或完成运行验证。

## 1. 现有代码需要解决的问题

| 当前实现 | 需要改变的行为 |
|---|---|
| `MeasureToolDialog` 创建自己的 `MeasurementSession`，关闭时释放 | 由体积对象长期持有 session，工具只借用 |
| 手工测量进入 `MeasurementEntity`，抓边结果留在控制器内部 | 正式抓边结果统一提交到所属体积的 session |
| `SceneTreePanel::rebuildTree()` 清空重建，操作绑定裁切 | 改为通用场景树模型，按业务对象变化更新 |
| 裁切更新 SDK 当前数据，没有登记独立前端体积 | 裁切成功后保存产物并登记新的体积对象 |
| 主界面只管理一份当前数据 | 保存多个体积，建立完整的激活与切换流程 |

树节点引用稳定业务对象，不持有 VTK actor。actor 是渲染层依据对象生成的显示内容，可以随刷新重新创建。

## 2. 对象归属与模块边界

### 2.1 新增前端组件

| 组件 | 职责 |
|---|---|
| `VolumeManager` | 保存体积对象、来源关系、测量集合和激活体积 ID |
| `VolumeHostBridge` | 通过 SDK 读取、发布完整图像状态 |
| `WorkspaceController` | 协调加载登记、体积切换、裁切产物登记和工具互斥 |
| `SceneTreeModel` | 将体积及其所属对象组织为树节点，提供操作能力描述 |

`AppContext` 持有这些组件。现有 `SessionManager` 继续负责 SDK 会话和请求；控制器协调管理器，管理器之间不互相持有。场景树不直接调用 SDK，测量模块不操作树节点或裁切模块。

```text
AppContext
├─ VolumeManager ── 保存体积与每体积测量集合
├─ SessionManager ─ SDK 会话和请求
└─ WorkspaceController
   ├─ 协调场景树操作
   ├─ 协调测量输入与 overlay
   └─ 调用 SessionManager 完成数据与裁切操作
```

### 2.2 体积对象

建议在 `C:\CT\MVVCVTK\c_ui\scene` 新增 `VolumeTypes.h`、`VolumeManager.h/.cpp`。以下类型是拟新增的前端设计，不是已有 SDK 类型，也不是可直接编译的完整实现：

```cpp
using VolumeId = std::uint64_t; // 0 表示无效

struct VolumeViewState {
    std::array<std::optional<HostWindowLevelParams>, 3> windowLevels;
    std::optional<std::array<double, 3>> cursorWorld;
};

struct VolumeObject {
    VolumeId id = 0;
    QString name;
    QString sourcePath;
    std::optional<VolumeId> sourceVolumeId;

    TrustedImageSnapshot archivedImage;
    VolumeViewState viewState;
    std::shared_ptr<measure::MeasurementSession> measurements;
};
```

约束：

- `VolumeId` 表示业务身份，SDK 的 `dataVersion` 用于检查输入是否变化，不能互相代替。
- 保存完整图像状态，包括 `image`、`validityMask` 和物理元信息。
- 每个新体积创建新的 `MeasurementSession`，测量引用使用 `{volumeId, measurementId}`。
- 切回同一体积可以获得新的 SDK 数据版本，但保留原体积 ID 和测量集合。
- 三个二维窗宽窗位独立保存，不合并成一个共享值。
- 首阶段原始体积及其裁切产物共用物理坐标体系，不实现每体积独立移动、旋转及位置恢复；overlay 使用当前视图提供的模型变换。

归档数据不能被后续裁切改写。先以深拷贝图像和 mask 建立隔离基线，发布时使用工作副本；用小数据验证。大体积正式使用前，再依据 SDK 不可变共享契约优化复制和内存，不能仅凭 `shared_ptr<const ...>` 判断 VTK 数据不会被修改。

## 3. 数据链路具体怎么改

### 3.1 先验证 VolumeHostBridge

沿用现有 `MeasureHostBridge : HostFeature` 的结构，封装 SDK 已有能力：

```text
读取：context.data->GetImageSnapshot()
发布：context.data->SetImageState(imageState, expected, published)
```

发布时读取当前 SDK 快照作为 `expected`，提交目标体积工作副本，接受返回的 `published` 及新版本，不强行恢复旧版本号。

`SetImageState()` 成功只证明数据发布结果，不能直接视为主视图切换完成。需要通过 SDK 支持的更新路径驱动刷新，并检查四视图的 `dataVersion` 与此次发布一致。如果现有接口无法完成数据与视图同步，再要求 SDK 补齐此能力，不能将单纯调用 `Render()` 当作切换成功。

不使用 `HostReloadRequest` 代替完整状态发布，因为它没有完整表达裁切 mask 的字段。

### 3.2 加载完成后登记原始体积

接入位置：`C:\CT\MVVCVTK\c_ui\MainWindow.cpp` 的 `CTViewer::handleLoadFinished()`。

```text
加载成功
→ 获取并冻结完整快照
→ 登记原始体积 A
→ 创建 A 的测量集合
→ 确认当前激活 A
→ 通知树模型新增体积节点
```

同时调整 `handleSessionChanged()` 与 `WorkspacePage::setDataState()`：会话状态决定界面是否可操作；已登记体积节点由 `VolumeManager` 管理，不能因 Loading、Failed 或切换状态被清空。加载失败不登记新体积，明确清空工作区时才清理登记对象。

### 3.3 裁切成功后登记独立产物

接入位置：`C:\CT\MVVCVTK\c_ui\context\SessionManager.cpp` 的 `SessionManager::applyCrop()`。

发起操作时冻结来源体积 ID、输入版本、操作编号及会话代次。增加携带产物信息的通知；原 `cropBuildFinished(bool, QString)` 可以继续服务进度和界面恢复。

```text
来源 A 上编辑裁切
→ 应用裁切
→ owner 线程取得成功发布后的完整产物快照
→ 校验操作和会话仍有效
→ 登记体积 B，sourceVolumeId = A
→ 创建 B 的空测量集合
→ 结束当前裁切编辑
→ 确认 B 为激活体积
```

只有成功产物登记为体积。预览和每条裁切步骤不新增体积，失败不改变 A 的测量。

生成 B 后结束本次裁切编辑。之后在 B 上继续裁切时，以 B 建立新的裁切上下文；切换体积不恢复未完成的裁切操作历史。

### 3.4 体积激活采用完整流程

新增统一入口 `WorkspaceController::ActivateVolume(id)`：

```text
检查没有加载、裁切发布或其他切换任务
→ 保存当前三个二维窗宽窗位和 cursor
→ 取消测量草稿、抓边预览，释放输入接管
→ 停止依赖旧数据的测量、裁切及 Gap Feature
→ 发布目标体积工作副本
→ 确认四个视图使用新数据
→ 重建必要 Feature
→ 恢复目标体积视图状态
→ 绑定目标测量集合和 overlay
→ 更新 activeVolumeId
→ 恢复交互
```

首次激活无缓存的体积时，采用有效默认 cursor 并重置相机。不要在未确认发布完成时提前更新激活 ID。

发布前失败保持旧体积；发布后失败尝试恢复旧体积。恢复仍失败时禁用编辑并显示错误，避免树显示 A、实际数据却是 B。

场景树“激活原始体积 A”走该入口，不等同于 SDK 裁切上下文的 `RestoreOriginal`。

## 4. 测量与抓边怎么改

### 4.1 先将 session 移出弹窗

修改 `C:\CT\MVVCVTK\measure\MeasureToolDialog.h/.cpp`：

- 构造函数接收外部 `shared_ptr<MeasurementSession>`。
- `BuildMeasurementViewport()` 不再创建 session。
- 打开工具时由控制器取得激活体积的 session。
- 关闭弹窗只解绑回调和销毁视口，完成对象仍由体积管理器持有。

先用独立窗口验证 A、B 的测量互不影响，再迁移主视图，避免同时修改数据归属和输入系统。

### 4.2 统一正式对象提交

修改 `C:\CT\MVVCVTK\measure\MeasurementSession.h/.cpp`：

- 增加正式抓边结果提交入口，共用 ID、删除、撤销和重做。
- 增加名称、可见性、来源方式等对象属性。
- 将单个 `SetChangedCallback()` 改为可订阅通知，至少区分正式对象变化与草稿预览变化，避免树和视图互相覆盖订阅，也避免鼠标移动刷新整树。
- 任务开始冻结体积、输入版本、视图和平面，提交时重新校验。

抓边操作采用“拖动预览 → 确认结果 → 新增正式对象”。未经确认的计算不生成节点。

### 4.3 抓边的物理值

当前 `ZcEdgeAlgorithm` 使用单位比例，封装返回像素几何；画绿线时转换物理位置，并不等于已经计算和保存物理长度。抓圆当前也明确显示像素半径。

线段正式提交链路：

```text
DLL 像素端点
→ 还原图像索引、extent 偏移和 Y 方向
→ 使用所属体积图像转换为物理点 P1、P2
→ ComputeLine({P1, P2})
→ 提交该体积的 MeasurementSession
```

普通正交图像下，平面两个方向 spacing 为 su、sv 时：

```text
L = sqrt((Δu × su)² + (Δv × sv)²)
```

相机缩放倍数不参与计算。圆和圆弧需保留真实几何表达；spacing 不相等时，像素圆变换后不一定是物理圆，不能直接乘一个比例声称获得物理圆半径。

当前抓边输入由体素、mask 和窗宽窗位生成。相机缩放不改变输入图像，但窗宽窗位可能改变重新抓边的结果；已确认对象保留既有结果，重新计算应为明确操作。

### 4.4 再接入主视图

新增主视图测量 Feature 和适配器，使用 SDK 的输入绑定、renderer 和 overlay 端口。将 handler、抓边控制器、overlay 对具体 `MeasureViewAdapter` 的依赖，改为小型视图接口，提供当前图像、切片平面、窗宽窗位、坐标转换及刷新能力。

现有独立视口适配器和主视图适配器分别实现该接口。**不能对现有主视图直接调用 `MeasureViewAdapter::Build()`，它会创建并替换 renderWindow。**

## 5. 通用场景树与交互规则

### 5.1 树模型

将 `C:\CT\MVVCVTK\c_ui\panels\SceneTreePanel.cpp` 的清空重建结构改为 `QTreeView + SceneTreeModel`。节点保存稳定业务引用、名称、类型和操作能力，例如：

```cpp
struct SceneObjectRef {
    VolumeId volumeId;
    SceneNodeKind kind; // Volume / Measurement / CropHistory
    std::uint64_t localId;
};
```

节点不保存图像、SDK 端口或 VTK actor。测量对象与裁切操作历史共用树展示，但保留各自业务语义。裁切历史操作执行前校验所属体积和当前裁切上下文。

裁切按钮放入专属编辑区，仅在裁切编辑时显示。选择测量节点显示测量属性。后续孔隙、标注等模块通过节点描述和操作路由接入，业务数据仍归各模块。

### 5.2 三个独立状态

| 状态 | 含义 |
|---|---|
| 选中对象 | 树中正在查看的对象 |
| 激活体积 | 主视图实际使用的数据 |
| 当前工具 | 导航、手工测量、抓边或裁切 |

| 用户操作 | 行为 |
|---|---|
| 单击体积 | 选择和查看属性，不发布数据 |
| 双击体积或“激活” | 请求控制器完成体积切换 |
| 单击测量 | 查看结果；所属体积激活时高亮 |
| 开始测量 | 在激活体积上创建草稿 |
| 缩放、平移 | 改变展示，不修改完成对象几何和数值 |
| 已取点后尝试滚轮换层 | 拦截目标视图换层，保持草稿平面 |
| 外部操作改变切片或切换体积 | 取消草稿，避免跨平面提交 |
| 切换测量与裁切 | 结束当前编辑，再启用另一工具 |
| Escape | 取消当前草稿或预览，退出当前工具 |

未启用测量时放行原有导航。现有 `MeasurementInteractionHandler` 在无活动测量时也消费左键和滚轮，迁移时必须修正。一次事件不能同时被测量和裁切或导航接管。

完成对象只叠加在所属激活体积、来源二维视图及对应切片上；非激活体积的对象仍保留在树中。

## 6. 实施顺序与验收

| 顺序 | 修改内容 | 验收标准 |
|---|---|---|
| 1 | 完整快照保存与发布验证 | A→B→A 往返，四视图和 mask 正确，再次裁切正常 |
| 2 | VolumeManager 与加载、裁切登记 | 体积稳定保存，后续操作不改写归档，失败不产生错误节点 |
| 3 | 每体积独立 MeasurementSession | 工具关闭重开不丢结果，A/B 删除和撤销互不影响 |
| 4 | SceneTreeModel 和操作路由 | 树归属正确，裁切刷新不删除测量节点 |
| 5 | 主视图测量 Feature 与输入协调 | 导航、取点、抓边、裁切不重复消费事件 |
| 6 | 抓边确认提交与物理值 | 预览不新增对象，确认一次新增一个，已知 spacing 下数值正确 |

补充验证：加载失败、裁切失败、切换失败、过期回调、清空会话、缩放后继续取点、三个独立窗宽窗位，以及 Feature 卸载后没有旧 renderer 和输入绑定残留。

新增源文件加入 `.vcxproj/.filters`；带 `Q_OBJECT` 的类按项目现有方式配置 MOC。

## 7. 首阶段范围与验证边界

- 支持原始体积与多个裁切产物分别保存、切换和测量。
- 不增加独立点工具、三维测量、多个体积同时叠加或项目文件保存。
- 不恢复跨体积切换前未完成的裁切编辑历史。
- 修改范围为前端和测量模块，优先使用现有 SDK，不直接修改 core。
- 现有接口为完整发布提供候选路径；发布后的渲染同步仍须运行验证。验证不通过时，明确向 SDK 侧提出所缺的同步发布能力。

**先完成第 1 步的数据链路验证，再展开后续改造。本文描述的是待实施设计，不代表已经编辑源码、编译通过或完成运行验收。**
