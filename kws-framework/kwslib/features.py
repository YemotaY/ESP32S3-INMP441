"""Log-mel feature extraction (numpy), mirroring the formula in
firmware/core/src/dsp/melspec.c so training features match the device front-end.

(Float results are not asserted bit-identical to C here; the *model* parity is what is
guaranteed bit-exact. A cffi bridge to the C DSP can be added later for exact skew-free
features.)
"""
from __future__ import annotations

import numpy as np


def hz_to_mel(hz):
    return 2595.0 * np.log10(1.0 + hz / 700.0)


def mel_to_hz(mel):
    return 700.0 * (10.0 ** (mel / 2595.0) - 1.0)


def mel_filterbank(sample_rate, fft_size, n_mels, fmin, fmax):
    n_bins = fft_size // 2 + 1
    edges = mel_to_hz(np.linspace(hz_to_mel(fmin), hz_to_mel(fmax), n_mels + 2))
    freqs = np.arange(n_bins) * sample_rate / fft_size
    fb = np.zeros((n_mels, n_bins))
    for m in range(n_mels):
        left, center, right = edges[m], edges[m + 1], edges[m + 2]
        for k in range(n_bins):
            f = freqs[k]
            if left <= f <= center and center > left:
                fb[m, k] = (f - left) / (center - left)
            elif center < f <= right and right > center:
                fb[m, k] = (right - f) / (right - center)
    return fb


def log_mel_frames(signal, sample_rate=16000, frame_len=400, frame_step=160,
                   fft_size=512, n_mels=40, fmin=20.0, fmax=None, preemph=0.97):
    if fmax is None:
        fmax = sample_rate / 2.0
    window = 0.5 - 0.5 * np.cos(2 * np.pi * np.arange(frame_len) / (frame_len - 1))
    fb = mel_filterbank(sample_rate, fft_size, n_mels, fmin, fmax)

    frames = []
    n = len(signal)
    for start in range(0, n - frame_len + 1, frame_step):
        seg = signal[start:start + frame_len].astype(np.float64).copy()
        if preemph:
            pe = seg.copy()
            pe[1:] = seg[1:] - preemph * seg[:-1]
            pe[0] = seg[0] - preemph * 0.0
            seg = pe
        seg = seg * window
        spec = np.fft.rfft(seg, n=fft_size)
        power = (spec.real ** 2 + spec.imag ** 2)
        mel = fb @ power
        frames.append(np.log(mel + 1e-10))
    if not frames:
        return np.zeros((0, n_mels))
    return np.stack(frames)
