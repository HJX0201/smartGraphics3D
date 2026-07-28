# Windows x64 工具链包

源码仓库只保存打包脚本、清单模板和许可证，不保存 SDK 或压缩附件。

运行 `scripts/toolchain/package_toolchain.py` 会生成：

- `artifacts/smartGraphics3D-toolchain-v0.1.0-windows/`
- 优先生成同名 `.7z`；找不到 7-Zip 时生成 `.zip`。

包内包含 OCCT 7.7.0 x64 Debug/Release 头文件、CMake 配置、导入库和运行 DLL，以及固定
的 CMake、Ninja、便携 Python。PDB、示例、文档、测试、备份库和构建缓存会被排除。
`toolchain-manifest.json` 供两个构建脚本发现路径，`SHA256SUMS` 记录每个文件的哈希。

Qt 不进入包内。使用者必须安装 Qt 5.12.10 MSVC x64，构建脚本会自动发现并部署实际
需要的 Qt DLL 和插件。MSVC 与 Windows SDK 也作为系统前置条件。

当前机器没有 7-Zip 时，ZIP 是功能等价的回退附件；两者都必须小于 GitHub Release
单附件 2 GiB 限制。
