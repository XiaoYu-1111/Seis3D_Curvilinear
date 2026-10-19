"""Render the completed production CPU/CUDA benchmark as Markdown and PNG."""
import csv
import json
from pathlib import Path
import argparse


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('results', type=Path)
    args = parser.parse_args()
    root = args.results.resolve()
    assert (root / 'COMPLETE.txt').is_file(), 'Benchmark has not completed'
    hardware = json.loads((root / 'hardware.json').read_text(encoding='utf-8'))
    rows = list(csv.DictReader((root / 'summary.csv').open(encoding='utf-8')))
    trials = list(csv.DictReader((root / 'trials.csv').open(encoding='utf-8')))
    assert len(rows) == 12 and len(trials) == 60, 'Expected six cases, two backends, five repeats'
    groups = {(int(r['dimension']), int(r['size']), r['backend']): r for r in rows}
    sizes = {2: [256, 1024, 2048], 3: [48, 96, 160]}
    lines = [
        '# 曲网格二维 / 三维 CPU 与 CUDA 性能测试', '',
        '测试日期：2026-10-19。生产求解器、Release、Float64。参照 Cartesian 项目的尺寸、32 线程、20 步预热、100 步计时和 5 次重复中位数方法。', '',
        '## 测试环境', '',
        f'- CPU：{hardware["cpu"]}；{hardware["physical_cores"]} 核 / {hardware["logical_processors"]} 逻辑处理器；OpenMP {hardware["threads"]} 线程。',
        f'- GPU：{hardware["gpu"]}。',
        f'- 物理内存：{hardware["ram_bytes"] / 1024**3:.2f} GiB；编译器 MSVC {hardware["msvc"]}。',
        f'- 接电状态：{hardware["ac_power"]}（1=接电，0=电池，255=未知）；进程高于正常优先级：{hardware["above_normal_priority"]}。',
        '- 保留非规格化数（FTZ/DAZ 关闭），OMP_WAIT_POLICY=PASSIVE。GPU 每轮先等待 CPU 线程休眠 600 ms，再进行至少 300 ms 的求解预热。',
        '- 本机当次测试结果，未锁定 CPU/GPU 频率。每尺寸先执行 CPU 五轮，再执行 CUDA 五轮；没有随机交错顺序，持续负载与功耗/温度状态可能影响结果。5 次重复的四分位范围用于展示波动，不是置信区间。', '',
        '## 计时口径与模型', '',
        '- 只计 `advance(1)` 的墙钟时间；先预热 20 步，再逐步计时 100 步，每后端重复 5 次。重置、初始化、快照下载、比对、导出和渲染不计入推进耗时；初始化时间另列。',
        '- 包含真实生产路径中的 RK4、曲面度量、自由表面、吸收海绵、回滚缓冲、每步数值诊断与一个接收点的采样。CUDA 完成同步包含在生产诊断/采样中，未使用仅记录 kernel 提交时间的计法。',
        '- 曲面起伏 20 m，波长 300 m；网格间距 10 m；震源主频 30 Hz、深度为网格的 1/4；两层介质：Vp/Vs/ρ = 3000/1700/2200 与 4200/2400/2500（SI），层界位于计算深度的中部。二维为五场 P-SV，三维为九场弹性波。',
        '- 时间步由现有几何/CFL/源采样约束自动计算，同一模型 CPU/CUDA 的 dt 必须相等。每轮均从零场重置；快照与接收波形在第 120 步进行比对。',
        '- 30 Hz 与 10 m 是参照模型参数，最慢层在 2.5f₀ 下只有约 2.27 PPW，属于性能压力工况，不作为传播精度验证。',
        '- 三维海绵层请求 16 格/轴，二维使用生产默认 16 格；求解器按模型尺寸限制实际厚度。物理节点数不包含幽灵层；内存存储节点数另列于原始 CSV。', '',
        '## 性能结果', '',
        '每步耗时均为 5 次重复的中位数；括号为 P25–P75。加速比 = CPU 中位耗时 / CUDA 中位耗时。吞吐量按物理节点计算，不乘 RK4 阶段数或物理场数。', '',
        '| 维度 / 档位 | 模型 | CPU ms/步（P25–P75） | CUDA ms/步（P25–P75） | CUDA 加速比 | CPU / CUDA 百万节点·步/秒 |',
        '|---|---|---:|---:|---:|---:|',
    ]
    for dim in sizes:
        for level, size in zip(['小', '中', '稍大'], sizes[dim]):
            c, g = (groups[(dim, size, backend)] for backend in ['CPU', 'CUDA'])
            interval = lambda r: f'{float(r["median_ms_step"]):.3f}（{float(r["p25_ms_step"]):.3f}–{float(r["p75_ms_step"]):.3f}）'
            model = '×'.join([str(size)] * dim)
            lines.append(f'| {dim}D {level} | {model} | {interval(c)} | {interval(g)} | {float(g["cpu_over_cuda"]):.2f}× | {float(c["physical_mnodes_per_second"]):.2f} / {float(g["physical_mnodes_per_second"]):.2f} |')
    lines += ['', '### CUDA 加速比曲线', '',
              '![二维与三维 CUDA 加速比曲线](speedup.png)', '',
              '横轴为物理节点总数（对数刻度），纵轴为 CPU 中位耗时 / CUDA 中位耗时；1× 表示等速。点标注为模型尺寸和实测倍速，连线仅用于观察趋势，不代表插值预测。两条曲线分别对应二维五场与三维九场求解，不能据此比较两种物理模型的绝对计算成本。', '',
              '[下载 SVG 矢量图](speedup.svg)', '', '### 实测结论', '']
    for dim in sizes:
        ratios = [float(groups[(dim, size, 'CUDA')]['cpu_over_cuda']) for size in sizes[dim]]
        lines.append(f'- {dim}D 三档 CUDA 加速比范围为 {min(ratios):.2f}–{max(ratios):.2f}×；小/中/稍大依次为 ' + ' / '.join(f'{r:.2f}×' for r in ratios) + '。')
    lines.append('- 以表中绝对耗时和四分位波动共同判断模拟效率；加速比不保证随网格增大而单调上升。初始化开销对很短的模拟仍可能显著。')
    lines += ['', '### 时间步、初始化与物理模拟效率', '',
              '初始化仅实测一次，不参与加速比；每秒推进物理秒数 = dt / 单步墙钟秒数，可避免只比较 steps/s 时忽略 dt 的差异。', '',
              '| 模型 | dt (ms) | CPU / CUDA 初始化 (s) | CPU / CUDA 100 步预计耗时 (s) | CPU / CUDA 物理秒/墙钟秒 |', '|---|---:|---:|---:|---:|']
    for dim in sizes:
        for size in sizes[dim]:
            c, g = (groups[(dim, size, b)] for b in ['CPU', 'CUDA'])
            dt = float(c['dt_s'])
            assert dt == float(g['dt_s'])
            lines.append(f'| {dim}D {size} | {dt*1000:.6f} | {float(c["init_ms"])/1000:.3f} / {float(g["init_ms"])/1000:.3f} | {float(c["median_ms_step"])/10:.3f} / {float(g["median_ms_step"])/10:.3f} | {dt*1000/float(c["median_ms_step"]):.5f} / {dt*1000/float(g["median_ms_step"]):.5f} |')
    lines += ['', '## 数值一致性', '',
              '比对二维全部五场、三维全部九场（含幽灵存储）和全部接收分量。每个分量分别计算相对 L2 与最大差/该分量峰值，再报告最坏分量。阈值均为 1e-9；非有限值或全零波场会使测试失败。', '',
              '这 120 步合计仅推进 0.08 s。较大模型的地表接收点距震源较远，可能尚无显著到达信号；接收记录的零差异不代表完成了非零到时验证。主要后端一致性依据是已确认非零的全波场比较。', '',
              '| 模型 | 最坏场相对 L2 | 最坏场最大误差/峰值 | 最坏接收分量相对 L2 |', '|---|---:|---:|---:|']
    for dim in sizes:
        for size in sizes[dim]:
            r = groups[(dim, size, 'CUDA')]
            lines.append(f'| {dim}D {size} | {float(r["field_relative_l2"]):.3e} | {float(r["field_max_scaled"]):.3e} | {float(r["receiver_relative_l2"]):.3e} |')
    lines += ['', '## 与 Cartesian 基准的关系', '',
              '参照文件：`SeisWaveLab_Cartesian/benchmarks/ComputeBenchmark.cpp`；当次 Cartesian 结果位于其外层 `out/benchmarks/cpu-cuda-2026-10-19/formal-2d` 和 `formal-3d`。本次只读这些参考，没有修改或重跑 Cartesian。', '',
              '| 项目 | Cartesian | 本次曲网格 |', '|---|---|---|',
              '| 尺寸、预热、计时步数、重复数、CPU 线程 | 256²/1024²/2048²；48³/96³/160³；20/100/5；32 | 相同 |',
              '| 精度与推进 | Float32，交错网格生产步进 | Float64，DRP + RK4 |',
              '| 地表与吸收 | 平面，自由表面与 CPML | 起伏曲面，自由表面与海绵吸收 |',
              '| dt | 固定 0.5 ms | 由生产求解器自动计算，CSV 记录实际值 |',
              '| 接收与诊断 | 计时不含接收记录；无本项目同等的每步检查 | 包含一个接收点和生产诊断/回滚保护 |',
              '| CUDA 同步 | 每步与每 8 步两种 | 生产每步同步；没有可等价切换的 sync8 模式 |', '',
              '**因此表中加速比只说明同一个曲网格模型的 CPU/CUDA 性能，不能用两项目绝对耗时直接判定曲网格或规则网格算法优劣。** CPU 32 线程用于复现参考口径，不代表已搜索到各尺寸的最优线程数。', '',
              '## 原始结果与复现', '',
              '- [每轮实测数据](trials.csv)、[汇总](summary.csv)、[硬件与方法](hardware.json)、[完成标志](COMPLETE.txt)。',
              '- 上一级目录包含 `provenance.json`（源码 SHA-256/提交信息）、`formal.log`、`gpu-start.csv` 与电源计划。',
              '- 从 MSVC x64 开发者终端构建：`cmake --build out/build/x64-Release --target seis_compute_benchmark --parallel 4`（在外层工作区运行）。',
              '- 运行：`seis_compute_benchmark.exe --output <外层out下的绝对路径> --threads 32 --repeats 5 --warmup 20 --steps 100`；可用 `--dimension 2/3 --size N` 单独复现。设置 `OMP_WAIT_POLICY=PASSIVE`。',
              '- 报告：`python -B benchmarks/report.py <结果目录>`（在内层仓库运行，需要 matplotlib、numpy）。', '']
    (root / 'REPORT.md').write_text('\n'.join(lines), encoding='utf-8')
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    import numpy as np
    fig, axes = plt.subplots(1, 2, figsize=(12, 5), layout='constrained')
    for ax, dim in zip(axes, sizes):
        x = np.arange(3)
        for offset, backend, color in [(-.18, 'CPU', '#3569a6'), (.18, 'CUDA', '#159788')]:
            entries = [groups[(dim, size, backend)] for size in sizes[dim]]
            values = np.array([float(r['median_ms_step']) for r in entries])
            lower = values - [float(r['p25_ms_step']) for r in entries]
            upper = np.array([float(r['p75_ms_step']) for r in entries]) - values
            bars = ax.bar(x + offset, values, .34, color=color, label=backend, yerr=[lower, upper], capsize=4)
            ax.bar_label(bars, labels=[f'{v:.2f}' for v in values], fontsize=9, padding=5)
        ax.set_xticks(x, [f'{s}^{dim}' for s in sizes[dim]])
        ax.set_yscale('log')
        ax.set_ylabel('Median ms / step (log scale)')
        ax.set_title(f'{dim}D production solver | CPU 32 threads vs CUDA')
        ax.grid(axis='y', alpha=.2)
        ax.set_axisbelow(True)
        ax.legend()
    fig.suptitle('Curvilinear CPU / CUDA | Float64 | 20 warmup + 100 timed steps x 5')
    fig.savefig(root / 'performance.png', dpi=180)
    plt.close(fig)
    fig, ax = plt.subplots(figsize=(10, 5.5), layout='constrained')
    for dim, color, marker in [(2, '#3569a6', 'o'), (3, '#159788', 's')]:
        entries = [groups[(dim, size, 'CUDA')] for size in sizes[dim]]
        nodes = [int(r['physical_nodes']) for r in entries]
        speedup = [float(r['cpu_over_cuda']) for r in entries]
        ax.plot(nodes, speedup, color=color, marker=marker, linewidth=2.2,
                markersize=8, label=f'{dim}D (CPU 32 threads / CUDA)')
        for size, n, ratio in zip(sizes[dim], nodes, speedup):
            offset = 15 if dim == 2 else -32
            if size == sizes[dim][-1]:
                offset = -24 if dim == 2 else 15
            model = f'{size}' + ('\u00b2' if dim == 2 else '\u00b3')
            ax.annotate(f'{model}  |  {ratio:.2f}\u00d7', (n, ratio),
                        xytext=(0, offset), textcoords='offset points',
                        ha='center', va='bottom' if offset > 0 else 'top',
                        fontsize=10, color=color,
                        bbox=dict(facecolor='white', edgecolor='none', alpha=.9, pad=2))
    ax.axhline(1, color='#777777', linestyle='--', linewidth=1.2, label='Equal speed (1x)')
    ax.set_xscale('log')
    ax.set_xlim(4e4, 7e6)
    ax.set_ylim(0, 5.7)
    ax.set_yticks(np.arange(0, 6))
    ax.set_xlabel('Physical grid nodes (log scale)')
    ax.set_ylabel('CUDA speedup = CPU median time / CUDA median time')
    ax.set_title('Curvilinear CPU / CUDA speedup vs model size\nFloat64 | 20 warmup + 100 timed steps | median of 5 trials')
    ax.grid(which='major', alpha=.22)
    ax.set_axisbelow(True)
    ax.legend(loc='lower left')
    fig.savefig(root / 'speedup.png', dpi=200)
    fig.savefig(root / 'speedup.svg')
    plt.close(fig)
    print(root / 'REPORT.md')


if __name__ == '__main__':
    main()
