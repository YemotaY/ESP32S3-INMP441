"""int8 DS-CNN inference reference, mirroring firmware/core/src/kws_model.c.

Consumes a quantized-model dict (see quantize.build_quant_model / gen_parity_case) and
an int8 input feature map, returning int8 logits identical to the C engine.
"""
from __future__ import annotations

import numpy as np

from . import kernels


def infer(qm: dict, inp_i8: np.ndarray) -> np.ndarray:
    """qm: quantized model dict. inp_i8: (in_h, in_w) int8 (1 channel). -> (num_classes,) int8."""
    x = inp_i8.reshape(qm['in_h'], qm['in_w'], 1).astype(np.int8)

    # conv2d + ReLU (ReLU folded via act_min == conv_zp).
    h = kernels.conv2d_i8(x, qm['conv_w'], qm['conv_b'], qm['conv_stride'], qm['conv_pad'],
                          qm['in_zp'], qm['conv_mult'], qm['conv_shift'],
                          qm['conv_zp'], qm['conv_zp'], 127)

    gap_in_zp = qm['conv_zp']
    if qm['n_blocks'] >= 1:
        h = kernels.depthwise_conv2d_i8(h, qm['dw_w'], qm['dw_b'], qm['dw_stride'],
                                        qm['dw_pad'], qm['conv_zp'],
                                        qm['dw_mult'], qm['dw_shift'],
                                        qm['dw_zp'], qm['dw_zp'], 127)
        h = kernels.conv2d_i8(h, qm['pw_w'], qm['pw_b'], 1, 0, qm['dw_zp'],
                              qm['pw_mult'], qm['pw_shift'],
                              qm['pw_zp'], qm['pw_zp'], 127)
        gap_in_zp = qm['pw_zp']

    pooled = kernels.global_avgpool_i8(h, gap_in_zp, qm['gap_mult'], qm['gap_shift'],
                                       qm['gap_zp'], -128, 127)

    logits = kernels.fully_connected_i8(pooled, qm['fc_w'], qm['fc_b'], qm['gap_zp'],
                                        qm['fc_mult'], qm['fc_shift'],
                                        qm['out_zp'], -128, 127)
    return logits
