/* In-place iterative radix-2 Cooley-Tukey FFT (float). n must be a power of two. */
#ifndef CORE_DSP_FFT_H
#define CORE_DSP_FFT_H

#include <stdbool.h>

/* Forward (inverse=false) or inverse (inverse=true) FFT over re[]/im[] of length n.
 * Inverse is scaled by 1/n. Returns false if n is not a power of two. */
bool fft_radix2(float *re, float *im, int n, bool inverse);

/* True if n is a power of two and > 0. */
bool fft_is_pow2(int n);

#endif /* CORE_DSP_FFT_H */
