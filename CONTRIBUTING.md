# Contributing

当前仓库为 Beta，欢迎提交可复现的 Issue。代码修改请先说明问题、设计边界和测试计划。

提交前必须：

- 使用 C++17、Qt 5.12.10、OCCT 7.7.0 和 Visual Studio 2022 x64；
- 遵守 `AGENTS.md` 和 `.clang-format`；
- 保持 OCC 类型只出现在 Kernel、Render 和 IO 适配实现；
- 自研 `.h/.cpp` 不超过 800 行；
- 运行 x64 Debug/Release 两个构建脚本及全部测试；
- 新功能包含核心、边界和错误路径测试；
- 不提交 SDK、构建产物、客户模型、密钥或本机绝对路径。

提交前还需执行 `python scripts/security/audit_sensitive_data.py --require-noreply`；发布包需用
同一脚本的 `--release` 参数检查目录或 ZIP。

性能修改必须保留原始 JSON、测试集 SHA-256、运行环境和复现命令。界面修改的视觉验收
使用人工脱敏截图，不提交包含客户数据的图片。

提交代码不改变本仓库的保留全部权利许可证；合并前需由版权持有人明确接受。
