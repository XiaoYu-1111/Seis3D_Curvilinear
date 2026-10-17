# Seis3D Curvilinear

独立的三维曲网格弹性波工程，当前版本 **0.16.0**。提供 CMake + C++20 数值核心、可切换的 CPU/OpenMP 与 CUDA Float64 后端，以及 Dear ImGui Docking + OpenGL 4.3 工作台。

**完整图文说明与博客**：[背景、原理、实现过程、界面介绍与附录](docs/PROJECT_MANUAL.md)。正文包含方程与边界推导、数值流程、高频采样分析、18 张界面截图和参考文献，明确区分实现能力与验证范围。GitHub Pages / Hexo 两版从同一正文生成，Hexo 图片采用 `![说明](01-model.png)`；发布包不附开发日志和计算中间文件。文档导航及生成方式见 [文档说明](docs/README.md)。

布局参考 SeisSim V2：左侧参数与数值诊断，中间四视图/三维优先/紧凑页签/记录优先，底部接收记录。顶栏可即时切换 English / 简体中文及 10 种浅色、深色主题；主题同步作用于三维、切片和记录区。语言切换保留停靠 ID，外观偏好保存在 `out/ui/preferences.cfg`。

运行设置集中在 **运行 / Run** 页签：初始模拟时长 **1 s**、最大计算步初始值 **5000**（0 表示按时长）、CFL 安全因子、进度、实际终止时刻、计算速率与预计剩余时间。时长与最大步数先到者停止；修改后按 **应用修改并重建 / Apply changes / rebuild** 生效。每帧计算预算默认 16 ms，在完整 RK4 步之间让出界面；单步耗时可能超过预算。接收记录额外包含 t=0 样本，因此推进 5000 步会得到 5001 个样本。

**切换核心**：运行与性能 → 性能对比 → 计算后端，选择 CPU / OpenMP 或 CUDA GPU，再点击 Apply changes / rebuild。切换从 t=0 开始，保留相机、切片和显示增益；初始化失败保留原会话。CUDA 不可用时禁用选项并显示原因，不会静默退回 CPU。

**默认核心为 CUDA**：图形程序和命令行启动时优先选择可用的 CUDA GPU；未编译 CUDA 或设备不可用时，启动选择 CPU 并可查看 CUDA 状态。显式选择的 CUDA 初始化失败仍报告错误，不把一次失败计算伪装成 CPU 运行。

**0.15 网格与操作更新**：新增 100³、200³、300³ 预设和快捷按钮，各物理轴允许 8..300 个节点。模型页按当前可用内存/显存预检，提供显式“按可用内存适配网格”，保持间距并适配源/接收位置。当前 Float64 CUDA 均匀 300³ 模型约需 16.67 GiB 显存；容量不足时保留原会话。C 原位清空场、记录与本次推进计时，F1 显示快捷键，顶部常驻清空/帮助及待修改时的应用按钮。尺寸显示为普通小数，生效 dt 与待应用参数的 dt 预览分开；曲面几何变化后重算度量。完整 12 项 CTest 和实际 200³ CUDA 两步检查通过，见 [0.15 检查记录](docs/VERIFICATION_0_15.md)。

**0.13 重新标定 CFL**：交替 DRP 核心默认系数改为 **0.8**，允许范围 0.02..0.8；0.22 可用于复现旧结果。时间步同时满足曲网格几何、吸收阻尼和震源时间采样上限。运行页和诊断页显示生效限制及实际 `Vp_max*dt/h`。规则网格 V2 的 CFL 还除以交错差分系数和，不能只比较控件数字。标定方法、精度与耗时实测见 [0.13 CFL 检查](docs/VERIFICATION_0_13.md)。

**0.14 尺寸与独立配色**：建模页显示待应用网格跨度及当前真实坐标包围盒，二维切片显示米制尺寸/坐标。显示页将五种波场分量与 Vp/Vs/密度分开，支持仅波场、仅模型和叠加三种模式；波场与模型各有独立色带、范围和色标开关，波场增益与归一化不改变模型物性颜色。模型切片按重建与切片位置缓存，二维/三维共享映射函数。参数复核和检查见 [0.14 更新](docs/VERIFICATION_0_14.md)。

