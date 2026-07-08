#include "core/nn/kernels.h"
#include "core/nn/quant.h"
#include "test.h"

#include <math.h>

static void identity_mult(int32_t *qm, int *shift)
{
    nn_quantize_multiplier(1.0, qm, shift);
}

static void test_fully_connected(void)
{
    int32_t qm; int shift; identity_mult(&qm, &shift);
    /* in=[10,20], w=[[1,2]], bias=[5] -> 5 + 10 + 40 = 55. */
    int8_t in[2] = {10, 20};
    int8_t w[2] = {1, 2};
    int32_t b[1] = {5};
    int8_t out[1];
    nn_fully_connected_i8(in, 2, w, b, 1, 0, qm, shift, 0, -128, 127, out);
    CHECK_EQ_INT(out[0], 55);
}

static void test_fully_connected_input_zp(void)
{
    int32_t qm; int shift; identity_mult(&qm, &shift);
    /* in=[12,22] with in_zp=2 -> effective [10,20]; same as above. */
    int8_t in[2] = {12, 22};
    int8_t w[2] = {1, 2};
    int32_t b[1] = {5};
    int8_t out[1];
    nn_fully_connected_i8(in, 2, w, b, 1, 2, qm, shift, 0, -128, 127, out);
    CHECK_EQ_INT(out[0], 55);
}

static void test_conv2d_1x1(void)
{
    int32_t qm; int shift; identity_mult(&qm, &shift);
    int32_t mult[1] = {qm}; int sh[1] = {shift};
    /* 1x1x1 input=10, 1x1 filter=2, bias=1 -> 21. */
    int8_t in[1] = {10};
    int8_t w[1] = {2};
    int32_t b[1] = {1};
    int8_t out[1];
    int oh = 0, ow = 0;
    nn_conv2d_i8(in, 1, 1, 1, w, b, 1, 1, 1, 1, 1, 0, 0, 0, mult, sh,
                 0, -128, 127, out, &oh, &ow);
    CHECK_EQ_INT(oh, 1);
    CHECK_EQ_INT(ow, 1);
    CHECK_EQ_INT(out[0], 21);
}

static void test_conv2d_valid_sum(void)
{
    int32_t qm; int shift; identity_mult(&qm, &shift);
    int32_t mult[1] = {qm}; int sh[1] = {shift};
    /* 2x2 input [[1,2],[3,4]], 2x2 all-ones filter, valid -> 1 output = 10. */
    int8_t in[4] = {1, 2, 3, 4};
    int8_t w[4] = {1, 1, 1, 1};
    int8_t out[1];
    int oh = 0, ow = 0;
    nn_conv2d_i8(in, 2, 2, 1, w, NULL, 1, 2, 2, 1, 1, 0, 0, 0, mult, sh,
                 0, -128, 127, out, &oh, &ow);
    CHECK_EQ_INT(oh, 1);
    CHECK_EQ_INT(ow, 1);
    CHECK_EQ_INT(out[0], 10);
}

static void test_conv2d_same_padding(void)
{
    int32_t qm; int shift; identity_mult(&qm, &shift);
    int32_t mult[1] = {qm}; int sh[1] = {shift};
    /* 2x2 input, 3x3 all-ones filter, pad 1, stride 1 -> 2x2 output.
     * Each output is the sum of the overlapping window (zeros outside). */
    int8_t in[4] = {1, 2, 3, 4};
    int8_t w[9] = {1,1,1, 1,1,1, 1,1,1};
    int8_t out[4];
    int oh = 0, ow = 0;
    nn_conv2d_i8(in, 2, 2, 1, w, NULL, 1, 3, 3, 1, 1, 1, 1, 0, mult, sh,
                 0, -128, 127, out, &oh, &ow);
    CHECK_EQ_INT(oh, 2);
    CHECK_EQ_INT(ow, 2);
    /* Every 3x3 window over a 2x2 image (pad1) covers all 4 pixels -> sum 10. */
    CHECK_EQ_INT(out[0], 10);
    CHECK_EQ_INT(out[1], 10);
    CHECK_EQ_INT(out[2], 10);
    CHECK_EQ_INT(out[3], 10);
}

static void test_depthwise(void)
{
    int32_t qm; int shift; identity_mult(&qm, &shift);
    int32_t mult[2] = {qm, qm}; int sh[2] = {shift, shift};
    /* 1x1x2 input=[10,20], 1x1 filter per channel=[2,3] -> [20,60]. */
    int8_t in[2] = {10, 20};
    int8_t w[2] = {2, 3};
    int8_t out[2];
    int oh = 0, ow = 0;
    nn_depthwise_conv2d_i8(in, 1, 1, 2, w, NULL, 1, 1, 1, 1, 0, 0, 0, mult, sh,
                           0, -128, 127, out, &oh, &ow);
    CHECK_EQ_INT(out[0], 20);
    CHECK_EQ_INT(out[1], 60);
}

static void test_global_avgpool(void)
{
    /* mean of [2,2,4,4] = 3. Fold 1/4 into the requant multiplier. */
    int32_t qm; int shift;
    nn_quantize_multiplier(0.25, &qm, &shift);
    int8_t in[4] = {2, 2, 4, 4};
    int8_t out[1];
    nn_global_avgpool_i8(in, 2, 2, 1, 0, qm, shift, 0, -128, 127, out);
    CHECK_EQ_INT(out[0], 3);
}

static void test_argmax(void)
{
    int8_t v[4] = {-3, 7, 2, 7};
    CHECK_EQ_INT(nn_argmax_i8(v, 4), 1); /* first max */
}

static void test_softmax(void)
{
    float logits[3] = {1.0f, 3.0f, 0.0f};
    float p[3];
    nn_softmax_f(logits, 3, p);
    /* Sums to 1, argmax is class 1, dominant prob. */
    float sum = p[0] + p[1] + p[2];
    CHECK(fabsf(sum - 1.0f) < 1e-5f);
    CHECK(p[1] > p[0] && p[1] > p[2]);
    CHECK(p[1] > 0.7f);
}

TEST_MAIN_BEGIN("kernels")
    RUN(test_fully_connected);
    RUN(test_fully_connected_input_zp);
    RUN(test_conv2d_1x1);
    RUN(test_conv2d_valid_sum);
    RUN(test_conv2d_same_padding);
    RUN(test_depthwise);
    RUN(test_global_avgpool);
    RUN(test_argmax);
    RUN(test_softmax);
TEST_MAIN_END()
