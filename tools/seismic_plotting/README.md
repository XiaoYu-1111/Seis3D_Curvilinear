# 地震波场实验重绘 Notebook

入口是三个Notebook。绘图算法放在同目录`seisplot/`的Python模块中，Notebook逐步调用，不再复制15Hz/30Hz两套脚本。

| Notebook | 输入 | 输出 |
| --- | --- | --- |
| [01 不同主频四组模型对比](01_不同主频四组模型对比.ipynb) | 一个或多个主频的四组SEG-Y、模型VTS、快照索引 | 原始/RMS/AGC记录、模型及浅层、同时刻波场、RMS/频谱/镜像/晚时/偏移距、反射参考和主频对比 |
| [02 面波波场与频散对比](02_面波波场与频散对比.ipynb) | 面波实验算例目录、可选理论曲线CSV | 记录、模型、裁切波场、网格/边界残差、原始道叠加、频散谱、多偏移距窗口和无理论门限的谱峰CSV |
| [03 频散反演恢复对比](03_频散反演恢复对比.ipynb) | 已有反演结果CSV、拟合曲线CSV、正演真值和起始模型参数 | 模型恢复、逐层偏差、曲线拟合、多初值、层厚扫描和敏感性图 |

## 使用

1. 在VS Code或JupyterLab中打开Notebook，选择装有依赖的Python内核。
2. 修改开头的参数单元格。常规重绘只需改输入目录/文件、输出目录；更换模型、道集布局或分层数时也应检查相应参数。
3. 从上向下执行。耗时的全模型/波场读取可通过`RUN_MODELS`、`RUN_WAVEFIELDS`关闭。
4. 图件在输出目录的`figures/`，统计和实际配置在`data/`，可失效缓存位于`cache/`。不会写回原始模拟文件。

默认分别写到项目外层`out/replots/four_frequency`、`out/replots/surface_wave`、`out/replots/inversion`。输出必须是本工作区外层`out`的子目录；可任意选择其下的文件夹。代码会拒绝向仓库、输入数据根目录或其他位置写图，遵循仓库的输出布局规则。

Notebook会自动从当前目录向上寻找Git仓库。若从无关目录启动，在第一个代码单元格填写`REPOSITORY_ROOT`。相对输入路径以Git仓库的父目录（外层工作区）为基准，输入也支持绝对路径，包括位于其他磁盘的数据。没有硬编码`D:`或本机用户名。

依赖见[requirements.txt](requirements.txt)。在自己的环境安装后运行：

```powershell
python -m pip install -r tools/seismic_plotting/requirements.txt
python -m jupyter lab tools/seismic_plotting
```

已有环境无需重新安装。微软雅黑不可用时会明确提示并选择已安装的字体；为中文图件建议安装微软雅黑或Noto Sans CJK。

## 不用Notebook也能重绘

在Git仓库根目录执行，`-B`禁止生成仓库内的Python字节码：

```powershell
python -B tools/seismic_plotting/run.py --kind four --config tools/seismic_plotting/examples/four_frequency.json
python -B tools/seismic_plotting/run.py --kind masw --config tools/seismic_plotting/examples/surface_wave.json
python -B tools/seismic_plotting/run.py --kind inversion --config tools/seismic_plotting/examples/inversion.json
```

可以只执行部分步骤：

```powershell
python -B tools/seismic_plotting/run.py --kind four --config tools/seismic_plotting/examples/four_frequency.json --steps inspect,records,metrics,frequencies
python -B tools/seismic_plotting/run.py --kind masw --config tools/seismic_plotting/examples/surface_wave.json --steps inspect,dispersion,windows,picks
```

JSON示例是小型版本化配置，原始SEG-Y、VTS、缓存和旧PPT没有复制进Git。克隆仓库后需自行放置数据，或改为实际输入路径。原来的`out/tools`、`out/surface_wave_masw`一次性脚本无需被调用，也没有被删除。

## 输入目录或独立文件

默认每个算例采用程序原生导出结构：