模型页提供 100×20×100、50×20×50、200×50×100 三组窄 Y 预设，选择后应用重建。新增 **震源 / Source** 页，可设置从零开始的 X/Y/Z 网格索引（X/Y 为 -1 时居中），查看实际坐标并将切片移到震源。震源中心需距边界至少两格。

模型预设首项现为 **波场验证：200×20×200**，替换原 28×24×28 项。它载入图中完整工况：10 m、12 Hz、震源 (100,10,50)、0.5 s、均匀 Vp/Vs=3000/1700、ρ=2200、起伏 20 m/300 m、吸收厚度 6/6/6；点“平地”切换同参数平地版本。其余预设保留有效源和停止设置，缩小时会适配越界震源。

波场色标默认使用归一化刻度：有符号分量 −1..1、速度模 0..1，注明 `1` 对应的实际 SI 振幅；显示页可切回物理刻度，介质色标始终使用物理单位。该开关独立于自动幅值范围和显示增益，不修改原始场。详见 [0.12 更新与检查](docs/VERIFICATION_0_12.md)。

接收记录参考 V2 升级为 ImPlot 道集和单道三分量，支持六种绘图样式、14 种色带、AGC、显示归一化、道号范围及缩放平移；导出可选原始 Vx/Vy/Vz 三分量。显示处理不改写原始记录。

新增 **介质 / Materials** 页：按用户表格提供黄土五层，并提供低速覆盖层、风化岩三层、沉积四层示例。默认按网格 k 层对齐，同一水平节点层共用物性，避免曲面下界面跳格；逐节点参数实际参与 CPU/CUDA、CFL、能量及 VTS 导出。支持 Vp/Vs/密度切片和完整地层表编辑。5 m 薄层可选择暂存 1 m 网格及完整时长算例。详见 [层状模型与图标](docs/LAYERED_MODELS.md)。

Windows EXE 与窗口已接入曲面地形、分层地质块和地震波环图标；生成资产在 `out/assets/icons`。

控制面板顶部固定横向显示“建模、采集、运行、显示、导出”五个入口，直接点击切换，不再折叠进分类下拉菜单。接收设置新增 X/Y 测线与 X-Y 面阵；性能页显示完整有效步的累计/平均耗时，保存 CPU/CUDA 记录，严格匹配相同配置与步数后显示耗时比，可导出 CSV。操作和计时口径见 [接收排列与性能对比](docs/ACQUISITION_AND_PERFORMANCE.md)。

0.11 按本地 CGFD3D 逐项复核核心，修正同位中心差分的高波数寄生分支，接入交替 DRP、三层虚点、近表面降阶及牵引通量镜像。新增原 C 函数直接对照和解析 P/S 波检查。吸收边界可启停、独立设置 X/Y/底部厚度和强度，并切换区域显示。结果及限制见 [0.11 数值复核](docs/VERIFICATION_0_11.md)。

## Build and run

采用两层 CMake：外层 `CMakeLists.txt` 管理工作区，内层 `Seis3D_Curvilinear/CMakeLists.txt` 定义目标。程序源码、头文件、资源、着色器和第三方图形源码已集中到内层文件夹，文档与维护工具保留在外层。请在 VS 中打开外层文件夹。需要 CMake 3.25+、支持 C++20 的编译器；图形工作台需要 OpenGL 4.3。VS 预设使用 Ninja、自动准备 x64 编译环境；命令行请先进入 MSVC x64 开发者终端。完整说明见 [VS 与构建布局](docs/BUILDING.md)。

```powershell
cmake --preset x64-Release
cmake --build --preset x64-Release
.\out\bin\Release\Seis3DCurvilinear.exe
```

Release / Debug 预设默认启用 CPU + CUDA 双后端，CUDA 架构默认检测本机 GPU；也可显式指定：

```powershell
cmake --preset x64-Release -DSEIS3D_ENABLE_CUDA=ON -DCMAKE_CUDA_ARCHITECTURES=89
cmake --build --preset x64-Release
ctest --preset x64-Release
```

其他显卡请设置对应的 CMake CUDA 架构；原始 CMake 选项仍允许关闭 CUDA，可用 `cmake --preset x64-Release -DSEIS3D_ENABLE_CUDA=OFF` 构建独立 CPU 版本。Windows 构建会将 CUDA 运行库 DLL 复制到 EXE 同级目录。缓存分别固定在外层 `out/build/x64-Release`、`out/build/x64-Debug`，程序分别输出到 `out/bin/Release`、`out/bin/Debug`。

