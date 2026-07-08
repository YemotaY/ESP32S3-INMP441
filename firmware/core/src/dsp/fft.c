#include "core/dsp/fft.h"

#include <math.h>

bool fft_is_pow2(int n)
{
    return n > 0 && (n & (n - 1)) == 0;
}

bool fft_radix2(float *re, float *im, int n, bool inverse)
{
    if (!fft_is_pow2(n)) {
        return false;
    }
    if (n == 1) {
        return true;
    }

    /* Bit-reversal permutation. */
    for (int i = 1, j = 0; i < n; i++) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) {
            j ^= bit;
        }
        j ^= bit;
        if (i < j) {
            float tr = re[i]; re[i] = re[j]; re[j] = tr;
            float ti = im[i]; im[i] = im[j]; im[j] = ti;
        }
    }

    const float sign = inverse ? 1.0f : -1.0f;
    for (int len = 2; len <= n; len <<= 1) {
        float ang = sign * 2.0f * (float)M_PI / (float)len;
        float wr = cosf(ang);
        float wi = sinf(ang);
        for (int i = 0; i < n; i += len) {
            float cur_r = 1.0f, cur_i = 0.0f;
            for (int k = 0; k < len / 2; k++) {
                int a = i + k;
                int b = i + k + len / 2;
                float ur = re[a], ui = im[a];
                float vr = re[b] * cur_r - im[b] * cur_i;
                float vi = re[b] * cur_i + im[b] * cur_r;
                re[a] = ur + vr;
                im[a] = ui + vi;
                re[b] = ur - vr;
                im[b] = ui - vi;
                float nr = cur_r * wr - cur_i * wi;
                cur_i = cur_r * wi + cur_i * wr;
                cur_r = nr;
            }
        }
    }

    if (inverse) {
        for (int i = 0; i < n; i++) {
            re[i] /= (float)n;
            im[i] /= (float)n;
        }
    }
    return true;
}
