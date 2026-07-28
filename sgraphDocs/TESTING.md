# 测试与性能复现

## 自动测试

两个构建脚本默认在编译后执行全部测试：

```powershell
ctest --test-dir build/64/debug --output-on-failure
ctest --test-dir build/64/release --output-on-failure
```

测试分为 Kernel、Document、ProjectCodec、CadCodec、TaskManager、Gui、Viewport 七组，
覆盖核心逻辑、边界、错误路径、撤销/重做、保存重开、选择映射和 AIS 资源结构。

构建脚本另有 4 组 Python 单元测试，覆盖显式/环境 Qt 候选优先级、正确版本与 x64
识别、Qt 6/错误位数/完全缺失拒绝，以及 `windeployqt` 缺失或失败时的最小依赖回退：

```powershell
python -m unittest scripts.build.test_build_scripts -v
```

当前验证结果：

| 配置 | 编译 | 自动测试 | Qt/OCCT 部署 |
| --- | --- | ---: | --- |
| x64 Debug | 通过 | 7/7 通过 | 通过 |
| x64 Release | 通过 | 7/7 通过 | 通过 |

编译选项包含 C++17、`/W4 /permissive- /Zc:__cplusplus /utf-8`。

## 实例复制基准

Release 会生成 `build/64/release/benchmarks/sgraphInstanceCopyBenchmark.exe`。标准套件
保留 1/10/50/100 份小模型趋势回归；正式重载套件使用一个包含 5000 个实体的大模型，
只复制 1/2/5/10 份。普通和共享模式分别在全新子进程中运行三次并取中位数，以减少一次
进程内缓存和顺序偏差。

指标包含创建时间、首次显示、30 帧重绘、Private Bytes、Working Set、估算 GPU 几何、
OCCT 原始统计、Graphic3d 结构和场景三角形数。性能数字不作为易波动的 CTest 门槛；
结构测试必须证明共享模式真实产生一个原型和 N 个 Connected 实例。

正式数据、模型哈希、HTML 报告和复现说明位于
`sgraphBenchmarks/instanceCopy/`。模型由基准程序确定性生成，不包含客户数据或第三方
下载模型。

## 发布前检查

发布前还需检查 clang-format、clang-tidy、自研 C++ 文件不超过 800 行、源码中无本机
绝对路径，以及发布 `bin` 中存在 `platforms/qwindows.dll` 且不含测试程序。
