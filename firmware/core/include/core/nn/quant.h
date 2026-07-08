/* Fixed-point quantization primitives, bit-compatible with TFLite / gemmlowp.
 *
 * These implement the exact integer requantization math TensorFlow Lite uses, so a
 * model quantized off-device (Phase 3) reproduces identical outputs on the ESP32-S3.
 * Keeping the math here (and unit-tested) is what makes the future C<->Python parity
 * check meaningful.
 */
#ifndef CORE_NN_QUANT_H
#define CORE_NN_QUANT_H

#include <stdint.h>

/* Saturating rounding doubling high multiply: returns the high 32 bits of
 * (a * b * 2) with rounding, saturating on the INT32_MIN*INT32_MIN overflow. */
int32_t nn_sat_round_doubling_high_mul(int32_t a, int32_t b);

/* Rounding divide by power-of-two (arithmetic), round-half-away-from-zero-ish per
 * gemmlowp. `exponent` >= 0. */
int32_t nn_rounding_divide_by_pot(int32_t x, int exponent);

/* Apply a quantized multiplier (qm, shift) to x. `shift` may be negative. */
int32_t nn_multiply_by_quantized_multiplier(int32_t x, int32_t qm, int shift);

/* Decompose a positive real multiplier (< 1 typically) into (qm in [2^30,2^31),
 * shift). Matches tflite::QuantizeMultiplier. */
void nn_quantize_multiplier(double real_multiplier, int32_t *qm, int *shift);

/* Requantize an int32 accumulator to int8: apply (qm,shift), add output_zp, clamp. */
int8_t nn_requantize_i8(int32_t acc, int32_t qm, int shift,
                        int32_t output_zp, int32_t act_min, int32_t act_max);

#endif /* CORE_NN_QUANT_H */
