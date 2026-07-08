"""Fixed-point quantization math, a bit-exact mirror of firmware/core/src/nn/quant.c.

Everything is computed with Python ints emulating int32/int64 C semantics so the
off-device reference reproduces the on-device integer kernels exactly. This is the
foundation of the C<->Python parity guarantee.
"""
from __future__ import annotations

import math

INT32_MIN = -(2 ** 31)
INT32_MAX = 2 ** 31 - 1


def to_int32(x: int) -> int:
    """Wrap an integer into signed 32-bit range (two's complement)."""
    x &= 0xFFFFFFFF
    return x - 0x100000000 if x & 0x80000000 else x


def _idiv_trunc(a: int, b: int) -> int:
    """Integer division truncating toward zero (C semantics)."""
    q = abs(a) // abs(b)
    return q if (a >= 0) == (b >= 0) else -q


def sat_round_doubling_high_mul(a: int, b: int) -> int:
    """Mirror of nn_sat_round_doubling_high_mul."""
    if a == b == INT32_MIN:
        return INT32_MAX
    ab = a * b
    nudge = (1 << 30) if ab >= 0 else (1 - (1 << 30))
    return _idiv_trunc(ab + nudge, 1 << 31)


def rounding_divide_by_pot(x: int, exponent: int) -> int:
    """Mirror of nn_rounding_divide_by_pot (arithmetic, gemmlowp rounding)."""
    if exponent <= 0:
        return x
    mask = (1 << exponent) - 1
    remainder = x & mask
    threshold = (mask >> 1) + (1 if x < 0 else 0)
    return (x >> exponent) + (1 if remainder > threshold else 0)


def multiply_by_quantized_multiplier(x: int, qm: int, shift: int) -> int:
    """Mirror of nn_multiply_by_quantized_multiplier."""
    left_shift = shift if shift > 0 else 0
    right_shift = 0 if shift > 0 else -shift
    scaled = sat_round_doubling_high_mul(to_int32(x * (1 << left_shift)), qm)
    return rounding_divide_by_pot(scaled, right_shift)


def quantize_multiplier(real_multiplier: float) -> tuple[int, int]:
    """Mirror of nn_quantize_multiplier -> (qm, shift)."""
    if real_multiplier <= 0.0:
        return 0, 0
    q, shift = math.frexp(real_multiplier)  # q in [0.5, 1.0)
    q_fixed = round(q * (1 << 31))
    if q_fixed == (1 << 31):
        q_fixed //= 2
        shift += 1
    return int(q_fixed), int(shift)


def requantize_i8(acc: int, qm: int, shift: int,
                  output_zp: int, act_min: int, act_max: int) -> int:
    """Mirror of nn_requantize_i8."""
    v = multiply_by_quantized_multiplier(acc, qm, shift) + output_zp
    if v < act_min:
        v = act_min
    if v > act_max:
        v = act_max
    return int(v)
