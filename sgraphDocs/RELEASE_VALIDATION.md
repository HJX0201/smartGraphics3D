# v0.1.0-beta.1 发布验证

验证日期：2026-07-27。

## 构建

| 配置 | 完整重建 | CTest | Qt 平台插件 | OCCT 依赖闭包 |
| --- | --- | ---: | --- | ---: |
| Windows x64 Debug | 通过 | 7/7 | `qwindowsd.dll` | 32 DLL |
| Windows x64 Release | 通过 | 7/7 | `qwindows.dll` | 32 DLL |

两次完整重建均使用整理后的工具链暂存目录和使用者安装的 Qt 5.12.10，不依赖原始 OCCT
安装目录。主程序位于 `bin/`，七个测试程序只位于 `tests/`。

构建参数包含 `/W4 /permissive- /Zc:__cplusplus /utf-8`。clang-format dry-run 通过；
clang-tidy 退出码 0；87 个自研 C++ 文件均不超过 800 行，最大文件 682 行。

构建脚本单元测试 4/4 通过，覆盖 Qt 显式与自动候选、Qt 6/错误位数/完全缺失拒绝，
以及 `windeployqt` 缺失或失败的回退部署。

## 文件格式

- `.sg3d`、Magic `SGRAPH3D`、版本 3 保存重开通过。
- `.sg3d.autosave` 新恢复文件通过。
- 截断、损坏、未来版本、错误 Magic 及旧版扩展名拒绝路径通过。
- `.sgdiag` 诊断扩展名和统一 CMake 版本头已验证。

## 性能

正式模型包含 5000 个实体和每份 2,342,442 个三角形，只复制 1/2/5/10 份。10 份共享
模式相对普通模式：

- Private Bytes：减少 74.37%；
- Working Set：减少 58.28%；
- GPU 几何估算：减少 90.00%；
- 展开三角形完全相同；
- 30 帧重绘相差 0.03%。

完整 HTML、汇总 JSON、24 次原始 JSON 和 BREP 哈希位于
`sgraphBenchmarks/instanceCopy/`。

## 工具链附件

本机没有 7-Zip，因此按设计回退为 ZIP：

- 文件：`smartGraphics3D-toolchain-v0.1.0-windows.zip`
- 大小：336.63 MiB
- SHA-256：`78c1fc974358a8eb38ab65cf423bdeffc2671adf7d47c53e41b0e8dd9f7f7ca0`
- Qt DLL：0
- PDB：0
- 许可证：OCCT LGPL/Exception、CMake、Ninja Apache 2.0、Python

附件位于被忽略的 `artifacts/`，不进入源码提交。
