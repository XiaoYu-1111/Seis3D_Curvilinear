"""Interior x-directed P/S Fourier comparison. No terrain, source or boundaries.

Uses repository DRP coefficients and eight-step stage schedule, versus V2 SG8
and second-order leapfrog. Both use the same normalized dt*Vp/h=0.1.
Output is a reproducible necessary accuracy comparison, not a full solver test.
"""
import csv
import json
from pathlib import Path
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT.parent / 'out/validation/dispersion'
RATIO, DT = 1700/3000, .1
COEF = np.array([-.30874, -.6326, 1.233, -.3334, .04168])
# Exact order from PointKernels3D.hpp.
FLAGS = [(0,0,0),(1,1,0),(1,1,1),(0,0,1),(0,1,0),(1,0,0),(1,0,1),(0,1,1)]

def drp(ppw, shear):
    speed = RATIO if shear else 1.
    theta = 2*np.pi/ppw
    forward = np.array([(COEF*np.exp(1j*np.arange(-1,4)*theta)).sum(), COEF.sum(), COEF.sum()])
    identity = np.eye(9, dtype=complex)
    cycle = identity.copy()
    mu, lam = RATIO**2, 1-2*RATIO**2
    for flags in FLAGS:
        rates, stage = [], identity
        for s in range(4):
            d = np.where((np.array(flags)+s)%2, -forward.conj(), forward)
            op = np.zeros((9,9), complex)
            for i, stress in enumerate([(3,6,7),(6,4,8),(7,8,5)]):
                for a in range(3):
                    op[i,stress[a]] = d[a]
                    op[i+3,a] = (lam+(2*mu if i==a else 0))*d[a]
            for stress,a,b in [(6,0,1),(7,0,2),(8,1,2)]:
                op[stress,a],op[stress,b] = mu*d[b],mu*d[a]
            rates.append(op@stage)
            if s<3: stage = identity+DT*(1 if s==2 else .5)*rates[-1]
        cycle = (identity+DT*(rates[0]+2*rates[1]+2*rates[2]+rates[3])/6)@cycle
    values,vectors = np.linalg.eig(cycle)
    # Two S polarizations are degenerate. Eigenvectors inside that subspace are
    # arbitrary; raw overlap with one S vector can select the opposite branch.
    velocity = abs(vectors[:3])**2
    norm = velocity.sum(axis=0)
    polarization = (velocity[1]+velocity[2] if shear else velocity[0])/np.maximum(norm,1e-30)
    candidates = np.where((norm>1e-10)&(polarization>.9)&(-np.angle(values)>0))[0]
    assert len(candidates), ('No propagating branch',ppw,shear)
    expected = speed*theta*8*DT
    index = candidates[np.argmin(abs(-np.angle(values[candidates])-expected))]
    value = values[index]
    phase = -np.angle(value)
    phase += 2*np.pi*round((expected-phase)/(2*np.pi))
    amplitude = abs(value)**(ppw/(8*DT*speed))
    return phase/expected,amplitude

def sg8(ppw,shear):
    speed = RATIO if shear else 1.
    theta = 2*np.pi/ppw
    w = np.array([1225/1024,-245/3072,49/5120,-5/7168])
    kh = 2*np.dot(w,np.sin((np.arange(4)+.5)*theta))
    return 2*np.arcsin(speed*DT*kh/2)/(speed*DT*theta)

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    rows=[]
    for shear in [False,True]:
        for ppw in np.linspace(2.01,20,180):
            phase,amp=drp(ppw,shear)
            rows.append(dict(wave='S' if shear else 'P',ppw=ppw,drp_phase_ratio=phase,drp_amplitude_per_wavelength=amp,sg8_phase_ratio=sg8(ppw,shear)))
    with (OUT/'phase_comparison.csv').open('w',newline='',encoding='utf-8') as f:
        writer=csv.DictWriter(f,fieldnames=rows[0].keys());writer.writeheader();writer.writerows(rows)
    fig,ax=plt.subplots(1,2,figsize=(12,4.3),layout='constrained')
    for shear in [False,True]:
        subset=[r for r in rows if r['wave']==('S' if shear else 'P')]
        x=[r['ppw'] for r in subset];label='S' if shear else 'P'
        ax[0].plot(x,[100*(r['drp_phase_ratio']-1) for r in subset],label=f'DRP/RK {label}')
        ax[0].plot(x,[100*(r['sg8_phase_ratio']-1) for r in subset],'--',label=f'SG8/leapfrog {label}')
        ax[1].plot(x,[r['drp_amplitude_per_wavelength'] for r in subset],label=f'DRP/RK {label}')
    ax[0].set(ylabel='Phase velocity error (%)',ylim=(-100,5));ax[1].set(ylabel='Amplitude after one wavelength',ylim=(0,1.02))
    for a in ax:
        a.set(xlabel='Grid points per wavelength',xlim=(2,20));a.grid(alpha=.25);a.legend();a.axvline(8,color='gray',lw=.8)
    fig.suptitle('Interior x-directed waves | Vp/Vs = 3000/1700 | dt Vp/h = 0.1 | no boundaries')
    for shear in [False,True]:
        ratios=np.array([r['drp_phase_ratio'] for r in rows if r['wave']==('S' if shear else 'P')])
        assert np.all((ratios>0)&(ratios<1.02)) and np.max(abs(np.diff(ratios)))<.2,'Discontinuous propagation branch'
    fig.savefig(OUT/'dispersion-comparison.png',dpi=180);plt.close(fig)
    samples=[]
    for f in [12,30,50,60]:
        for wave,v in [('P',3000),('S',1700)]:
            ppw=v/(f*10);phase,amp=drp(ppw,wave=='S')
            samples.append(dict(f0_hz=f,wave=wave,spacing_m=10,ppw_at_f0=ppw,ppw_at_2_5_f0=ppw/2.5,drp_phase_error_percent=(phase-1)*100,drp_amplitude_per_wavelength=amp,sg8_phase_error_percent=(sg8(ppw,wave=='S')-1)*100))
    (OUT/'summary.json').write_text(json.dumps(dict(normalized_dt=DT,assumptions=__doc__,cases=samples),indent=2),encoding='utf-8')
    for row in samples: print(json.dumps(row),flush=True)

if __name__=='__main__': main()
