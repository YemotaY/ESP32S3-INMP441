/* Minimal int8 neural-network kernels (NHWC), TFLite-compatible integer math.
 *
 * Weights are assumed symmetric (zero-point 0), matching TFLite's per-axis int8
 * weight quantization. Requantization uses per-output-channel (qm, shift) arrays;
 * pass length-1-style identical values for per-tensor behaviour.
 *
 * ReLU / clamping is folded into (act_min, act_max).
 */
#ifndef CORE_NN_KERNELS_H
#define CORE_NN_KERNELS_H

#include <stdint.h>
#include <stdbool.h>

/* Fully connected: output[o] = requant( bias[o] + sum_i (in[i]-in_zp)*w[o][i] ).
 * weights laid out [out_dim][in_dim]. bias may be NULL. */
void nn_fully_connected_i8(const int8_t *input, int in_dim,
                           const int8_t *weights, const int32_t *bias, int out_dim,
                           int32_t input_zp, int32_t out_mult, int out_shift,
                           int32_t output_zp, int32_t act_min, int32_t act_max,
                           int8_t *output);

/* 2D convolution, NHWC. weights [out_c][filt_h][filt_w][in_c].
 * Symmetric zero-padding with zeros (padded value == input_zp). Per-channel requant.
 * Writes out_h/out_w if non-NULL. */
void nn_conv2d_i8(const int8_t *input, int in_h, int in_w, int in_c,
                  const int8_t *weights, const int32_t *bias,
                  int out_c, int filt_h, int filt_w,
                  int stride_h, int stride_w, int pad_h, int pad_w,
                  int32_t input_zp, const int32_t *out_mult, const int *out_shift,
                  int32_t output_zp, int32_t act_min, int32_t act_max,
                  int8_t *output, int *out_h, int *out_w);

/* Depthwise 2D convolution (depth multiplier 1), NHWC.
 * weights [filt_h][filt_w][in_c]; out_c == in_c. */
void nn_depthwise_conv2d_i8(const int8_t *input, int in_h, int in_w, int in_c,
                            const int8_t *weights, const int32_t *bias,
                            int filt_h, int filt_w,
                            int stride_h, int stride_w, int pad_h, int pad_w,
                            int32_t input_zp, const int32_t *out_mult, const int *out_shift,
                            int32_t output_zp, int32_t act_min, int32_t act_max,
                            int8_t *output, int *out_h, int *out_w);

/* Global average pool over H,W -> [in_c]. Per-tensor requant. */
void nn_global_avgpool_i8(const int8_t *input, int in_h, int in_w, int in_c,
                          int32_t input_zp, int32_t out_mult, int out_shift,
                          int32_t output_zp, int32_t act_min, int32_t act_max,
                          int8_t *output);

/* Argmax over int8 vector. */
int nn_argmax_i8(const int8_t *v, int n);

/* Numerically-stable float softmax. */
void nn_softmax_f(const float *logits, int n, float *out);

#endif /* CORE_NN_KERNELS_H */
