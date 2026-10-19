# CPU/CUDA 生产求解器基准

`ComputeBenchmark.cpp` 直接调用当前二维、三维生产 API，不修改内核或同步策略。参照 Cartesian 基准采用二维 256²/1024²/2048²、三维 48³/96³/160³，32 个 OpenMP 线程、20 步预热、100 步计时、每后端 5 次重复。输出 60 条原始试验数据和 CPU/CUDA 全场、接收波形误差。

在外层工作区的 MSVC x64 开发者终端运行：

```powershell
cmake --build out/build/x64-Release --target seis_compute_benchmark --parallel 4
$env:OMP_WAIT_POLICY="PASSIVE"
out/bin/Release/seis_compute_benchmark.exe --output <外层out下的绝对结果路径> --threads 32 --repeats 5 --warmup 20 --steps 100
```

可加 `--dimension 2/3 --size N` 缩小范围。报告从内层仓库运行 `python -B benchmarks/report.py <结果目录>` 生成；需 matplotlib、numpy。

计时包含曲网格 RK4、Float64、每步诊断/回滚保护和一个接收点，不包括初始化、重置、输出、绘图或快照下载。CUDA 每步由生产诊断/采样同步完成，不等同于 Cartesian 的 Float32、无接收记录内核基准，不能直接用两项目绝对耗时判定算法优劣。粗网格高频模型用于性能压力测试，并非精度验证。

运行结果默认保存在外层 `out/benchmarks`。按用户要求发布的最新结果见 [性能对比报告](../docs/PERFORMANCE_BENCHMARK.md)，包括单步耗时图、倍速曲线和 60 轮原始 CSV；文档中的图片、数据与源码版本信息均在仓库内，可随 Git 同步。

明确发布时，从内层仓库执行 `python -B benchmarks/publish.py <已完成的结果目录>`。它校验完成标志、复制已复核图表与数据、改写仓库内相对链接，并记录 SHA-256；运行日志、试跑、EXE 和构建缓存留在外层 out。
