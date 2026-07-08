/* Custom wake-word inference (no WakeNet, no TFLM).
 *
 * Phase 2 provides a small int8 2-layer MLP forward built from the tested nn kernels,
 * plus the wake/no-wake decision layer. The DS-CNN convolutional path uses the same
 * kernels and lands with trained weights in Phase 3; the decision logic here is what
 * the firmware/server debounce relies on and is fully unit-tested.
 */
#ifndef CORE_KWS_H
#define CORE_KWS_H

#include <stdint.h>
#include <stdbool.h>

#define KWS_MAX_HIDDEN  64
#define KWS_MAX_CLASSES 8

/* Quantized 2-layer MLP: input(in_dim) -> [FC+ReLU] hidden -> [FC] logits(num_classes).
 * Weights symmetric (zp 0). Per-tensor requant multipliers precomputed via
 * nn_quantize_multiplier(). */
typedef struct {
    int in_dim;
    int hidden_dim;
    int num_classes;

    const int8_t  *w1;   /* [hidden_dim][in_dim] */
    const int32_t *b1;   /* [hidden_dim] or NULL */
    int32_t        w1_mult;
    int            w1_shift;

    const int8_t  *w2;   /* [num_classes][hidden_dim] */
    const int32_t *b2;   /* [num_classes] or NULL */
    int32_t        w2_mult;
    int            w2_shift;

    int32_t in_zp;       /* input zero-point */
    int32_t hidden_zp;   /* hidden (post-ReLU) zero-point */
    int32_t out_zp;      /* logits zero-point */
} kws_mlp_t;

/* Run the MLP; writes num_classes int8 logits. */
void kws_mlp_forward(const kws_mlp_t *m, const int8_t *input, int8_t *logits_out);

/* Decision result. */
typedef struct {
    int   class_id;
    float confidence;   /* softmax probability of class_id */
    bool  is_wake;
} kws_result_t;

/* Turn int8 logits into a wake decision.
 * Dequantizes with (out_scale, out_zp), softmaxes, takes argmax, and flags a wake
 * when argmax == wake_class and confidence >= threshold. */
kws_result_t kws_decide(const int8_t *logits, int num_classes,
                        float out_scale, int32_t out_zp,
                        int wake_class, float threshold);

#endif /* CORE_KWS_H */
