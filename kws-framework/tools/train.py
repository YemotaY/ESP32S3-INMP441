#!/usr/bin/env python3
"""Train a DS-CNN wake-word model, quantize to int8, and codegen the C header.

Phase-3 demonstration on a synthetic separable dataset (swap in real recorded features
via the same (X, y) interface). Produces artifacts/kws_model_data.h for deployment.

Usage: train.py [--epochs N] [--out PATH]
"""
import argparse
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))  # kws-framework/

from kwslib import model as kmodel, optim, quantize, codegen, infer_int8, dataset  # noqa: E402


def train(in_h=8, in_w=8, num_classes=3, epochs=60, seed=0, verbose=True):
    X, y = dataset.make_synthetic(40, in_h, in_w, num_classes, seed=seed)
    Xte, yte = dataset.make_synthetic(15, in_h, in_w, num_classes, seed=seed + 99)

    m = kmodel.DSCNN(in_h, in_w, conv_oc=6, conv_k=3, n_blocks=1,
                     pw_oc=6, num_classes=num_classes, seed=seed)
    opt = optim.Adam(m.p, lr=2e-2)

    for ep in range(epochs):
        loss, grads, acc = m.loss_and_grads(X, y)
        opt.step(m.p, grads)
        if verbose and (ep % 10 == 0 or ep == epochs - 1):
            print(f"epoch {ep:3d}  loss {loss:.4f}  train_acc {acc:.3f}")

    float_acc = (m.predict(Xte) == yte).mean()

    qm = quantize.build_quant_model(m, X, wake_class=1, threshold=0.5)
    int8_pred = np.array([
        int(infer_int8.infer(qm, Xte[i].reshape(in_h, in_w)
                             .clip(-1, 1)  # placeholder if not pre-quantized
                             .astype(np.int8)).argmax())
        for i in range(len(yte))
    ])
    return m, qm, float_acc, int8_pred, yte


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--epochs", type=int, default=60)
    ap.add_argument("--out", default=os.path.join(os.path.dirname(HERE),
                                                  "artifacts", "kws_model_data.h"))
    args = ap.parse_args()

    m, qm, float_acc, _, _ = train(epochs=args.epochs)
    os.makedirs(os.path.dirname(args.out), exist_ok=True)
    codegen.write_header(args.out, qm, symbol="g_kws_model")
    print(f"float test accuracy: {float_acc:.3f}")
    print(f"wrote model header: {args.out}")


if __name__ == "__main__":
    main()
