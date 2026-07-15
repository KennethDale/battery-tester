#!/usr/bin/env python3
"""
Early capacity estimator for the battery tester.

Estimates the final delivered capacity (mAh) of a constant-current discharge
from only the first few minutes of a run.

KEY FINDING
-----------
These cells have a flat LiPo voltage plateau and are discharged at ~constant
current (~0.9 A). During the plateau the voltage barely moves and every cell
delivers nearly the same charge per minute, so the shape of the first 5-20 min
carries almost no information about total capacity. What *does* predict the
total is the STARTING (rested) voltage V0, which encodes the initial state of
charge. A single-feature linear calibration on V0 gives a median error of
~6-7% -- and a 5-minute window is essentially as accurate as a 20-minute one.

Coulomb-counting / voltage-fuel-gauge extrapolation performs far worse early on
precisely because of the flat plateau (dividing by a tiny, noisy
discharge-fraction blows up).

CALIBRATION (leave-one-out cross-validated on the 13 clean runs in results/)
    final_mAh ~= 1228.6 * V0 - 3486.5
The window length does not change the fit: V0 is the first sample, so 5, 10 and
20 min give the same estimate (the flat plateau carries no extra early signal).
LOOCV MAPE ~21%, median ~7%. The mean is inflated by one genuine outlier this
cannot catch: a cell that read full (V0=3.54) but whose test was cut short
(channel_0_0_101841). ALWAYS sanity-check V0 is in the 3.0-3.6 V band.

These are the same constants the firmware uses (see src/config.h
CAPACITY_ESTIMATE_*). Keep the two in sync if you re-fit.

NOTE: capacity is re-derived by integrating current (coulomb counting) rather
than trusting the logged `capacity_mah` column. The old-firmware files whose
column was ~1000x too large have been removed; current firmware logs it
correctly (matches the coulomb count within ~1%).
"""
import sys, glob
import numpy as np
import pandas as pd

# V0-based calibration (slope, intercept). Mirror of src/config.h constants.
SLOPE, INTERCEPT = 1228.6, -3486.5
CALIB = {5: (SLOPE, INTERCEPT), 10: (SLOPE, INTERCEPT), 20: (SLOPE, INTERCEPT)}

def load(path):
    df = pd.read_csv(path).apply(pd.to_numeric, errors='coerce')
    df = df.dropna(subset=['elapsed_s', 'voltage', 'current_a'])
    if len(df) < 5:
        return None
    df = df.sort_values('elapsed_s').reset_index(drop=True)
    df['t'] = df['elapsed_s'] - df['elapsed_s'].iloc[0]
    return df

def delivered_mah(df, tmax=None):
    d = df if tmax is None else df[df['t'] <= tmax]
    if len(d) < 2:
        return 0.0
    return float(np.trapezoid(d['current_a'].values, d['t'].values) / 3600.0 * 1000.0)

def estimate(df, minutes):
    """Estimate final capacity (mAh) from the first `minutes` of a run."""
    slope, intercept = CALIB.get(minutes, CALIB[10])
    V0 = df['voltage'].iloc[0]
    est = slope * V0 + intercept
    warn = "" if 3.0 <= V0 <= 3.6 else "  [!] V0 outside 3.0-3.6V calibration band"
    return est, V0, warn

if __name__ == '__main__':
    files = sys.argv[1:] or sorted(glob.glob('*.csv'))
    print(f"{'file':28s} {'V0':>5} {'5min':>7} {'10min':>7} {'20min':>7} {'actual':>7}")
    for f in files:
        df = load(f)
        if df is None:
            continue
        if not (df['voltage'].iloc[0] > 2.5 and df['current_a'].max() > 0.3):
            continue
        actual = delivered_mah(df) if df['voltage'].iloc[-1] < 1.5 else float('nan')
        cells = []
        for m in (5, 10, 20):
            if df['t'].iloc[-1] >= m * 60 * 0.9:
                est, V0, _ = estimate(df, m)
                cells.append(f"{est:7.0f}")
            else:
                cells.append(f"{'--':>7}")
        print(f"{f:28s} {df['voltage'].iloc[0]:5.2f} {cells[0]} {cells[1]} {cells[2]} {actual:7.0f}")