```text
算例目录/
  run_metadata.json
  model/model.vts
  records/Vx.sgy
  records/Vz.sgy
  records/receiver_geometry.csv
  snapshots/times.csv
  snapshots/wavefield_1.vts ...
```

文件名不同时，在Notebook参数单元格覆盖算例配置即可。例如只读一个Vz分量：

```python
c = CONFIG["datasets"][0]["cases"][0]
c["folder"] = r"E:\my_experiment\model_A"
c["record_files"] = {"Vz": r"E:\my_data\vertical.sgy"}
c["geometry_csv"] = r"E:\my_data\receivers.csv"
c["model_vts"] = r"E:\my_data\velocity.vts"
c["snapshot_index_csv"] = r"E:\my_data\times.csv"
c["snapshot_dir"] = r"E:\my_data\snapshots"
# 没有metadata文件时，按真实参数提供，不猜测震源或网格间距。
c["metadata"] = {"source_x_m": 200, "source_depth_m": 1,
                 "spacing_m": 0.25, "frequency_hz": 20}
```

不画模型或快照时，只需提供要使用的SEG-Y与几何。不同算例可提供不同组件。分量必须匹配的对比步骤会明确报错。`snapshot_index_csv`列为`file,target_time_s,actual_time_s,step`，按目标时间匹配，不按文件排序冒充同时刻。索引中的`file`相对于`snapshot_dir`。

SEG-Y从卷头/道头读取dt、样本格式、道数、每道点数，支持IBM32（格式1）、IEEE32（5）、IEEE64（6）及已知数量的扩展文本头。卷/道头dt冲突、截断记录、变道长和非有限样值会报错。头中缺少dt时必须显式提供`sample_interval_s`；显式值覆盖有效头值会给出警告。角度坐标、缺失或重复X坐标应提供几何CSV。

几何CSV每道一行，必需`x_m`，可选`z_m`、`offset_m`。缺少offset时使用`x_m-source_x_m`。不假定250/241道、6001/5001点或固定0.5ms。不同道数和采样的算例可分别画图；残差分析要求dt、道数/点数、偏移距一致，不会静默重采样。

VTS目前支持本程序的**二维X-Z ASCII StructuredGrid**，读取`Vp`、`Vs`、`Vx`、`Vz`及`Coordinates`等命名数组。不支持二进制/压缩VTS和三维体网格，会明确报错。单行大数组按块解析，显示抽稀后缓存，并保留原生全分辨率最值。缓存键包含源路径、文件尺寸、修改时间、字段及抽稀步长，更改输入文件后不会复用旧缓存。若外部程序刻意保留了文件尺寸和修改时间，应手动清除**输出目录**的缓存。

## 常用显示参数

配置在`examples/*.json`，也可以直接在Notebook内覆盖：

```python
CONFIG["formats"] = ["png", "pdf"]
CONFIG["dpi"] = 200
CONFIG["snapshots"]["times_s"] = [0.5, 1.0]  # 必须是已有导出时刻
CONFIG["snapshots"]["crop_m"] = [4800, 5200, 0, 160]
CONFIG["snapshots"]["relative_x"] = False
CONFIG["snapshots"]["depth_coordinate"] = "eta"
CONFIG["snapshots"]["equal_aspect"] = True
```

- 全部空间量都使用m，空间1:1无需额外乘0.001。地震记录使用X/偏移距—时间坐标，不强制空间1:1。
- `depth_coordinate="physical"`读取真实保存的基准深度Z；`"eta"`为原生网格行号乘`spacing_m`的计算坐标；`"surface_relative"`为物理Z减当地地表Z。后二者在曲网格中不能混用。
- `relative_x=True`时裁切X以震源为零，否则为绝对X。`crop_m=[xmin,xmax,zmin,zmax]`只裁图，不修改模拟域。
- 速度模型默认viridis，波场使用seismic或RdBu_r，记录使用gray和双线性显示插值。规则间距记录用imshow，不规则接收X使用pcolormesh以保留位置。
- 波场只保留detail_symlog。`scale_scope="global"`在该主频数据集全部算例及所选时刻共用尺度；`"per_time"`为每个时刻的算例共用尺度。后者不能通过跨时刻颜色判断衰减。默认15/30Hz各自全局定标，不能直接比较两种主频PNG的色深；比较绝对响应应使用原始记录统计。
- `stride_x/z`只对显示数据抽稀，不改变物理坐标和原始文件。裁切较细节的图应减小步长。
- `snapshots.individual=True`时，除模型对比图外，另存每个算例每个时刻的单独快照，仍使用该对比图的同一物理色阶。
- AGC可设置窗口、RMS分母下限、边界处理与奇数窗口。四组示例沿用截短边缘归一化及1%整体RMS下限；面波示例沿用reflect边缘及最大局部RMS的3%下限。增益只用于显示。

