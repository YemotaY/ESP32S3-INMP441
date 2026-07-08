#!/usr/bin/env python3
"""Guard the committed C<->Python parity fixture:
  - regenerating it is byte-identical to what is committed (deterministic),
  - the Python int8 reference reproduces the committed expected logits.
If this passes and the C test_parity passes, C and Python agree bit-for-bit."""
import os
import sys
import tempfile

import numpy as np

from _util import Checker  # noqa: E402

sys.path.insert(0, os.path.join(
    os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))),
    "kws-framework", "tools"))
import gen_parity_case as gpc  # noqa: E402
from kwslib import infer_int8  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
COMMITTED = os.path.join(ROOT, "tests", "host", "generated")


def main():
    c = Checker("py_parity_regen")

    qm, inp, expected = gpc.build()
    recomputed = infer_int8.infer(qm, inp)
    c.check(np.array_equal(recomputed, expected), "infer reproduces build() expected")

    with tempfile.TemporaryDirectory() as tmp:
        gpc.emit(qm, inp, expected, tmp)
        for fn in ("parity_model.h", "parity_case.h"):
            new = open(os.path.join(tmp, fn)).read()
            committed_path = os.path.join(COMMITTED, fn)
            c.check(os.path.exists(committed_path), f"committed {fn} exists")
            if os.path.exists(committed_path):
                old = open(committed_path).read()
                c.check(new == old, f"{fn} byte-identical to committed")

    sys.exit(c.close())


if __name__ == "__main__":
    main()
