"""Command line companion to the notebooks. No simulation or inversion is run."""
import argparse,sys
from pathlib import Path
sys.dont_write_bytecode=True
sys.path.insert(0,str(Path(__file__).resolve().parent))
from seisplot.core import load_config
from seisplot.four_case import FourCaseStudy
from seisplot.surface_wave import SurfaceWaveStudy
from seisplot.inversion import InversionStudy

def run(kind,config,steps=None):
    classes={'four':FourCaseStudy,'masw':SurfaceWaveStudy,'inversion':InversionStudy}
    study=classes[kind](config)
    methods={
      'four':{'inspect':'inspect','terrain':'plot_terrain_profile','records':'plot_records','models':'plot_models','wavefields':'plot_wavefields','metrics':'plot_metrics','frequencies':'plot_frequency_compare','reflections':'plot_reflections'},
      'masw':{'inspect':'inspect','records':'plot_records','models':'plot_models','wavefields':'plot_wavefields','differences':'plot_record_differences','traces':'plot_trace_overlay','dispersion':'plot_dispersion','windows':'plot_offset_windows','picks':'plot_phase_curves'},
      'inversion':{'models':'plot_models','fit':'plot_curve_fit','multistart':'plot_multistart','thickness':'plot_thickness_scan','sensitivity':'plot_sensitivity'}}[kind]
    for step in steps or list(methods):
        if step not in methods:raise ValueError(f'Unknown {kind} step: {step}; use {list(methods)}')
        getattr(study,methods[step])();print('Completed:',step,flush=True)
    return study

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--kind',choices=['four','masw','inversion'],required=True)
    p.add_argument('--config',type=Path,required=True)
    p.add_argument('--steps',help='Comma-separated steps; omit to run all')
    a=p.parse_args();study=run(a.kind,load_config(a.config),a.steps.split(',') if a.steps else None)
    print('Output:',study.out)
