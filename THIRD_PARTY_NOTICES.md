# Third-party notices

This project adapts the curvilinear-grid metric transformation and isotropic velocity-stress formulation from **CGFD3D**, by Zhang Wei and collaborators, under written authorization from the authors and the upstream GPL-3.0 license. The upstream source snapshot used for this work is documented in the development record; the upstream LICENSE is included as `LICENSE`.

Upstream project: https://github.com/zw-sustech/CGFD3D

The initial prototype omitted CGFD3D's MPI orchestration, NetCDF I/O and full boundary implementation. The current solver includes the alternating MacCormack-DRP coefficient/schedule adaptation, a local traction-rate free-surface closure and Jacobian/metric-weighted traction images, with independently documented differences and tests. It does not reproduce CGFD3D's complete MPI/NetCDF/CFS-PML application, and full waveform equivalence is not claimed.

The independent GUI vendors Dear ImGui 1.92.6 WIP (docking) from the existing local SeisWave Lab dependency snapshot, by Omar Cornut and contributors. Its MIT license is included in `Seis3D_Curvilinear/third_party/imgui/LICENSE.txt`; upstream: https://github.com/ocornut/imgui. GLFW and GLEW are linked from caller-configurable external SDK locations and are not vendored here.

The record viewer vendors ImPlot 0.18 WIP from the same local SeisSim V2 dependency snapshot, by Evan Pezent, Breno Cunha Queiroz and contributors. Its MIT license is preserved in the source headers and `Seis3D_Curvilinear/third_party/implot/LICENSE.txt`; upstream: https://github.com/epezent/implot. The AGC, extrema reduction and record palette definitions are adapted from this user's SeisSim V2 project; their local sources were `Main_header/TraceProcessing.h` and `Main_header/WavePalettes.h`.
