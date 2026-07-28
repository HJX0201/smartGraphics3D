# smartGraphics3D

[English](README.en.md) | 简体中文

smartGraphics3D 是一个基于 C++17、Qt 5.12.10 和 OpenCascade 7.7.0 的 Windows x64
桌面 3D CAD 应用。当前版本为 `v0.1.0-beta.1`，重点覆盖可靠的 CAD 文件交换、实体建模、
测量、多视口和大规模重复零件显示。

## 主要功能

- 导入和导出 STEP、IGES、BREP、STL、OBJ。
- 创建长方体、圆柱、圆锥、球体和圆环体。
- 布尔、圆角、倒角、孔、镜像、移动、旋转、缩放和阵列。
- 点、边、面、实体选择，以及距离、角度、半径、面积、体积和包围盒测量。
- 正交/透视、标准视角、单/双/四视口、同步相机、剖切和显示质量调整。
- 文档事务、100 步撤销/重做、快照、自动恢复和 `.sg3d` 项目文件。
- 两种复制方式：
  - `Ctrl+D` 普通复制：每个对象拥有独立 AIS Presentation。
  - `Ctrl+Shift+D` 共享显示实例：通过 `AIS_ConnectedInteractive` 复用显示几何。

## 共享实例实测

确定性生成的单一重载模型包含 5000 个实体、每份 2,342,442 个三角形。只复制 10 份时，
同一台测试机 Release 中位数如下：

| 指标 | 普通复制 | 共享显示实例 | 变化 |
| --- | ---: | ---: | ---: |
| Private Bytes | 1950.91 MiB | 499.99 MiB | -74.37% |
| Working Set | 998.65 MiB | 416.64 MiB | -58.28% |
| 估算 GPU 几何 | 1876.50 MiB | 187.65 MiB | -90.00% |
| 首次显示 | 18099.88 ms | 8563.87 ms | -52.69% |
| 30 帧重绘 | 950.41 ms | 950.69 ms | +0.03% |

它不会让进程内存“固定减半”。节省比例取决于显示几何占总内存的比例；Qt、OCCT、
拓扑、选择 Owner、实例变换和 Graphic3d 连接结构仍然存在。完整方法、原始 JSON 和
HTML 报告见 [实例复制基准](sgraphBenchmarks/instanceCopy/README.md)。

## 快速构建

前置条件：

- Windows 10/11 x64；
- Visual Studio 2022，安装“使用 C++ 的桌面开发”和 Windows SDK；
- Qt 5.12.10 MSVC x64 Kit；
- OCCT 7.7.0 x64 SDK，或项目工具链包。

```powershell
python scripts/build/build_64_debug.py --occt-root C:\SDK\occt-7.7.0
python scripts/build/build_64_release.py --occt-root C:\SDK\occt-7.7.0
```

脚本会自动查找 VS、Qt、CMake、Ninja 和 OCCT，执行编译与 7 项测试，并把 Qt 平台插件
及实际使用的 OCCT DLL 部署到：

- `build/64/debug/bin/smartGraphics3D.exe`
- `build/64/release/bin/smartGraphics3D.exe`

Qt 未找到或版本/位数不匹配时，脚本会列出被检查和拒绝的候选。可使用 `--qt-root`、
`SMARTGRAPHICS3D_QT_ROOT` 或 `QTDIR` 明确指定。

## 文档

- [使用说明](sgraphDocs/USAGE.md)
- [代码架构](sgraphDocs/ARCHITECTURE.md)
- [编译与工具链](sgraphDocs/BUILDING.md)
- [测试与性能复现](sgraphDocs/TESTING.md)
- [发布验证记录](sgraphDocs/RELEASE_VALIDATION.md)
- [第三方声明](THIRD_PARTY_NOTICES.md)
- [变更记录](CHANGELOG.md)
- [安全策略](SECURITY.md)

当前为 Beta 版本，不承诺项目格式向后兼容。仓库不包含 Qt、OCCT SDK、构建产物或客户
模型。界面截图将在完成脱敏人工验收后补充。

Copyright © 2026 huangjiaxin. All rights reserved.
