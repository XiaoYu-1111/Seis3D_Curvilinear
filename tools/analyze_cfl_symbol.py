"""Fourier audit of the actual alternating nine-field elastic DRP/RK cycle.

This is an interior, constant-material necessary check, not a free-surface or
heterogeneous-grid stability proof. All generated data goes under out/.
"""
import argparse
import json
from pathlib import Path
import numpy as np


def audit(cfl, ratio, samples):
    theta = np.linspace(-np.pi, np.pi, samples)
    waves = np.stack(np.meshgrid(theta, theta, theta, indexing="ij"), -1).reshape(-1, 3)
    coef = np.array([-.30874, -.6326, 1.233, -.3334, .04168])
    forward = (np.exp(1j * waves[:, :, None] * np.arange(-1, 4)) * coef).sum(-1)
    backward = -forward.conj()
    dt = cfl / np.sqrt(3)
    mu, lam = ratio**2, 1 - 2 * ratio**2
    identity = np.broadcast_to(np.eye(9, dtype=complex), (len(waves), 9, 9))
    cycle = identity.copy()
    worst_step = 0
    for flags in [(0,0,0), (1,1,0), (1,1,1), (0,0,1), (0,1,0), (1,0,0), (1,0,1), (0,1,1)]:
        rates = []
        stage = identity
        for s in range(4):
            d = np.where((np.array(flags) + s) % 2, backward, forward)
            op = np.zeros_like(identity)
            for i, stress in enumerate([(3,6,7), (6,4,8), (7,8,5)]):
                for a in range(3):
                    op[:, i, stress[a]] = d[:, a]
                for a in range(3):
                    op[:, i+3, a] = (lam + (2*mu if i==a else 0))*d[:, a]
            for stress, a, b in [(6,0,1),(7,0,2),(8,1,2)]:
                op[:, stress, a] = mu*d[:, b]
                op[:, stress, b] = mu*d[:, a]
            rates.append(op @ stage)
            if s < 3:
                stage = identity + dt*(1 if s==2 else .5)*rates[-1]
        step = identity + dt*(rates[0]+2*rates[1]+2*rates[2]+rates[3])/6
        worst_step = max(worst_step, float(np.max(np.abs(np.linalg.eigvals(step)))))
        cycle = step @ cycle
    radius = np.max(np.abs(np.linalg.eigvals(cycle)), axis=1)
    index = np.argmax(radius)
    return dict(cfl=cfl, vs_over_vp=ratio, samples_per_axis=samples,
                max_cycle_radius=float(radius[index]), max_step_radius=worst_step,
                worst_theta_over_pi=(waves[index]/np.pi).tolist())


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--samples", type=int, default=17)
    parser.add_argument("--cfl", type=float, nargs="+", default=[.22,.5,.8,1,1.2,1.3,1.4])
    parser.add_argument("--ratios", type=float, nargs="+", default=[.05,1700/3000,.85])
    args = parser.parse_args()
    results = []
    for ratio in args.ratios:
        for cfl in args.cfl:
            row = audit(cfl, ratio, args.samples)
            print(json.dumps(row), flush=True)
            results.append(row)
    path = Path(__file__).resolve().parents[2]/"out"/"validation"/"cfl_symbol.json"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(results, indent=2), encoding="utf-8")
