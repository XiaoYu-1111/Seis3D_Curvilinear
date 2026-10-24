"""Check clean notebook sources; optionally execute real local-data smoke paths.

All execution outputs go to ../out/validation/seismic_plotting. The source
notebooks are never modified. --execute requires the example simulation data.
"""
import sys,json,ast,argparse,contextlib,io,copy
from pathlib import Path
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT))
from seisplot.core import WORKSPACE

def check(execute=False):
    output=WORKSPACE/'out/validation/seismic_plotting/notebooks';output.mkdir(parents=True,exist_ok=True)
    results=[]
    for path in sorted(ROOT.glob('*.ipynb')):
        nb=json.loads(path.read_text(encoding='utf8'));assert nb['nbformat']==4
        assert len({c['id'] for c in nb['cells']})==len(nb['cells'])
        code=[c for c in nb['cells'] if c['cell_type']=='code']
        for cell in code:
            assert cell['outputs']==[] and cell['execution_count'] is None
            ast.parse(''.join(cell['source']))
        if execute:
            env={'__name__':'__main__'};clone=copy.deepcopy(nb);count=0
            for cell in clone['cells']:
                if cell['cell_type']!='code':continue
                count+=1;stream=io.StringIO()
                with contextlib.redirect_stdout(stream):
                    exec(compile(''.join(cell['source']),str(path)+f':cell{count}','exec'),env)
                    if 'parameters' in cell['metadata'].get('tags',[]):
                        env['SHOW_INLINE']=False;env['CONFIG']['output_dir']=str(output/path.stem)
                        if path.name.startswith('01'):
                            # All 15/30Hz records and kinematic comparisons are exercised.
                            # Native terrain VTS geometry is separately checked in integration.
                            env['RUN_MODELS']=False;env['RUN_WAVEFIELDS']=False
                        if path.name.startswith('02'):
                            env['CONFIG']['snapshots']['times_s']=[.2]
                            env['CONFIG']['dispersion']['frequency_hz']=[8,40,1]
                            env['CONFIG']['dispersion']['velocity_m_s']=[180,1200,4]
                            env['CONFIG']['models'].update(stride_x=8,stride_z=8)
                            env['CONFIG']['snapshots'].update(stride_x=8,stride_z=8)
                cell['execution_count']=count
                cell['outputs']=[dict(output_type='stream',name='stdout',text=stream.getvalue().splitlines(keepends=True))]
            (output/path.name).write_text(json.dumps(clone,ensure_ascii=False,indent=1),encoding='utf8')
        results.append(dict(notebook=path.name,code_cells=len(code),clean_source=True,executed=execute))
        print('PASS',path.name,flush=True)
    receipt='execution_verification.json' if execute else 'source_verification.json'
    (output/receipt).write_text(json.dumps(results,ensure_ascii=False,indent=2),encoding='utf8')
    return results

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--execute',action='store_true')
    check(p.parse_args().execute)
