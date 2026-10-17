# CGFD3D reference source subset

Unmodified files retained solely for the direct RHS validation target. These are original upstream source files, not generated build headers. The main solver does not compile or link this upstream application.

Upstream: https://github.com/zw-sustech/CGFD3D

Source snapshot: the existing local CGFD3D-master/forward used by this project. No upstream commit ID was recorded; SHA-256 identifies the exact retained files. Do not infer a commit or release from the snapshot folder name.

| File | SHA-256 |
| --- | --- |
| `fd_t.h` | `cb527c3d1be0f26a92ae2354c2466273bad3ae9a582ee0f84eee1ca469b7b7fa` |
| `sv_curv_col_el.c` | `fd8edd15da90ca66093931606415f559af05b6a967ec5849915dfa4d486fd4bf` |
| `sv_curv_col_el_iso.c` | `4621ed5a35ad02bc9235bd8fcc922ed4911b6e58a664b6cb021fc7570e5fe09b` |

License: the upstream GPL-3.0 notice is retained in `LICENSE` and the repository root. `tools/generate_upstream_reference.py` extracts selected bodies at build time into the external build directory; portability substitutions and function hashes are recorded in that generated header.
