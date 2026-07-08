#include "core/nn/quant.h"

#include <math.h>

int32_t nn_sat_round_doubling_high_mul(int32_t a, int32_t b)
{
    int overflow = (a == b) && (a == INT32_MIN);
    int64_t ab = (int64_t)a * (int64_t)b;
    int32_t nudge = (ab >= 0) ? (1 << 30) : (1 - (1 << 30));
    int32_t high = (int32_t)((ab + nudge) / (1LL << 31));
    return overflow ? INT32_MAX : high;
}

int32_t nn_rounding_divide_by_pot(int32_t x, int exponent)
{
    if (exponent <= 0) {
        return x;
    }
    int32_t mask = (int32_t)((1LL << exponent) - 1);
    int32_t remainder = x & mask;
    int32_t threshold = (mask >> 1) + (x < 0 ? 1 : 0);
    return (x >> exponent) + (remainder > threshold ? 1 : 0);
}

int32_t nn_multiply_by_quantized_multiplier(int32_t x, int32_t qm, int shift)
{
    int left_shift = shift > 0 ? shift : 0;
    int right_shift = shift > 0 ? 0 : -shift;
    int32_t scaled = nn_sat_round_doubling_high_mul(x * (1 << left_shift), qm);
    return nn_rounding_divide_by_pot(scaled, right_shift);
}

void nn_quantize_multiplier(double real_multiplier, int32_t *qm, int *shift)
{
    if (real_multiplier <= 0.0) {
        *qm = 0;
        *shift = 0;
        return;
    }
    int s = 0;
    double q = frexp(real_multiplier, &s); /* q in [0.5, 1.0) */
    int64_t q_fixed = (int64_t)llround(q * (double)(1LL << 31));
    if (q_fixed == (1LL << 31)) {
        q_fixed /= 2;
        s += 1;
    }
    *qm = (int32_t)q_fixed;
    *shift = s;
}

int8_t nn_requantize_i8(int32_t acc, int32_t qm, int shift,
                        int32_t output_zp, int32_t act_min, int32_t act_max)
{
    int32_t v = nn_multiply_by_quantized_multiplier(acc, qm, shift) + output_zp;
    if (v < act_min) v = act_min;
    if (v > act_max) v = act_max;
    return (int8_t)v;
}
