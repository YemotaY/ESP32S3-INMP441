"""Dataset helpers: waveform augmentation + a synthetic feature-map generator.

The synthetic generator produces class-separable log-mel-like feature maps so the
trainer and quantizer can be tested end-to-end without recorded audio. Real datasets
(recorded via the firmware capture mode) plug into the same (X, y) interface.
"""
from __future__ import annotations

import numpy as np


# --- waveform augmentation (operate on float [-1,1] mono signals) ---

def add_noise(sig, snr_db, rng):
    p_sig = np.mean(sig ** 2) + 1e-12
    p_noise = p_sig / (10.0 ** (snr_db / 10.0))
    noise = rng.standard_normal(len(sig)) * np.sqrt(p_noise)
    return sig + noise


def time_shift(sig, max_shift, rng):
    s = int(rng.integers(-max_shift, max_shift + 1))
    return np.roll(sig, s)


def gain(sig, min_db, max_db, rng):
    g = 10.0 ** (rng.uniform(min_db, max_db) / 20.0)
    return np.clip(sig * g, -1.0, 1.0)


# --- synthetic feature-map dataset ---

def make_synthetic(n_per_class, in_h, in_w, num_classes, seed=0, noise=0.3):
    """Each class k has an activated horizontal band; classifier must localise it.
    Returns X (N, in_h, in_w, 1) float, y (N,) int."""
    rng = np.random.default_rng(seed)
    band = in_h // num_classes
    xs, ys = [], []
    for k in range(num_classes):
        for _ in range(n_per_class):
            m = rng.standard_normal((in_h, in_w)) * noise
            lo = k * band
            hi = in_h if k == num_classes - 1 else (k + 1) * band
            m[lo:hi, :] += 2.0
            xs.append(m[..., None])
            ys.append(k)
    X = np.array(xs)
    y = np.array(ys)
    perm = rng.permutation(len(y))
    return X[perm], y[perm]
