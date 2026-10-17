"""Extract unchanged CGFD3D RHS bodies for a local numerical cross-check.

Only portability substitutions: C restrict, VLA extents (maximum five), and
function names. The original local repository remains read-only.
"""
import argparse
import hashlib
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--source',required=True);p.add_argument('--output',required=True);args=p.parse_args()
root=Path(args.source);output=Path(args.output)
header='''// Generated from local CGFD3D; see THIRD_PARTY_NOTICES.md. DO NOT EDIT.
#pragma once
#include <cstddef>
namespace Upstream {
struct fd_op_t { int total_len; int* indx; float* coef; };
'''
fd=(root/'fd_t.h').read_text(encoding='utf-8')
header+=fd[fd.index('#define M_FD_SHIFT('):fd.index('// use indx relative')]
for filename,name in [('sv_curv_col_el_iso.c','sv_curv_col_el_iso_rhs_inner'),('sv_curv_col_el.c','sv_curv_col_el_rhs_timg_z2'),('sv_curv_col_el_iso.c','sv_curv_col_el_iso_rhs_vlow_z2')]:
    data=(root/filename).read_text(encoding='utf-8');start=data.index('\nvoid\n'+name+'(')+1
    begin=data.index('{',start);level=1;end=begin+1
    # Braces in these functions' comments are balanced, same as the C bodies.
    while level:
        level+=(data[end]=='{')-(data[end]=='}');end+=1
    function=data[start:end]
    header+=f'// {filename}: sha256={hashlib.sha256(function.encode()).hexdigest()}\n'
    function=function.replace('restrict','')
    for size in ['fdx_len','fdy_len','fdz_len','fdz_max_len']:function=function.replace('['+size+']','[5]')
    header+=function+'\n'
header+='\n#undef M_FD_SHIFT\n#undef M_FD_SHIFT_PTR\n#undef M_FD_SHIFT_PTR_MACDRP\n#undef M_FD_NOINDX\n}\n'
output.parent.mkdir(parents=True,exist_ok=True);output.write_text(header,encoding='utf-8')
