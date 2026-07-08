#!/usr/bin/env python3
"""Verify the Python fixed-point math reproduces the same values asserted in the C
test_quant suite (tests/host/test_quant.c). Same numbers on both sides => parity."""
import sys

from _util import Checker  # noqa: E402
from kwslib import quant  # noqa: E402


def main():
    c = Checker("py_quant")
    mbq = quant.multiply_by_quantized_multiplier

    qm, sh = quant.quantize_multiplier(1.0)
    c.check(mbq(100, qm, sh) == 100, "identity 100")
    c.check(mbq(-100, qm, sh) == -100, "identity -100")
    c.check(mbq(0, qm, sh) == 0, "identity 0")
    c.check(mbq(32767, qm, sh) == 32767, "identity 32767")

    qm, sh = quant.quantize_multiplier(0.5)
    c.check(mbq(100, qm, sh) == 50, "half 100")
    c.check(mbq(200, qm, sh) == 100, "half 200")
    c.check(mbq(-200, qm, sh) == -100, "half -200")

    qm, sh = quant.quantize_multiplier(0.25)
    c.check(mbq(400, qm, sh) == 100, "quarter 400")
    c.check(mbq(-400, qm, sh) == -100, "quarter -400")

    qm, sh = quant.quantize_multiplier(2.0)
    c.check(mbq(50, qm, sh) == 100, "gt_one 50")
    c.check(mbq(-50, qm, sh) == -100, "gt_one -50")

    rdp = quant.rounding_divide_by_pot
    c.check(rdp(8, 2) == 2, "rdp 8")
    c.check(rdp(9, 2) == 2, "rdp 9")
    c.check(rdp(10, 2) == 3, "rdp 10")
    c.check(rdp(-8, 2) == -2, "rdp -8")
    c.check(rdp(7, 0) == 7, "rdp identity")

    qm, sh = quant.quantize_multiplier(1.0)
    rq = quant.requantize_i8
    c.check(rq(200, qm, sh, 0, -128, 127) == 127, "clamp hi")
    c.check(rq(-200, qm, sh, 0, -128, 127) == -128, "clamp lo")
    c.check(rq(10, qm, sh, 5, -128, 127) == 15, "zp offset")
    c.check(rq(-10, qm, sh, 0, 0, 127) == 0, "relu clamp")

    srdh = quant.sat_round_doubling_high_mul
    c.check(srdh(quant.INT32_MIN, quant.INT32_MIN) == quant.INT32_MAX, "high mul edge")
    c.check(srdh(1 << 30, 1 << 30) == 1 << 29, "high mul 2^30")

    sys.exit(c.close())


if __name__ == "__main__":
    main()
