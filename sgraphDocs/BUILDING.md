# 编译与工具链

## 支持范围

当前发布只支持 Windows x64。OCCT 7.7.0 发布 SDK 和本项目自动测试均以 x64 为基线，
因此不提供未经同等验证的 32 位脚本或二进制。

需要 Visual Studio 2022 C++ 桌面工具、Windows SDK 和 Qt 5.12.10 MSVC x64 Kit。Qt
必须由使用者安装，不包含在工具链包中。

## 两个独立入口

```powershell
python scripts/build/build_64_debug.py [--jobs N] [--qt-root PATH] [--occt-root PATH]
python scripts/build/build_64_release.py [--jobs N] [--qt-root PATH] [--occt-root PATH]
```

每个文件都是可单独复制和运行的完整入口，不依赖公共 Python 模块。默认并行数为逻辑
CPU 数，可用 `--clean` 清理对应配置后重建。

查找顺序：

1. 命令行参数；
2. `SMARTGRAPHICS3D_QT_ROOT` / `SMARTGRAPHICS3D_OCCT_ROOT`；
3. `QTDIR`、`CMAKE_PREFIX_PATH`、`CASROOT`；
4. 工具链清单、PATH 和常用安装目录。

Qt 必须为 5.12.10 MSVC x64；Qt 6、MinGW Kit 和 x86 Kit 会被拒绝并显示原因。部署优先
使用同一 Kit 的 `windeployqt`。若它缺失或失败，脚本按内置模块清单复制 Qt DLL 与
插件，再验证 `platforms/qwindows.dll`。OCCT 只复制主程序 PE 依赖闭包内的 DLL。

输出固定为：

```text
build/64/debug/bin/smartGraphics3D.exe
build/64/debug/tests/*.exe
build/64/release/bin/smartGraphics3D.exe
build/64/release/tests/*.exe
```

## 工具链包

`scripts/toolchain/package_toolchain.py` 可把 OCCT 7.7.0 x64 Debug/Release SDK、CMake、
Ninja 和便携 Python 整理成单一压缩包。它会排除 PDB、示例、文档、备份库和构建缓存，
生成 `toolchain-manifest.json`、文件 SHA-256 清单和第三方许可证目录。

```powershell
python scripts/toolchain/package_toolchain.py `
  --occt-root C:\SDK\occt-7.7.0 `
  --cmake-root C:\Tools\CMake `
  --ninja C:\Tools\ninja.exe `
  --python-root C:\Tools\PythonPortable
```

默认输出到被 `.gitignore` 排除的 `artifacts/`。Qt 不会被复制。构建脚本会查找项目同级
工具链目录，也可使用 `--toolchain-root` 或 `SMARTGRAPHICS3D_TOOLCHAIN_ROOT`。

系统模式下若缺少任何工具，脚本会报告具体缺项和可使用的参数/环境变量，不会切换到
ABI 不兼容的编译器。