## 反射与频散结果的解释

反射图的层界和Vp在`reflection_reference`显式提供，子波延迟默认1.5/主频，可用数据集`source_delay_s`覆盖。更换震源子波后必须检查延迟约定。起伏地表使用的是几何近似参考，并非严格曲面射线追踪。红/橙线不是自动拾取。相关性计算始终使用原始记录。

频散计算使用指定分量的原始记录、Tukey端点渐消、逐道相位归一化和相移叠加。理论CSV只叠加显示，不用于限制峰值搜索。全局相干峰仍可能包含高阶模态、近场或混叠，**不能直接当作已确认基阶**。时窗和孔径决定实际分辨率，细化搜索网格不增加观测信息。理论列名可通过`theory_frequency_column`和`theory_mode_columns`配置。

## 反演Notebook的CSV格式

默认结果表一行一个模型：

```text
id,success,rmse,h1,h2,h3,vs1,vs2,vs3,vs4
model_A,1,0.3,10,20,40,250,520,800,1250
```

最后一层为半空间，没有其厚度。`results_csv`可以是一个或多个文件；结果ID不得重复。新表头可通过`id_column`、`rmse_column`、`vs_columns`、`thickness_columns`映射。缺少厚度列时使用配置中的固定层厚。`fit_csv`默认列：

```text
frequency_hz,observed_m_s,initial_m_s,fitted_m_s
```

可用`fit_columns={"frequency":...,"observed":...,"initial":...,"fitted":...}`映射；`initial`列可省略。拟合曲线RMSE与所选结果行不符时会报错，避免选错文件。理论曲线可选。这里不自动解析Qt所有历史私有文本格式，必要时先整理为上述CSV。

`truth`为正演真值，`initial_vs_m_s`为反演起始模型，二者分开配置；`free_vs`明确哪些层真正参与求解。灰底和误差表标出固定先验，不把其零偏差计作独立恢复。`multistart_groups`、`thickness_scan`和`sensitivity_cases`可省略，或填写新的模型ID。本Notebook只画已有反演结果，不编译Qt、不运行优化、不改原始模型。

## 来源与验证

重构来源：`Analyze-Jingbian-Records[-30Hz].py`、`Export-Jingbian-Gain[-30Hz].py`、`Export-Jingbian-Vz[-30Hz].py`、`Revise-Jingbian-Figures[-30Hz].py`、面波`analyze_results.py`和`plot_inversion.py`。保留物理单位、原始幅度定量、AGC显示分离、真实网格、裁切和模态限制；移除固定工作盘、固定样本数、缓存目录依赖及字符串补丁。

本工具包不自动构建PPT。重绘PNG/PDF可替换到后续报告。统计方法和显示配置会随每次输出保存，适合复核。原始旧报告的全部硬编码数值或解释没有复制为新算例的结论。

运行读入及数值检查（夹具与日志生成到外层out）：

```powershell
python -B -m unittest discover -s tools/seismic_plotting/tests -v
python -B tools/seismic_plotting/check_notebooks.py
# 在默认实验数据齐全时，执行三个Notebook的实际数据检查：
python -B tools/seismic_plotting/check_notebooks.py --execute
```

版本库中的Notebook清除所有执行输出。保留带输出副本时另存到外层out；不要把缓存、图件或原始数据提交进Git。
