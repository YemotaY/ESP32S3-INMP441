#!/usr/bin/env python3
"""Train the DS-CNN on synthetic separable data and assert it actually learns."""
import sys

from _util import Checker  # noqa: E402
from kwslib import model as kmodel, optim, dataset  # noqa: E402


def main():
    c = Checker("py_train_learns")
    in_h, in_w, num_classes = 8, 8, 3
    X, y = dataset.make_synthetic(30, in_h, in_w, num_classes, seed=1)

    m = kmodel.DSCNN(in_h, in_w, conv_oc=6, conv_k=3, n_blocks=1,
                     pw_oc=6, num_classes=num_classes, seed=1)
    opt = optim.Adam(m.p, lr=2e-2)

    first_loss = None
    last_acc = 0.0
    for ep in range(80):
        loss, grads, acc = m.loss_and_grads(X, y)
        if first_loss is None:
            first_loss = loss
        opt.step(m.p, grads)
        last_acc = acc

    final_loss, _, final_acc = m.loss_and_grads(X, y)
    c.check(final_loss < first_loss * 0.5, f"loss dropped {first_loss:.3f}->{final_loss:.3f}")
    c.check(final_acc > 0.95, f"train acc {final_acc:.3f} > 0.95")

    Xte, yte = dataset.make_synthetic(15, in_h, in_w, num_classes, seed=555)
    test_acc = (m.predict(Xte) == yte).mean()
    c.check(test_acc > 0.9, f"test acc {test_acc:.3f} > 0.9")

    sys.exit(c.close())


if __name__ == "__main__":
    main()
