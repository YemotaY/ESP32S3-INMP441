#!/usr/bin/env python3
"""Gradient checks for the DS-CNN float layers.

Weights: finite-difference vs analytic backward, so the trainer optimises the true
objective. ReLU is non-differentiable at 0, so elements whose perturbation straddles a
kink (left/right derivatives disagree) are skipped -- undefined there, not wrong.

Biases: a bias shifts a whole channel and unavoidably straddles ReLU kinks under finite
differences, so they are validated directly against the analytic identity db == sum(dout)
by exercising each layer's backward with a random upstream gradient.
"""
import sys

import numpy as np

from _util import Checker  # noqa: E402
from kwslib import model as kmodel, layers  # noqa: E402


def loss_only(m, X, y):
    logits, _ = m.forward(X)
    loss, _, _ = layers.softmax_xent(logits, y)
    return loss


def main():
    c = Checker("py_gradcheck")
    rng = np.random.default_rng(3)
    in_h, in_w, num_classes = 5, 5, 3
    m = kmodel.DSCNN(in_h, in_w, conv_oc=2, conv_k=3, n_blocks=1,
                     pw_oc=2, num_classes=num_classes, seed=7)
    N = 4
    X = rng.standard_normal((N, in_h, in_w, 1))
    y = rng.integers(0, num_classes, size=N)

    _, grads, _ = m.loss_and_grads(X, y)
    base = loss_only(m, X, y)

    eps = 1e-5
    for name in ("conv_w", "dw_w", "pw_w", "fc_w"):
        flat = m.p[name].reshape(-1)
        gflat = grads[name].reshape(-1)
        max_rel = 0.0
        checked = 0
        for i in range(flat.size):
            orig = flat[i]
            flat[i] = orig + eps
            lp = loss_only(m, X, y)
            flat[i] = orig - eps
            lm = loss_only(m, X, y)
            flat[i] = orig

            left = (base - lm) / eps
            right = (lp - base) / eps
            # Skip ReLU kinks: one-sided derivatives disagree -> not differentiable here.
            if abs(left - right) / (abs(left) + abs(right) + 1e-9) > 1e-2:
                continue
            checked += 1
            num = (lp - lm) / (2 * eps)
            ana = gflat[i]
            denom = max(1e-8, abs(num) + abs(ana))
            max_rel = max(max_rel, abs(num - ana) / denom)
        c.check(checked > 0, f"grad {name}: some elements checked")
        c.check(max_rel < 1e-4, f"grad {name}: max_rel={max_rel:.2e} over {checked} elems")

    # Bias gradients: analytic identity db == sum(dout) per layer.
    brng = np.random.default_rng(11)
    x = brng.standard_normal((3, 6, 6, 4))
    w = brng.standard_normal((5, 3, 3, 4))
    b = brng.standard_normal(5)
    _, cache = layers.conv2d_forward(x, w, b, stride=1, pad=1)
    dout = brng.standard_normal((3, 6, 6, 5))
    _, _, db = layers.conv2d_backward(dout, cache)
    c.check(np.allclose(db, dout.sum(axis=(0, 1, 2))), "conv db == sum(dout)")

    wdw = brng.standard_normal((3, 3, 4))
    bdw = brng.standard_normal(4)
    _, cache = layers.depthwise_forward(x, wdw, bdw, stride=1, pad=1)
    dout = brng.standard_normal((3, 6, 6, 4))
    _, _, db = layers.depthwise_backward(dout, cache)
    c.check(np.allclose(db, dout.sum(axis=(0, 1, 2))), "depthwise db == sum(dout)")

    xf = brng.standard_normal((3, 7))
    wf = brng.standard_normal((5, 7))
    bf = brng.standard_normal(5)
    _, cache = layers.dense_forward(xf, wf, bf)
    dout = brng.standard_normal((3, 5))
    _, _, db = layers.dense_backward(dout, cache)
    c.check(np.allclose(db, dout.sum(axis=0)), "dense db == sum(dout)")

    sys.exit(c.close())


if __name__ == "__main__":
    main()
