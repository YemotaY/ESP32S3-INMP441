#include "core/kws.h"
#include "core/nn/kernels.h"

void kws_mlp_forward(const kws_mlp_t *m, const int8_t *input, int8_t *logits_out)
{
    int8_t hidden[KWS_MAX_HIDDEN];

    /* Layer 1: FC + ReLU (ReLU folded via act_min = hidden_zp). */
    nn_fully_connected_i8(input, m->in_dim, m->w1, m->b1, m->hidden_dim,
                          m->in_zp, m->w1_mult, m->w1_shift,
                          m->hidden_zp, m->hidden_zp, 127, hidden);

    /* Layer 2: FC -> logits (full int8 range). */
    nn_fully_connected_i8(hidden, m->hidden_dim, m->w2, m->b2, m->num_classes,
                          m->hidden_zp, m->w2_mult, m->w2_shift,
                          m->out_zp, -128, 127, logits_out);
}

kws_result_t kws_decide(const int8_t *logits, int num_classes,
                        float out_scale, int32_t out_zp,
                        int wake_class, float threshold)
{
    float deq[KWS_MAX_CLASSES];
    float prob[KWS_MAX_CLASSES];
    for (int i = 0; i < num_classes; i++) {
        deq[i] = out_scale * (float)(logits[i] - out_zp);
    }
    nn_softmax_f(deq, num_classes, prob);

    int arg = nn_argmax_i8(logits, num_classes);

    kws_result_t r;
    r.class_id = arg;
    r.confidence = prob[arg];
    r.is_wake = (arg == wake_class) && (prob[arg] >= threshold);
    return r;
}
