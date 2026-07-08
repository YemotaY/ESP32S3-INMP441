#!/usr/bin/env python3
"""Quantize a trained model, then check:
  1. int8 predictions agree with float predictions on most held-out samples,
  2. the Python int8 reference is deterministic,
  3. codegen emits a well-formed C header."""
import sys

import numpy as np

from _util import Checker  # noqa: E402
from kwslib import model as kmodel, optim, quantize, codegen, infer_int8, dataset  # noqa: E402


def main():
    c = Checker("py_quantize_codegen")
    in_h, in_w, num_classes = 8, 8, 3
    X, y = dataset.make_synthetic(30, in_h, in_w, num_classes, seed=2)

    m = kmodel.DSCNN(in_h, in_w, conv_oc=6, conv_k=3, n_blocks=1,
                     pw_oc=6, num_classes=num_classes, seed=2)
    opt = optim.Adam(m.p, lr=2e-2)
    for _ in range(80):
        _, grads, _ = m.loss_and_grads(X, y)
        opt.step(m.p, grads)

    qm = quantize.build_quant_model(m, X, wake_class=1, threshold=0.5)

    Xte, yte = dataset.make_synthetic(20, in_h, in_w, num_classes, seed=321)
    float_pred = m.predict(Xte)
    int8_pred = []
    for i in range(len(Xte)):
        xi = quantize.quantize_input(Xte[i, :, :, 0], qm['in_scale'], qm['in_zp'])
        logits = infer_int8.infer(qm, xi)
        int8_pred.append(int(logits.argmax()))
        # determinism
        logits2 = infer_int8.infer(qm, xi)
        c.check(np.array_equal(logits, logits2), f"deterministic sample {i}")
    int8_pred = np.array(int8_pred)

    agree = (int8_pred == float_pred).mean()
    c.check(agree >= 0.7, f"int8 vs float argmax agreement {agree:.2f} >= 0.70")
    int8_acc = (int8_pred == yte).mean()
    c.check(int8_acc >= 0.7, f"int8 test accuracy {int8_acc:.2f} >= 0.70")

    header = codegen.generate(qm, symbol="g_test_model")
    for token in ("#include \"core/kws_model.h\"", "kws_model_t g_test_model",
                  "kws_model_get", "conv_w", "fc_w", "#endif"):
        c.check(token in header, f"header contains '{token}'")

    sys.exit(c.close())


if __name__ == "__main__":
    main()
