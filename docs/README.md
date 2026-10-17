# 文档导航

| 内容 | 入口 |
| --- | --- |
| 实际 DEM 预览、横/纵剖面、范围选择与 CSV 导出 | [DEM 剖面提取 Notebook](../tools/dem_profiles/README.md) |
| 控制方程、交错/同位网格比较、曲网格映射、DRP/RK 与稳定性 | [曲网格波场理论整理](CURVILINEAR_WAVE_THEORY.md) |
| 定向体力、各向同性爆炸源、单位与验证 | [震源机制](SOURCE_MECHANISMS.md) |
| 二维外部高程格式、分层弹窗预设与大模型容量 | [外部高程与模型容量](SURFACE_PROFILES.md) |
| 二维参数应用、共用记录显示与 SEG-Y 导入/分量导出 | [地震记录与 SEG-Y](SEGY_AND_RECORDS.md) |
| 主页维度入口、二维 P–SV 原理及独立源码分组 | [二维与三维工作流](TWO_D_WORKFLOW.md) |
| 背景、原理、实现过程、界面介绍与附录 | [完整学术说明](PROJECT_MANUAL.md) |
| 方程、边界闭合、差分、时间组合与诊断 | [数值原理](NUMERICAL_METHOD.md) |
| 模型、震源、接收器、显示与快捷键 | [用户指南](USER_GUIDE.md) |
| 接收几何及 CPU/CUDA 计时 | [采集与性能](ACQUISITION_AND_PERFORMANCE.md) |
| 分层介质与图标资源 | [层状模型](LAYERED_MODELS.md) |
| VS、依赖、外部输出、设备选择与分发 | [构建说明](BUILDING.md) |
| Git 上传内容及仓库边界 | [仓库组织](REPOSITORY_LAYOUT.md) |
| 已验证事项与证据边界 | [数值验证状态](NUMERICS_AND_VALIDATION.md) |

正文图片全部保存在本仓库 `docs/images`，GitHub 阅读不依赖外层 out。`images/capture.tsv` 保存已发表截图的算例与时间步元数据，供图集及复核使用。原始 PPM、模拟场和分析中间数据留在外层 `../out`。`VERIFICATION_*.md` 是历史检查记录，旧参数不代表当前默认设置，其中本地输出也不随仓库发布。

## 生成离线说明和博客

从仓库根目录执行 `python -m pip install -r tools/requirements.txt`，然后执行：

```powershell
python -B tools/package_documentation.py
```

工具直接读取仓库内正文、发表 PNG 和截图元数据，无需重新模拟。离线图文包生成到 `../out/documentation`，GitHub Pages / Hexo 两套资源生成到 `../out/blog`，ZIP 也在对应外层目录。

两版博客同源，只适配 front matter 和资源路径。Hexo 保持 `![说明](01-model.png)`，PNG 放在文章同名资源目录。站点需自行配置数学公式支持；工具不修改网站或推送。

重新采集界面可用 `--capture-workflow`，输出位于 `../out/screenshots/curvilinear-workflow`；`python -B tools/analyze_dispersion.py` 输出位于 `../out/validation/dispersion`。更新发表图片时先复核图像、参数及解释，再将选定 PNG 和对应 capture.tsv 更新到 `docs/images` 并重新打包。不要复制原始帧、构建日志或模拟中间文件进入 docs。
