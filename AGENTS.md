# smartGraphics3D 项目协作约束

本文件适用于仓库内全部自研代码。构建输出、OpenCascade、Qt 和原样引入的第三方源码
不属于自研代码，不得为了满足项目风格直接修改。

## 强制规则

- 使用 C++17、Qt 5.12.10、OpenCascade 7.7.0。
- 每个自研 `.h`、`.cpp` 文件最多 800 个物理行；达到约 700 行时按单一职责拆分。
- 控制语句、函数、类型和命名空间使用 Allman 风格，左大括号必须另起一行。
- 自研模块目录使用 `sgraph` 前缀；文件使用 `s_` 加小写下划线。
- 类型使用 `S` 前缀，接口使用 `SI` 前缀；私有成员使用 `m_` 前缀。
- OpenCascade 类型仅允许出现在 `sgraphKernel`、`sgraphRender` 和 `sgraphIo` 适配实现中，
  不得泄漏到文档、命令、GUI 公共接口。
- 公共 API 不传播异常，失败通过 `SResult<T>` 返回。
- 非 QObject 禁止裸拥有指针；QObject 使用父子所有权，弱引用使用 `QPointer`。
- 修改自研 C++ 后必须编译；新增功能必须包含核心逻辑、边界和错误路径测试。

## 提交门槛

- 通过 `.clang-format` 和 `.clang-tidy`。
- 通过 `/W4 /permissive- /Zc:__cplusplus /utf-8`。
- 通过 Debug/Release 构建和全部自动测试。
- 不提交构建产物、第三方 SDK、无授权字体、模型或客户数据。
