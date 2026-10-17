# DEM 横向、纵向与任意剖面提取

打开同目录的 **DEM高程剖面提取.ipynb**。Notebook 提供地形预览、矩形范围、横向/纵向方向、剖面位置、起止距离、采样间距、最近邻/双线性插值和 CSV 导出。也可在支持 ipympl 的 Jupyter 前端启用鼠标框选。

## 使用

1. 在 VS Code 中打开 Notebook，选择 Python 3.10+ 内核。首次使用可运行其中的安装提示，或在本目录执行 `python -m pip install -r requirements.txt`。JupyterLab 用户同样需使用已安装这些库的内核。
2. 修改参数区的 `DEM_PATH`。当前已填写用户指定的 `3_jingbiancenter.dem` 路径；输出自动定位到项目外层 `out/dem_profiles`，也可修改 `OUTPUT_DIR`。
3. 从上到下运行，查看实际尺寸、分辨率、单位和坐标。用范围滑块或数值输入确定矩形范围；横向沿 X 增大、纵向沿 Y 增大，位置参数为 0–1。
4. 点击“更新剖面”，检查地图、剖面位置和实际高程。起止距离相对选定整条剖面，终点填 0 表示到末尾；可直接截取其中 0–1000 m 等区间。
5. 点击“导出当前参数”。导出动作会重新读取当前控件，避免参数已经修改而仍保存上次预览的结果。每次输出到独立时间戳文件夹。

同目录的 `dem_profiles.py` 是 Notebook 使用的读写和采样模块。换位置使用时，请保留 Notebook、此模块及 requirements 文件；不需要修改或重新编译波场程序。

## 当前 DEM 的实际适配

该文件使用 1024 字节固定记录的 USGS 风格结构，声明规则剖面、投影代码 9、水平米和垂直米。当前 GDAL USGSDEM 驱动不能直接识别此变体；模块会先尝试标准 Rasterio 读取，失败后再验证并读取该固定记录结构。它检查列编号、每列样点数、坐标对齐、范围和高程缩放，并正确处理 B 记录的继续块及南向北的样点顺序。

实际读取结果为 **1181 列 × 840 行**，间距约 **38.218514 m**；像元中心覆盖 X≈19297025.153–19342122.999 m、Y≈4142718.101–4174783.434 m。原始样点的实际高程约 **1093.6–1730.3 m**；头部包围范围为 1093–1731 m，与实际样点极值不同。原文件的 Z 分辨率为 0.1 m，读取时将原始整数乘该分辨率并加每列局部高程基准，**只处理一次**。

按用户选择，默认采用文件声明的米制平面坐标计算水平格网距离，不假定 EPSG、椭球或地理基准，不输出猜测的经纬度。这里的长度是投影平面距离，不是已完成投影改正的地面测距；高程仍保留源文件的垂直参考，没有另作大地水准面或高程基准转换。以后确认坐标系后，可明确填写 `CRS_OVERRIDE`，再选择 `geodesic` 距离模式。

兼容分支只适配经验证的无旋转、米制、规则投影代码 9 固定记录；其他 Rasterio 无法识别的变体会报错，不会静默套用错误坐标。读取过程不修改原始 DEM。兼容缓存以 Float64 GeoTIFF 保存到输出目录的 cache 下，不附加猜测的 CRS。

## 导出内容

| 文件 | 用途 |
| --- | --- |
| `surface_profile.csv` | 仅两列 `x_m,elevation_m`，可直接导入当前二维波场程序 |
| `profile_with_coordinates.csv` | 距离、实际高程、原 DEM 的 X/Y、原剖面距离和有效标记；选择已确认 CRS 的测地线模式时另有经纬度 |
| `selection.json` | 原始文件、读取方式、坐标范围、单位、起止点、采样与插值、截取范围等 |
| `profile.png` | 实际高程剖面图 |
| `dem_with_profile.png` | DEM、选择矩形和剖面线位置 |

CSV 保留实际高程，不归一化、不减均值、不自动去趋势；距离从所选有效剖面起点设为 0。常规采样采用指定间距并保留末端点，所以最后一个间隔可能短于指定间距，实际值记录在 metadata 中。

NoData 或范围外位置保持无效，不填成零高程，且拒绝直接导出为模拟输入。可以缩小范围，或明确勾选“只保留最长连续有效段”；后者会报告丢弃点数、保留的原始距离范围，并重新从 0 计距，不会跨空洞拼接数据。

## 与二维模拟衔接

- 实际 DEM 的约 38.2 m 像元不能因输出为 1 m 或 5 m 采样而增加真实地形分辨率。双线性插值是在已有样点间估值，不是获取更精细的实测高程。
- 在波场程序中选择 `surface_profile.csv`。对于这里约 1000 多米的实际海拔，通常以导入器默认的**高程均值**作为局部模型基准；不要把人工零高程模型使用的“基准 0 m”机械套用到实际海拔数据。CSV 的原始高程本身保持不变。
- 文件水平覆盖需满足 `(Nx−1)h ≤ 剖面长度`。例如截取恰好 1000 m，可用 Nx=401、h=2.5 m；Nz、展平深度、主频和材料仍需结合地形幅度与数值分辨率检查。
- 横向/纵向按 DEM 坐标轴定义，不等于任意旋转栅格的某一行/列，也不自动等于真东/真北；当前文件为无旋转规则米制剖面。

## 技术依据

[Rasterio 栅格读取](https://rasterio.readthedocs.io/en/stable/topics/reading.html)、[窗口读取](https://rasterio.readthedocs.io/en/stable/topics/windowed-rw.html)、[GDAL USGSDEM 说明](https://gdal.org/en/stable/drivers/raster/usgsdem.html)、[GDAL USGSDEM 读取器源码](https://github.com/OSGeo/gdal/blob/v3.10.3/frmts/usgsdem/usgsdemdataset.cpp)、[PyProj CRS](https://pyproj4.github.io/pyproj/stable/api/crs/crs.html)、[Matplotlib 矩形选择器](https://matplotlib.org/stable/gallery/widgets/rectangle_selector.html)。本模块的兼容分支是独立 Python 实现，对源记录进行校验，不向用户 DEM 写入修补后的伪投影代码。