命令行示例（默认优先 CUDA）：`out/bin/Release/seis3d_curvilinear_demo.exe`，输出 `out/curvilinear_demo.vts`。所有生成文件放在本项目外层 `out`。VS 可打开本目录并选择预设；解决方案默认启动 GUI 目标。

工作区各面板可通过页签或标题栏的 × 关闭，从“工作区”菜单勾选重新打开；F 恢复全部面板与默认布局，关闭状态随偏好保存。切片网格按实际模型节点逐格连接，可独立于十字线开关并调整透明度；波场与记录共用参考项目的 14 色带定义，二维/三维切片均显示带 SI 刻度的物理量色标。运行、模型、显示、导出、警告、重置操作分别采用绿色、蓝色、青色、紫色、琥珀色、红色功能层次。

工作台支持运行/暂停、单步、重建模型、10 种主题、14 种波场色带、切片缩放/平移/联动选点、轨道相机、从零时刻开始的接收记录，以及带路径选择的 VTS/CSV 导出。显示增益和固定色标不会因切换分量、色带或重建模型而重置。

![实际 OpenGL 工作台：1600 × 1000 原始帧](docs/images/workbench.png)

Windows GUI 默认使用本机 GLFW/GLEW SDK，可通过 `SEIS3D_GLFW_ROOT`、`SEIS3D_GLEW_ROOT` 指定。Dear ImGui 随源码保存在 `Seis3D_Curvilinear/third_party/imgui`。无图形依赖时使用 `-DSEIS3D_BUILD_GUI=OFF` 构建 CPU 部分。

本版验证见 [0.15 网格、快捷键与时间步检查](docs/VERIFICATION_0_15.md)，历史分层及图标验证见 [0.8 记录](docs/VERIFICATION_0_8.md)。

详细推导见 [数值原理与实现](docs/NUMERICAL_METHOD.md)，历史界面检查见 [0.7 验证记录](docs/VERIFICATION_0_7.md)。

操作与参数说明见 [用户指南](docs/USER_GUIDE.md)，数值限制与验证状态见 [验证说明](docs/NUMERICS_AND_VALIDATION.md)。

前版新增功能见 [0.6 预设、震源与接收记录验证](docs/VERIFICATION_0_6.md)。此前实测见 [0.5 布局与尺寸/主频矩阵验证](docs/VERIFICATION_0_5.md)，前一版本见 [0.4 稳定性和双后端验证](docs/VERIFICATION_0_4.md)。

## Scope of this first baseline

- 数值后端支持 CPU 串行/OpenMP 和 CUDA；空间差分、RK4、自由表面更新均在选定核心执行。
- 时间步同时受逆度量、实际相邻平面距离和海绵阻尼约束；逐步监测机械能并拒绝非有限步骤。
- 高主频额外约束 `dt <= 1/(50*f0)`；波长采样按实际最大物理网格边长估计。稳定性通过但波长采样不足时，界面仍明确报警，不代表精度通过。
- 同位网格交替 MacCormack-DRP、四阶段 RK 权重；均匀或分层各向同性弹性介质、竖直点力震源、正弦起伏地表。
- 使用度量约束 CFL、零牵引速度导数闭合、Jacobian/度量加权的牵引镜像和侧面/底部海绵吸收。
- 自由表面处理尚未与 CGFD3D 的完整 Traction Image 实现完成定量对照，海绵层也不等价于 CFS-PML。
- VTS 当前支持导出，尚未加入任意 VTS 模型导入。

The solver structure follows the licensed CGFD3D curvilinear collocated-grid formulation. See `docs/NUMERICS_AND_VALIDATION.md` and `THIRD_PARTY_NOTICES.md` for attribution and validation status.

快捷键与规则网格三维项目对齐：Q 切换波场色带，W 切换波场分量，E 切换模型配色，Shift 反向；Space 运行/暂停，N 单步，C 清空波场，R 适配全部视图，G 网格，B 吸收区域，V 三维切片，F1 帮助。F 恢复布局，Ctrl+S 导出 VTS。波场和模型配色独立保存，切换不重置显示增益。完整表见 [用户指南](docs/USER_GUIDE.md#6-快捷键)。
