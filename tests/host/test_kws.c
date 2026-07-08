#include "core/kws.h"
#include "core/nn/quant.h"
#include "test.h"

#include <math.h>

/* Hand-built int8 MLP that computes logits = relu(input) per channel:
 *   layer1 w = I (2x2), relu   -> hidden = relu(input)
 *   layer2 w = I (2x2)         -> logits = hidden
 * All zero-points 0, identity requant multipliers. Fully hand-checkable. */
static void build_identity_mlp(kws_mlp_t *m,
                               const int8_t **w1, const int8_t **w2)
{
    static const int8_t W1[4] = {1, 0, 0, 1};
    static const int8_t W2[4] = {1, 0, 0, 1};
    int32_t qm; int shift;
    nn_quantize_multiplier(1.0, &qm, &shift);

    m->in_dim = 2;
    m->hidden_dim = 2;
    m->num_classes = 2;
    m->w1 = W1; m->b1 = NULL; m->w1_mult = qm; m->w1_shift = shift;
    m->w2 = W2; m->b2 = NULL; m->w2_mult = qm; m->w2_shift = shift;
    m->in_zp = 0; m->hidden_zp = 0; m->out_zp = 0;
    *w1 = W1; *w2 = W2;
}

static void test_forward_identity(void)
{
    kws_mlp_t m; const int8_t *w1, *w2;
    build_identity_mlp(&m, &w1, &w2);

    int8_t in[2] = {5, 50};
    int8_t logits[2];
    kws_mlp_forward(&m, in, logits);
    CHECK_EQ_INT(logits[0], 5);
    CHECK_EQ_INT(logits[1], 50);
}

static void test_forward_relu_clamps_negative(void)
{
    kws_mlp_t m; const int8_t *w1, *w2;
    build_identity_mlp(&m, &w1, &w2);

    int8_t in[2] = {-5, 50};
    int8_t logits[2];
    kws_mlp_forward(&m, in, logits);
    CHECK_EQ_INT(logits[0], 0);   /* relu clamps -5 -> 0 */
    CHECK_EQ_INT(logits[1], 50);
}

static void test_decide_wake(void)
{
    int8_t logits[2] = {5, 50};
    kws_result_t r = kws_decide(logits, 2, 0.1f, 0, /*wake_class*/1, /*thr*/0.5f);
    CHECK_EQ_INT(r.class_id, 1);
    CHECK(r.is_wake);
    CHECK(r.confidence > 0.9f);
}

static void test_decide_wrong_class(void)
{
    int8_t logits[2] = {50, 5};
    kws_result_t r = kws_decide(logits, 2, 0.1f, 0, /*wake_class*/1, /*thr*/0.5f);
    CHECK_EQ_INT(r.class_id, 0);
    CHECK(!r.is_wake);
}

static void test_decide_below_threshold(void)
{
    /* Near-tie logits -> low confidence -> no wake even if argmax == wake_class. */
    int8_t logits[2] = {50, 51};
    kws_result_t r = kws_decide(logits, 2, 0.1f, 0, /*wake_class*/1, /*thr*/0.7f);
    CHECK_EQ_INT(r.class_id, 1);
    CHECK(!r.is_wake);
    CHECK(r.confidence < 0.7f);
}

static void test_end_to_end_forward_then_decide(void)
{
    kws_mlp_t m; const int8_t *w1, *w2;
    build_identity_mlp(&m, &w1, &w2);

    int8_t in[2] = {2, 90};
    int8_t logits[2];
    kws_mlp_forward(&m, in, logits);
    kws_result_t r = kws_decide(logits, 2, 0.1f, 0, 1, 0.5f);
    CHECK(r.is_wake);
    CHECK_EQ_INT(r.class_id, 1);
}

TEST_MAIN_BEGIN("kws")
    RUN(test_forward_identity);
    RUN(test_forward_relu_clamps_negative);
    RUN(test_decide_wake);
    RUN(test_decide_wrong_class);
    RUN(test_decide_below_threshold);
    RUN(test_end_to_end_forward_then_decide);
TEST_MAIN_END()
