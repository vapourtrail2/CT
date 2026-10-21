# MVVCVTK 后端源码关联

- 后端仓库：https://github.com/HelenTung/MVVCVTK
- 后端源码目录：https://github.com/HelenTung/MVVCVTK/tree/master/MVVCVTK
- 本次查阅日期：2026-10-21。
- 本次查阅的 master 提交：`e7a34027a675802c80b151fc4172f012cae66991`。

本文件记录当前 CT 项目与后端源码的参考关系，不执行源码下载或构建方式迁移。

## 当前集成方式

`MVVCVTK/MVVCVTK.vcxproj` 导入 `MVVCVTK.Sdk.props`，通过
`MVVCVTKSdkRoot` 或 `MVVCVTK_SDK_ROOT` 指定 SDK，使用 SDK 的头文件、库及运行时。
远端 master 源码不代表当前 SDK 二进制的精确版本。

`.gitmodules` 中留有 `MVVCVTK/core/MVVCVTK` 到上述仓库的地址配置，但本次检查时
该路径没有源码目录，也没有 `git ls-files --stage` 返回的 gitlink 条目；不能把它当作已初始化的子模块。

## 视图角色与模式

`HostRenderViewRole::Primary3D` 表示主 3D 视图角色；`Composite3D` 是另一种 3D
视图角色，可用来标记额外的组合视图。角色用于视图查找和路由，具体渲染方式由
`HostRenderMode` 决定。当前 CT 的 `ViewportGather::getHostConfig()` 为
`primary-3d` 设置 `Primary3D` 角色，并使用 `CompositeIsoSurface` 或
`CompositeVolume` 模式。

后端证据（固定提交）：

- [两种角色都属于 3D](https://github.com/HelenTung/MVVCVTK/blob/e7a34027a675802c80b151fc4172f012cae66991/MVVCVTK/src/Host/HostViewRuntimeRegistry.cpp#L1642-L1647)
- [主端点优先 Primary3D，其次 Composite3D，再取首端点](https://github.com/HelenTung/MVVCVTK/blob/e7a34027a675802c80b151fc4172f012cae66991/MVVCVTK/src/Host/VtkAppHostSession.cpp#L1654-L1676)
- [按角色选择时精确匹配，不自动将 Primary3D 替换为 Composite3D](https://github.com/HelenTung/MVVCVTK/blob/e7a34027a675802c80b151fc4172f012cae66991/MVVCVTK/src/Host/HostViewRuntimeRegistry.cpp#L871-L892)
- [示例分别配置角色和模式](https://github.com/HelenTung/MVVCVTK/blob/e7a34027a675802c80b151fc4172f012cae66991/examples/standalone/main.cpp#L1331-L1390)
