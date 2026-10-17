# 维护与文档工具

从 Git 仓库根目录运行。所有工具的默认输出均为仓库外的 `../out`，请使用 `python -B` 避免产生仓库内 Python 缓存。Python 分析及文档依赖由 `requirements.txt` 列出；C++ 模拟器不依赖这些分析包。

| 命令/脚本 | 用途与输出 |
| --- | --- |
| `python -B tools/check_repository.py` | 只读检查文档本地链接、必需源资源、外部构建预设和误放的生成文件 |
| `python -B tools/package_documentation.py` | 从 docs 正文和发表图片生成离线说明与 GitHub Pages/Hexo 包；输出到 `../out/documentation`、`../out/blog` |
| `python -B tools/analyze_dispersion.py` | 均匀内点 P/S 波频散与振幅分析；输出到 `../out/validation/dispersion` |
| `python -B tools/analyze_cfl_symbol.py` | 交替方向八步 Fourier 周期审计；输出到 `../out/validation/cfl_symbol.json` |
| `python -B tools/verify_exports.py` | 验证已有导出；默认读取 `../out`，可传显式目录 |
| `generate_upstream_reference.py` | 由 CMake 调用，将仓库内三个原始参考文件的选定函数提取到外部构建目录 |

数值分析脚本各有明确的内点与参数假设，结果不能代替自由表面、吸收区和复杂介质的完整精度验证。更新发表图片需经过人工核对，再更新 docs/images 中的 PNG 与 capture.tsv；打包工具不会自动用外层原始截图覆盖仓库资源。
