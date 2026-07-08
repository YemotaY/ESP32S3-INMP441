"""Float DS-CNN -> int8 quantized model (TFLite-style), producing the dict consumed by
infer_int8 (Python reference) and codegen (C `kws_model_data.h`).

Weights: symmetric per-output-channel int8 for conv/depthwise/pointwise, per-tensor for
the final dense (the C dense kernel uses a single requant multiplier). Activations:
per-tensor asymmetric int8 from calibration ranges.
"""
from __future__ import annotations

import numpy as np

from . import layers
from .quant import quantize_multiplier


def quantize_input(x, in_scale, in_zp):
    """Quantize a float feature map to int8 using the model input scale/zero-point."""
    q = np.round(x / in_scale) + in_zp
    return np.clip(q, -128, 127).astype(np.int8)


def quantize_tensor_asym(amin, amax):
    amin = min(float(amin), 0.0)
    amax = max(float(amax), 0.0)
    scale = (amax - amin) / 255.0
    if scale <= 0.0:
        scale = 1e-8
    zp = int(round(-amin / scale)) - 128
    zp = max(-128, min(127, zp))
    return scale, zp


def quantize_weight_per_channel(w, out_axis=0):
    """w: (OC, ...). Returns (int8 array, scales[OC])."""
    oc = w.shape[out_axis]
    flat = w.reshape(oc, -1)
    scales = np.maximum(np.abs(flat).max(axis=1), 1e-12) / 127.0
    wq = np.clip(np.round(flat / scales[:, None]), -127, 127).astype(np.int8)
    return wq.reshape(w.shape), scales


def quantize_weight_per_tensor(w):
    scale = max(float(np.abs(w).max()), 1e-12) / 127.0
    wq = np.clip(np.round(w / scale), -127, 127).astype(np.int8)
    return wq, scale


def _bias_i32(b, bias_scale):
    return np.round(b / np.maximum(bias_scale, 1e-30)).astype(np.int64).astype(np.int32)


def float_activations(model, x):
    p, pad, c = model.p, model.pad, model.cfg
    a = {'input': x}
    h, _ = layers.conv2d_forward(x, p['conv_w'], p['conv_b'], 1, pad)
    h, _ = layers.relu_forward(h); a['conv'] = h
    if c['n_blocks']:
        h, _ = layers.depthwise_forward(h, p['dw_w'], p['dw_b'], 1, 1)
        h, _ = layers.relu_forward(h); a['dw'] = h
        h, _ = layers.conv2d_forward(h, p['pw_w'], p['pw_b'], 1, 0)
        h, _ = layers.relu_forward(h); a['pw'] = h
    g, _ = layers.global_avgpool_forward(h); a['gap'] = g
    logits, _ = layers.dense_forward(g, p['fc_w'], p['fc_b']); a['logits'] = logits
    return a


def _mults(in_scale, w_scales, out_scale):
    mult, shift = [], []
    for ws in np.atleast_1d(w_scales):
        qm, sh = quantize_multiplier(in_scale * float(ws) / out_scale)
        mult.append(qm); shift.append(sh)
    return mult, shift


def build_quant_model(model, x_calib, wake_class=1, threshold=0.5):
    c = model.cfg
    a = float_activations(model, x_calib)

    def rng(name):
        return quantize_tensor_asym(a[name].min(), a[name].max())

    in_scale, in_zp = rng('input')
    conv_scale, conv_zp = rng('conv')

    conv_wq, conv_wsc = quantize_weight_per_channel(model.p['conv_w'])
    conv_b = _bias_i32(model.p['conv_b'], in_scale * conv_wsc)
    conv_mult, conv_shift = _mults(in_scale, conv_wsc, conv_scale)

    qm = {
        'in_h': c['in_h'], 'in_w': c['in_w'],
        'conv_w': conv_wq, 'conv_b': conv_b,
        'conv_mult': conv_mult, 'conv_shift': conv_shift,
        'conv_oc': c['conv_oc'], 'conv_kh': c['conv_k'], 'conv_kw': c['conv_k'],
        'conv_stride': 1, 'conv_pad': model.pad,
        'n_blocks': c['n_blocks'],
        'in_scale': in_scale,
        'in_zp': in_zp, 'conv_zp': conv_zp,
        'dw_k': 3, 'dw_stride': 1, 'dw_pad': 1,
    }

    if c['n_blocks']:
        dw_scale, dw_zp = rng('dw')
        pw_scale, pw_zp = rng('pw')
        dw_wq, dw_wsc = quantize_weight_per_channel(model.p['dw_w'].transpose(2, 0, 1))
        dw_wq = dw_wq.transpose(1, 2, 0)  # back to (KH,KW,C)
        pw_wq, pw_wsc = quantize_weight_per_channel(model.p['pw_w'])
        qm.update({
            'dw_w': dw_wq, 'dw_b': _bias_i32(model.p['dw_b'], conv_scale * dw_wsc),
            'dw_mult': _mults(conv_scale, dw_wsc, dw_scale)[0],
            'dw_shift': _mults(conv_scale, dw_wsc, dw_scale)[1],
            'dw_zp': dw_zp,
            'pw_w': pw_wq, 'pw_b': _bias_i32(model.p['pw_b'], dw_scale * pw_wsc),
            'pw_mult': _mults(dw_scale, pw_wsc, pw_scale)[0],
            'pw_shift': _mults(dw_scale, pw_wsc, pw_scale)[1],
            'pw_oc': c['pw_oc'], 'pw_zp': pw_zp,
        })
        gap_in_scale, gap_in_zp = pw_scale, pw_zp
    else:
        qm.update({'dw_zp': 0, 'pw_oc': c['conv_oc'], 'pw_zp': 0})
        gap_in_scale, gap_in_zp = conv_scale, conv_zp

    # Global average pool: out = requant(sum(in-in_zp)); real mult = in_scale/(out_scale*N).
    n_pool = c['in_h'] * c['in_w']
    gap_scale, gap_zp = rng('gap')
    gm, gs = quantize_multiplier(gap_in_scale / (gap_scale * n_pool))
    qm.update({'gap_mult': gm, 'gap_shift': gs, 'gap_zp': gap_zp})

    # Final dense: per-tensor weight scale (C dense uses one requant multiplier).
    out_scale, out_zp = rng('logits')
    fc_wq, fc_wsc = quantize_weight_per_tensor(model.p['fc_w'])
    fc_mult, fc_shift = quantize_multiplier(gap_scale * fc_wsc / out_scale)
    qm.update({
        'fc_w': fc_wq, 'fc_b': _bias_i32(model.p['fc_b'], gap_scale * fc_wsc),
        'fc_mult': fc_mult, 'fc_shift': fc_shift, 'num_classes': c['num_classes'],
        'out_zp': out_zp, 'out_scale': out_scale,
        'wake_class': wake_class, 'threshold': threshold,
    })
    return qm
