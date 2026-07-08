#include "core/nn/kernels.h"
#include "core/nn/quant.h"

#include <math.h>
#include <stddef.h>

void nn_fully_connected_i8(const int8_t *input, int in_dim,
                           const int8_t *weights, const int32_t *bias, int out_dim,
                           int32_t input_zp, int32_t out_mult, int out_shift,
                           int32_t output_zp, int32_t act_min, int32_t act_max,
                           int8_t *output)
{
    for (int o = 0; o < out_dim; o++) {
        int32_t acc = bias ? bias[o] : 0;
        const int8_t *w = weights + (size_t)o * in_dim;
        for (int i = 0; i < in_dim; i++) {
            acc += (int32_t)(input[i] - input_zp) * (int32_t)w[i];
        }
        output[o] = nn_requantize_i8(acc, out_mult, out_shift,
                                     output_zp, act_min, act_max);
    }
}

void nn_conv2d_i8(const int8_t *input, int in_h, int in_w, int in_c,
                  const int8_t *weights, const int32_t *bias,
                  int out_c, int filt_h, int filt_w,
                  int stride_h, int stride_w, int pad_h, int pad_w,
                  int32_t input_zp, const int32_t *out_mult, const int *out_shift,
                  int32_t output_zp, int32_t act_min, int32_t act_max,
                  int8_t *output, int *out_h, int *out_w)
{
    int oh = (in_h + 2 * pad_h - filt_h) / stride_h + 1;
    int ow = (in_w + 2 * pad_w - filt_w) / stride_w + 1;
    if (out_h) *out_h = oh;
    if (out_w) *out_w = ow;

    for (int y = 0; y < oh; y++) {
        for (int x = 0; x < ow; x++) {
            for (int oc = 0; oc < out_c; oc++) {
                int32_t acc = bias ? bias[oc] : 0;
                for (int fy = 0; fy < filt_h; fy++) {
                    int iy = y * stride_h - pad_h + fy;
                    for (int fx = 0; fx < filt_w; fx++) {
                        int ix = x * stride_w - pad_w + fx;
                        int inside = (iy >= 0 && iy < in_h && ix >= 0 && ix < in_w);
                        for (int ic = 0; ic < in_c; ic++) {
                            int32_t in_v = inside
                                ? input[((size_t)iy * in_w + ix) * in_c + ic]
                                : input_zp; /* zero-padding == input_zp */
                            int32_t w_v = weights[(((size_t)oc * filt_h + fy) * filt_w + fx) * in_c + ic];
                            acc += (in_v - input_zp) * w_v;
                        }
                    }
                }
                output[((size_t)y * ow + x) * out_c + oc] =
                    nn_requantize_i8(acc, out_mult[oc], out_shift[oc],
                                     output_zp, act_min, act_max);
            }
        }
    }
}

void nn_depthwise_conv2d_i8(const int8_t *input, int in_h, int in_w, int in_c,
                            const int8_t *weights, const int32_t *bias,
                            int filt_h, int filt_w,
                            int stride_h, int stride_w, int pad_h, int pad_w,
                            int32_t input_zp, const int32_t *out_mult, const int *out_shift,
                            int32_t output_zp, int32_t act_min, int32_t act_max,
                            int8_t *output, int *out_h, int *out_w)
{
    int oh = (in_h + 2 * pad_h - filt_h) / stride_h + 1;
    int ow = (in_w + 2 * pad_w - filt_w) / stride_w + 1;
    if (out_h) *out_h = oh;
    if (out_w) *out_w = ow;

    for (int y = 0; y < oh; y++) {
        for (int x = 0; x < ow; x++) {
            for (int c = 0; c < in_c; c++) {
                int32_t acc = bias ? bias[c] : 0;
                for (int fy = 0; fy < filt_h; fy++) {
                    int iy = y * stride_h - pad_h + fy;
                    for (int fx = 0; fx < filt_w; fx++) {
                        int ix = x * stride_w - pad_w + fx;
                        int inside = (iy >= 0 && iy < in_h && ix >= 0 && ix < in_w);
                        int32_t in_v = inside
                            ? input[((size_t)iy * in_w + ix) * in_c + c]
                            : input_zp;
                        int32_t w_v = weights[((size_t)fy * filt_w + fx) * in_c + c];
                        acc += (in_v - input_zp) * w_v;
                    }
                }
                output[((size_t)y * ow + x) * in_c + c] =
                    nn_requantize_i8(acc, out_mult[c], out_shift[c],
                                     output_zp, act_min, act_max);
            }
        }
    }
}

void nn_global_avgpool_i8(const int8_t *input, int in_h, int in_w, int in_c,
                          int32_t input_zp, int32_t out_mult, int out_shift,
                          int32_t output_zp, int32_t act_min, int32_t act_max,
                          int8_t *output)
{
    int n = in_h * in_w;
    for (int c = 0; c < in_c; c++) {
        int32_t acc = 0;
        for (int y = 0; y < in_h; y++) {
            for (int x = 0; x < in_w; x++) {
                acc += input[((size_t)y * in_w + x) * in_c + c] - input_zp;
            }
        }
        /* Mean via a per-tensor requant multiplier that already folds in 1/n. */
        (void)n;
        output[c] = nn_requantize_i8(acc, out_mult, out_shift,
                                     output_zp, act_min, act_max);
    }
}

int nn_argmax_i8(const int8_t *v, int n)
{
    int best = 0;
    for (int i = 1; i < n; i++) {
        if (v[i] > v[best]) {
            best = i;
        }
    }
    return best;
}

void nn_softmax_f(const float *logits, int n, float *out)
{
    float m = logits[0];
    for (int i = 1; i < n; i++) {
        if (logits[i] > m) m = logits[i];
    }
    float sum = 0.0f;
    for (int i = 0; i < n; i++) {
        out[i] = expf(logits[i] - m);
        sum += out[i];
    }
    if (sum > 0.0f) {
        for (int i = 0; i < n; i++) {
            out[i] /= sum;
        }
    }
}
