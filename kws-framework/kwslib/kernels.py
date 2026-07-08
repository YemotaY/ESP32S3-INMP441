"""int8 NN kernels, a bit-exact mirror of firmware/core/src/nn/kernels.c.

NHWC layout, symmetric weights (zero-point 0), per-output-channel requant. Uses
Python-int accumulation so results match the C engine exactly. Sizes are tiny (KWS
models), so plain loops are fine.
"""
from __future__ import annotations

import numpy as np

from . import quant


def fully_connected_i8(inp, weights, bias, input_zp, out_mult, out_shift,
                       output_zp, act_min, act_max):
    """inp: (in_dim,), weights: (out_dim, in_dim)."""
    in_dim = inp.shape[0]
    out_dim = weights.shape[0]
    out = np.empty(out_dim, dtype=np.int8)
    for o in range(out_dim):
        acc = int(bias[o]) if bias is not None else 0
        for i in range(in_dim):
            acc += (int(inp[i]) - input_zp) * int(weights[o, i])
        out[o] = quant.requantize_i8(acc, out_mult, out_shift,
                                     output_zp, act_min, act_max)
    return out


def conv2d_i8(inp, weights, bias, stride, pad, input_zp,
              out_mult, out_shift, output_zp, act_min, act_max):
    """inp: (H,W,IC), weights: (OC,KH,KW,IC). Zero-padding == input_zp.
    out_mult/out_shift are per-output-channel lists."""
    in_h, in_w, in_c = inp.shape
    out_c, filt_h, filt_w, _ = weights.shape
    oh = (in_h + 2 * pad - filt_h) // stride + 1
    ow = (in_w + 2 * pad - filt_w) // stride + 1
    out = np.empty((oh, ow, out_c), dtype=np.int8)
    for y in range(oh):
        for x in range(ow):
            for oc in range(out_c):
                acc = int(bias[oc]) if bias is not None else 0
                for fy in range(filt_h):
                    iy = y * stride - pad + fy
                    for fx in range(filt_w):
                        ix = x * stride - pad + fx
                        inside = 0 <= iy < in_h and 0 <= ix < in_w
                        for ic in range(in_c):
                            in_v = int(inp[iy, ix, ic]) if inside else input_zp
                            acc += (in_v - input_zp) * int(weights[oc, fy, fx, ic])
                out[y, x, oc] = quant.requantize_i8(
                    acc, out_mult[oc], out_shift[oc], output_zp, act_min, act_max)
    return out


def depthwise_conv2d_i8(inp, weights, bias, stride, pad, input_zp,
                        out_mult, out_shift, output_zp, act_min, act_max):
    """inp: (H,W,C), weights: (KH,KW,C). depth multiplier 1."""
    in_h, in_w, in_c = inp.shape
    filt_h, filt_w, _ = weights.shape
    oh = (in_h + 2 * pad - filt_h) // stride + 1
    ow = (in_w + 2 * pad - filt_w) // stride + 1
    out = np.empty((oh, ow, in_c), dtype=np.int8)
    for y in range(oh):
        for x in range(ow):
            for c in range(in_c):
                acc = int(bias[c]) if bias is not None else 0
                for fy in range(filt_h):
                    iy = y * stride - pad + fy
                    for fx in range(filt_w):
                        ix = x * stride - pad + fx
                        inside = 0 <= iy < in_h and 0 <= ix < in_w
                        in_v = int(inp[iy, ix, c]) if inside else input_zp
                        acc += (in_v - input_zp) * int(weights[fy, fx, c])
                out[y, x, c] = quant.requantize_i8(
                    acc, out_mult[c], out_shift[c], output_zp, act_min, act_max)
    return out


def global_avgpool_i8(inp, input_zp, out_mult, out_shift,
                      output_zp, act_min, act_max):
    """inp: (H,W,C) -> (C,). 1/(H*W) folded into out_mult."""
    in_h, in_w, in_c = inp.shape
    out = np.empty(in_c, dtype=np.int8)
    for c in range(in_c):
        acc = 0
        for y in range(in_h):
            for x in range(in_w):
                acc += int(inp[y, x, c]) - input_zp
        out[c] = quant.requantize_i8(acc, out_mult, out_shift,
                                     output_zp, act_min, act_max)
    return out
