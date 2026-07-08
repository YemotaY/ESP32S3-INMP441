"""Float DS-CNN layers with forward + analytic backward (numpy), for training.

Layout NHWC. Each forward returns (output, cache); each backward consumes the cache
and the upstream gradient. Correctness is guarded by finite-difference gradient checks
in tests/py/test_layers_gradcheck.py.
"""
from __future__ import annotations

import numpy as np


# --- ReLU ---

def relu_forward(x):
    return np.maximum(x, 0.0), x


def relu_backward(dout, cache):
    x = cache
    return dout * (x > 0.0)


# --- Conv2D (OC, KH, KW, IC) ---

def conv2d_forward(x, w, b, stride=1, pad=0):
    n, h, ww, ic = x.shape
    oc, kh, kw, _ = w.shape
    xp = np.pad(x, ((0, 0), (pad, pad), (pad, pad), (0, 0)))
    oh = (h + 2 * pad - kh) // stride + 1
    ow = (ww + 2 * pad - kw) // stride + 1
    out = np.zeros((n, oh, ow, oc))
    for oy in range(oh):
        for ox in range(ow):
            region = xp[:, oy * stride:oy * stride + kh, ox * stride:ox * stride + kw, :]
            out[:, oy, ox, :] = np.einsum('nklc,oklc->no', region, w)
    out += b
    return out, (x, w, b, stride, pad)


def conv2d_backward(dout, cache):
    x, w, b, stride, pad = cache
    n, h, ww, ic = x.shape
    oc, kh, kw, _ = w.shape
    xp = np.pad(x, ((0, 0), (pad, pad), (pad, pad), (0, 0)))
    dxp = np.zeros_like(xp)
    dw = np.zeros_like(w)
    db = dout.sum(axis=(0, 1, 2))
    oh, ow = dout.shape[1], dout.shape[2]
    for oy in range(oh):
        for ox in range(ow):
            region = xp[:, oy * stride:oy * stride + kh, ox * stride:ox * stride + kw, :]
            d = dout[:, oy, ox, :]                      # (N, OC)
            dw += np.einsum('no,nklc->oklc', d, region)
            dxp[:, oy * stride:oy * stride + kh, ox * stride:ox * stride + kw, :] += \
                np.einsum('no,oklc->nklc', d, w)
    dx = dxp[:, pad:pad + h, pad:pad + ww, :] if pad > 0 else dxp
    return dx, dw, db


# --- Depthwise Conv2D (KH, KW, C) ---

def depthwise_forward(x, w, b, stride=1, pad=0):
    n, h, ww, c = x.shape
    kh, kw, _ = w.shape
    xp = np.pad(x, ((0, 0), (pad, pad), (pad, pad), (0, 0)))
    oh = (h + 2 * pad - kh) // stride + 1
    ow = (ww + 2 * pad - kw) // stride + 1
    out = np.zeros((n, oh, ow, c))
    for oy in range(oh):
        for ox in range(ow):
            region = xp[:, oy * stride:oy * stride + kh, ox * stride:ox * stride + kw, :]
            out[:, oy, ox, :] = np.einsum('nklc,klc->nc', region, w)
    out += b
    return out, (x, w, b, stride, pad)


def depthwise_backward(dout, cache):
    x, w, b, stride, pad = cache
    n, h, ww, c = x.shape
    kh, kw, _ = w.shape
    xp = np.pad(x, ((0, 0), (pad, pad), (pad, pad), (0, 0)))
    dxp = np.zeros_like(xp)
    dw = np.zeros_like(w)
    db = dout.sum(axis=(0, 1, 2))
    oh, ow = dout.shape[1], dout.shape[2]
    for oy in range(oh):
        for ox in range(ow):
            region = xp[:, oy * stride:oy * stride + kh, ox * stride:ox * stride + kw, :]
            d = dout[:, oy, ox, :]                      # (N, C)
            dw += np.einsum('nc,nklc->klc', d, region)
            dxp[:, oy * stride:oy * stride + kh, ox * stride:ox * stride + kw, :] += \
                np.einsum('nc,klc->nklc', d, w)
    dx = dxp[:, pad:pad + h, pad:pad + ww, :] if pad > 0 else dxp
    return dx, dw, db


# --- Global average pool ---

def global_avgpool_forward(x):
    n, h, w, c = x.shape
    return x.mean(axis=(1, 2)), (x.shape,)


def global_avgpool_backward(dout, cache):
    (shape,) = cache
    n, h, w, c = shape
    return np.ones(shape) * dout[:, None, None, :] / (h * w)


# --- Dense (OUT, IN) ---

def dense_forward(x, w, b):
    return x @ w.T + b, (x, w, b)


def dense_backward(dout, cache):
    x, w, b = cache
    dx = dout @ w
    dw = dout.T @ x
    db = dout.sum(axis=0)
    return dx, dw, db


# --- Softmax cross-entropy ---

def softmax_xent(logits, labels):
    z = logits - logits.max(axis=1, keepdims=True)
    ez = np.exp(z)
    probs = ez / ez.sum(axis=1, keepdims=True)
    n = logits.shape[0]
    loss = -np.log(probs[np.arange(n), labels] + 1e-12).mean()
    dlogits = probs.copy()
    dlogits[np.arange(n), labels] -= 1.0
    dlogits /= n
    return loss, dlogits, probs
