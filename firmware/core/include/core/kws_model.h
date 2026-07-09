/* Custom wake-word DS-CNN inference runner (no WakeNet, no TFLM).
 *
 * Fixed, small topology assembled from the tested int8 kernels:
 *
 *   input [in_h x in_w x 1] int8
 *     -> conv2d (conv_kh x conv_kw, conv_oc filters) + ReLU
 *     -> [ depthwise 3x3 + ReLU  ->  pointwise 1x1 (pw_oc) + ReLU ] x n_blocks (0 or 1)
 *     -> global average pool -> [channels]
 *     -> fully connected -> [num_classes] logits (int8)
 *
 * All layer parameters (weights, biases, per-channel requant multipliers, zero-points)
 * live in a kws_model_t, which the Phase-3 codegen emits as `kws_model_data.h`. The
 * same struct is consumed here on the device and by the host parity test, guaranteeing
 * the off-device Python reference and this engine agree bit-for-bit.
 */
#ifndef CORE_KWS_MODEL_H
#define CORE_KWS_MODEL_H

#include <stdint.h>
#include <stdbool.h>
#include "core/kws.h"

/* Scratch sizing: bump if a larger model is codegen'd. Two ping-pong int8 buffers. */
#ifndef KWS_MODEL_SCRATCH
#define KWS_MODEL_SCRATCH 8192
#endif

typedef struct {
    /* Input feature map dimensions (T frames x F mel bins x 1 channel). */
    int in_h;
    int in_w;

    /* --- conv2d layer --- */
    const int8_t  *conv_w;      /* [conv_oc][conv_kh][conv_kw][1] */
    const int32_t *conv_b;      /* [conv_oc] or NULL */
    const int32_t *conv_mult;   /* [conv_oc] */
    const int     *conv_shift;  /* [conv_oc] */
    int conv_oc, conv_kh, conv_kw, conv_stride, conv_pad;

    /* --- optional depthwise+pointwise block --- */
    int n_blocks;               /* 0 or 1 */
    const int8_t  *dw_w;        /* [dw_k][dw_k][conv_oc] */
    const int32_t *dw_b;        /* [conv_oc] or NULL */
    const int32_t *dw_mult;     /* [conv_oc] */
    const int     *dw_shift;    /* [conv_oc] */
    int dw_k, dw_stride, dw_pad;

    const int8_t  *pw_w;        /* [pw_oc][1][1][conv_oc] */
    const int32_t *pw_b;        /* [pw_oc] or NULL */
    const int32_t *pw_mult;     /* [pw_oc] */
    const int     *pw_shift;    /* [pw_oc] */
    int pw_oc;

    /* --- global average pool (per-tensor requant) --- */
    int32_t gap_mult;
    int     gap_shift;

    /* --- fully connected (per-tensor requant) --- */
    const int8_t  *fc_w;        /* [num_classes][gap_channels] */
    const int32_t *fc_b;        /* [num_classes] or NULL */
    int32_t fc_mult;
    int     fc_shift;
    int num_classes;

    /* Zero points for each stage's output. */
    int32_t in_zp, conv_zp, dw_zp, pw_zp, gap_zp, out_zp;

    /* Input quantisation scale (float feature -> int8: q = round(x/in_scale)+in_zp).
     * Emitted by codegen; used by the on-device front-end, not by kws_model_infer. */
    float in_scale;

    /* Output dequant scale + wake config for kws_decide(). */
    float out_scale;
    int   wake_class;
    float threshold;
} kws_model_t;

/* Run the model over `input` (in_h*in_w int8, row-major HxW, 1 channel).
 * Writes num_classes int8 logits to `logits_out`.
 * Returns false if intermediate sizes exceed KWS_MODEL_SCRATCH. */
bool kws_model_infer(const kws_model_t *m, const int8_t *input, int8_t *logits_out);

/* Convenience: infer + kws_decide() using the model's out_scale/wake_class/threshold. */
bool kws_model_run(const kws_model_t *m, const int8_t *input, kws_result_t *result);

#endif /* CORE_KWS_MODEL_H */
