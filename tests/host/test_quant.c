#include "core/nn/quant.h"
#include "test.h"

static void test_multiplier_identity(void)
{
    /* real_multiplier 1.0 must act as identity within int32. */
    int32_t qm; int shift;
    nn_quantize_multiplier(1.0, &qm, &shift);
    CHECK_EQ_INT(nn_multiply_by_quantized_multiplier(100, qm, shift), 100);
    CHECK_EQ_INT(nn_multiply_by_quantized_multiplier(-100, qm, shift), -100);
    CHECK_EQ_INT(nn_multiply_by_quantized_multiplier(0, qm, shift), 0);
    CHECK_EQ_INT(nn_multiply_by_quantized_multiplier(32767, qm, shift), 32767);
}

static void test_multiplier_half(void)
{
    int32_t qm; int shift;
    nn_quantize_multiplier(0.5, &qm, &shift);
    /* Rounding divide by 2: 100->50, 101->50 or 51 (round half up magnitude). */
    CHECK_EQ_INT(nn_multiply_by_quantized_multiplier(100, qm, shift), 50);
    CHECK_EQ_INT(nn_multiply_by_quantized_multiplier(200, qm, shift), 100);
    CHECK_EQ_INT(nn_multiply_by_quantized_multiplier(-200, qm, shift), -100);
}

static void test_multiplier_quarter(void)
{
    int32_t qm; int shift;
    nn_quantize_multiplier(0.25, &qm, &shift);
    CHECK_EQ_INT(nn_multiply_by_quantized_multiplier(400, qm, shift), 100);
    CHECK_EQ_INT(nn_multiply_by_quantized_multiplier(-400, qm, shift), -100);
}

static void test_multiplier_gt_one(void)
{
    int32_t qm; int shift;
    nn_quantize_multiplier(2.0, &qm, &shift);
    CHECK_EQ_INT(nn_multiply_by_quantized_multiplier(50, qm, shift), 100);
    CHECK_EQ_INT(nn_multiply_by_quantized_multiplier(-50, qm, shift), -100);
}

static void test_rounding_divide_by_pot(void)
{
    CHECK_EQ_INT(nn_rounding_divide_by_pot(8, 2), 2);
    CHECK_EQ_INT(nn_rounding_divide_by_pot(9, 2), 2);   /* remainder 1 <= threshold */
    CHECK_EQ_INT(nn_rounding_divide_by_pot(10, 2), 3);  /* remainder 2 > threshold  */
    CHECK_EQ_INT(nn_rounding_divide_by_pot(-8, 2), -2);
    CHECK_EQ_INT(nn_rounding_divide_by_pot(7, 0), 7);   /* exponent 0 -> identity   */
}

static void test_requantize_clamp(void)
{
    int32_t qm; int shift;
    nn_quantize_multiplier(1.0, &qm, &shift);
    /* acc 200 with zp 0 clamps to 127. */
    CHECK_EQ_INT(nn_requantize_i8(200, qm, shift, 0, -128, 127), 127);
    /* acc -200 clamps to -128. */
    CHECK_EQ_INT(nn_requantize_i8(-200, qm, shift, 0, -128, 127), -128);
    /* In range with zero-point offset. */
    CHECK_EQ_INT(nn_requantize_i8(10, qm, shift, 5, -128, 127), 15);
    /* ReLU-style clamp (act_min == zp). */
    CHECK_EQ_INT(nn_requantize_i8(-10, qm, shift, 0, 0, 127), 0);
}

static void test_high_mul_edge(void)
{
    /* SaturatingRoundingDoublingHighMul(INT32_MIN, INT32_MIN) saturates. */
    CHECK_EQ_INT(nn_sat_round_doubling_high_mul(INT32_MIN, INT32_MIN), INT32_MAX);
    /* 2^30 * 2^31-ish: high mul of x with 2^30 approx x/2. */
    CHECK_EQ_INT(nn_sat_round_doubling_high_mul(1 << 30, 1 << 30), 1 << 29);
}

TEST_MAIN_BEGIN("quant")
    RUN(test_multiplier_identity);
    RUN(test_multiplier_half);
    RUN(test_multiplier_quarter);
    RUN(test_multiplier_gt_one);
    RUN(test_rounding_divide_by_pot);
    RUN(test_requantize_clamp);
    RUN(test_high_mul_edge);
TEST_MAIN_END()
