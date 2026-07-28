# smartGraphics3D 代码架构

## 模块

| 模块 | 职责 | 主要依赖 |
| --- | --- | --- |
| `sgraphCore` | 结果类型、基础数据、坐标与任务 | Qt Core |
| `sgraphKernel` | OCCT 几何创建、运算、测量与物化 | Core、OCCT |
| `sgraphDocument` | 场景对象、事务、快照、复制组 | Core、Kernel |
| `sgraphCommands` | 命令注册和调用 | Core、Document |
| `sgraphIo` | 项目归档与 CAD 编解码适配 | Core、Document、Kernel、OCCT |
| `sgraphRender` | AIS、视口、选择映射、显示资源统计 | Core、Document、Kernel、OCCT |
| `sgraphGui` | Ribbon、对话框、操作编排 | 上述业务模块 |
| `sgraphApp` | 进程入口和应用生命周期 | GUI |
| `sgraphTests` | 七组自动测试 | 对应模块、Qt Test |
| `sgraphBenchmarks` | 独立性能基准和正式数据 | Render、Kernel |

公共接口通过 `SResult<T>` 报告失败，不传播异常。OpenCascade 类型只在 Kernel、Render
和 IO 适配实现中出现，不进入 Document、Commands 或 GUI 公共接口。

## 文档与几何

`S3dDocument` 是业务状态的唯一所有者。命令在事务中批量修改对象，成功后提交一次
撤销记录，失败则不改变已提交状态。对象保存基础 `SKernelShape`、场景变换和
`presentation_group_id`。几何编辑、测量和导出统一从“物化场景几何”入口取得基础
Shape 与对象变换的合并结果。

## AIS 生命周期

普通对象对应独立 `AIS_Shape`。具有相同显示组、几何和有效显示属性的多个可见对象对应
一个未显示的 `AIS_Shape` 原型及多个 `AIS_ConnectedInteractive`。每个 Connected 对象
保留业务对象 ID、选择 Owner 和自身变换。清理时先 Disconnect 并移除实例，最后释放
原型。

选择映射先定位具体实例，再通过底层 `TShape/IsPartner` 匹配原型子形状，从而在实例
Location 不同的情况下仍返回正确的面、边、点索引。

## 持久化边界

`.sg3d` 使用 Magic `SGRAPH3D` 和格式版本 3。版本头由 CMake 单点生成，应用、项目和
诊断信息使用同一个版本。当前 Beta 明确拒绝旧扩展名和旧 Magic，不包含迁移分支。
