#include "core/kws_model.h"
#include "core/nn/kernels.h"

#include <stddef.h>

bool kws_model_infer(const kws_model_t *m, const int8_t *input, int8_t *logits_out)
{
    static int8_t buf_a[KWS_MODEL_SCRATCH];
    static int8_t buf_b[KWS_MODEL_SCRATCH];

    /* conv2d + ReLU (ReLU folded via act_min == conv_zp). */
    int ch = 0, cw = 0;
    if (m->conv_oc * ((m->in_h + 2 * m->conv_pad - m->conv_kh) / m->conv_stride + 1) *
        ((m->in_w + 2 * m->conv_pad - m->conv_kw) / m->conv_stride + 1) > KWS_MODEL_SCRATCH) {
        return false;
    }
    nn_conv2d_i8(input, m->in_h, m->in_w, 1,
                 m->conv_w, m->conv_b, m->conv_oc, m->conv_kh, m->conv_kw,
                 m->conv_stride, m->conv_stride, m->conv_pad, m->conv_pad,
                 m->in_zp, m->conv_mult, m->conv_shift,
                 m->conv_zp, m->conv_zp, 127, buf_a, &ch, &cw);

    int8_t *cur = buf_a;      /* current feature map */
    int cur_h = ch, cur_w = cw, cur_c = m->conv_oc;

    if (m->n_blocks >= 1) {
        /* depthwise + ReLU into buf_b. */
        int dh = 0, dw = 0;
        if ((size_t)cur_c * ((cur_h + 2 * m->dw_pad - m->dw_k) / m->dw_stride + 1) *
            ((cur_w + 2 * m->dw_pad - m->dw_k) / m->dw_stride + 1) > KWS_MODEL_SCRATCH) {
            return false;
        }
        nn_depthwise_conv2d_i8(cur, cur_h, cur_w, cur_c,
                               m->dw_w, m->dw_b, m->dw_k, m->dw_k,
                               m->dw_stride, m->dw_stride, m->dw_pad, m->dw_pad,
                               m->conv_zp, m->dw_mult, m->dw_shift,
                               m->dw_zp, m->dw_zp, 127, buf_b, &dh, &dw);
        cur = buf_b; cur_h = dh; cur_w = dw;

        /* pointwise 1x1 conv + ReLU into buf_a. */
        int ph = 0, pw = 0;
        if ((size_t)m->pw_oc * cur_h * cur_w > KWS_MODEL_SCRATCH) {
            return false;
        }
        nn_conv2d_i8(cur, cur_h, cur_w, cur_c,
                     m->pw_w, m->pw_b, m->pw_oc, 1, 1, 1, 1, 0, 0,
                     m->dw_zp, m->pw_mult, m->pw_shift,
                     m->pw_zp, m->pw_zp, 127, buf_a, &ph, &pw);
        cur = buf_a; cur_h = ph; cur_w = pw; cur_c = m->pw_oc;
    }

    /* Global average pool -> [cur_c] into buf_b. */
    int32_t gap_in_zp = (m->n_blocks >= 1) ? m->pw_zp : m->conv_zp;
    nn_global_avgpool_i8(cur, cur_h, cur_w, cur_c,
                         gap_in_zp, m->gap_mult, m->gap_shift,
                         m->gap_zp, -128, 127, buf_b);

    /* Fully connected -> logits. */
    nn_fully_connected_i8(buf_b, cur_c, m->fc_w, m->fc_b, m->num_classes,
                          m->gap_zp, m->fc_mult, m->fc_shift,
                          m->out_zp, -128, 127, logits_out);
    return true;
}

bool kws_model_run(const kws_model_t *m, const int8_t *input, kws_result_t *result)
{
    int8_t logits[KWS_MAX_CLASSES];
    if (m->num_classes > KWS_MAX_CLASSES) {
        return false;
    }
    if (!kws_model_infer(m, input, logits)) {
        return false;
    }
    *result = kws_decide(logits, m->num_classes, m->out_scale, m->out_zp,
                         m->wake_class, m->threshold);
    return true;
}
