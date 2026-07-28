# smartGraphics3D 协作约束

适用于全部自研代码；不得为统一风格修改构建输出、OpenCascade、Qt 或原样引入的第三方源码。

## 代码

- 固定使用 C++17、Qt 5.12.10、OpenCascade 7.7.0。
- 自研 `.h`、`.cpp` 不超过 800 个物理行；约 700 行时按单一职责拆分。
- 控制语句、函数、类型、命名空间均用 Allman 风格。
- 模块目录以 `sgraph` 开头；文件名为 `s_` 加小写下划线；类型、接口、私有成员分别以 `S`、`SI`、`m_` 开头。
- OpenCascade 类型仅限 `sgraphKernel`、`sgraphRender`、`sgraphIo` 适配实现，不得进入文档、命令或 GUI 公共接口。
- 公共 API 不传播异常，失败返回 `SResult<T>`。
- 非 QObject 不得裸拥有指针；QObject 使用父子所有权，弱引用使用 `QPointer`。

## 验证

- 修改自研 C++ 必须编译；新增功能须测试核心逻辑、边界和错误路径。
- 必须通过 `.clang-format`、`.clang-tidy`、`/W4 /permissive- /Zc:__cplusplus /utf-8`、Debug/Release 构建及全部自动测试。
- 不提交构建产物、第三方 SDK、无授权字体、模型或客户数据。

## Git 工作流

- 修改前后检查工作区；仅处理当前需求，不覆盖或夹带无关改动。
- 每完成一个独立功能、需求或项目阶段，执行最小有效测试；自研 C++、新增功能和发布仍须满足上述门槛。
- 未完成、测试失败或存在已知错误时不得提交。
- 测试成功后，打印仅暂存本次明确文件的 Git 指令和准确的提交说明；禁止使用全量暂存。
- Agent 不执行 `git add`、`git commit` 或 `git push`，由开发者操作。
- 不使用破坏性 Git 操作清除他人改动，不擅自改写共享历史。
