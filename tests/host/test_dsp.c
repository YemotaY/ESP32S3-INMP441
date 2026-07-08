#include "core/dsp/fft.h"
#include "core/dsp/melspec.h"
#include "test.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static int approx(float a, float b, float eps) { return fabsf(a - b) < eps; }

static void test_pow2(void)
{
    CHECK(fft_is_pow2(1));
    CHECK(fft_is_pow2(512));
    CHECK(!fft_is_pow2(0));
    CHECK(!fft_is_pow2(3));
    CHECK(!fft_is_pow2(100));
}

static void test_fft_impulse(void)
{
    /* FFT of a unit impulse is all ones. */
    float re[8] = {1, 0, 0, 0, 0, 0, 0, 0};
    float im[8] = {0};
    CHECK(fft_radix2(re, im, 8, false));
    for (int k = 0; k < 8; k++) {
        CHECK(approx(re[k], 1.0f, 1e-4f));
        CHECK(approx(im[k], 0.0f, 1e-4f));
    }
}

static void test_fft_dc(void)
{
    /* FFT of a constant -> energy only in bin 0 (= N). */
    float re[4] = {1, 1, 1, 1};
    float im[4] = {0, 0, 0, 0};
    CHECK(fft_radix2(re, im, 4, false));
    CHECK(approx(re[0], 4.0f, 1e-4f));
    CHECK(approx(re[1], 0.0f, 1e-4f));
    CHECK(approx(re[2], 0.0f, 1e-4f));
    CHECK(approx(re[3], 0.0f, 1e-4f));
}

static void test_fft_nyquist(void)
{
    /* [1,-1,1,-1] -> energy at Nyquist bin N/2. */
    float re[4] = {1, -1, 1, -1};
    float im[4] = {0, 0, 0, 0};
    CHECK(fft_radix2(re, im, 4, false));
    CHECK(approx(re[2], 4.0f, 1e-4f));
    CHECK(approx(re[0], 0.0f, 1e-4f));
}

static void test_fft_inverse_roundtrip(void)
{
    float re[8] = {3, 1, -2, 5, 0, 4, -1, 2};
    float im[8] = {0};
    float orig[8];
    memcpy(orig, re, sizeof orig);
    CHECK(fft_radix2(re, im, 8, false));
    CHECK(fft_radix2(re, im, 8, true));
    for (int i = 0; i < 8; i++) {
        CHECK(approx(re[i], orig[i], 1e-3f));
    }
}

static void test_fft_rejects_non_pow2(void)
{
    float re[3] = {1, 2, 3};
    float im[3] = {0};
    CHECK(!fft_radix2(re, im, 3, false));
}

static void test_mel_hz_roundtrip(void)
{
    for (float hz = 0.0f; hz < 8000.0f; hz += 500.0f) {
        float back = melspec_mel_to_hz(melspec_hz_to_mel(hz));
        CHECK(approx(back, hz, 1.0f));
    }
    CHECK(melspec_hz_to_mel(0.0f) == 0.0f);
}

static void test_melspec_init_validation(void)
{
    melspec_t m;
    melspec_cfg_t ok = { 16000, 400, 512, 40, 20.0f, 8000.0f, 0.97f };
    CHECK(melspec_init(&m, &ok));

    melspec_cfg_t bad_fft = ok; bad_fft.fft_size = 300; /* not pow2 */
    CHECK(!melspec_init(&m, &bad_fft));

    melspec_cfg_t bad_mels = ok; bad_mels.n_mels = 999;
    CHECK(!melspec_init(&m, &bad_mels));

    melspec_cfg_t bad_range = ok; bad_range.fmax_hz = 10.0f; bad_range.fmin_hz = 20.0f;
    CHECK(!melspec_init(&m, &bad_range));
}

/* A pure tone should light up the mel band containing its frequency. */
static void test_melspec_tone_peaks_expected_band(void)
{
    melspec_t m;
    melspec_cfg_t cfg = { 16000, 400, 512, 40, 20.0f, 8000.0f, 0.0f };
    CHECK(melspec_init(&m, &cfg));

    const float f0 = 1000.0f;
    float frame[400];
    for (int i = 0; i < 400; i++) {
        frame[i] = sinf(2.0f * (float)M_PI * f0 * (float)i / 16000.0f);
    }
    float feat[40];
    melspec_frame(&m, frame, feat);

    int arg = 0;
    for (int k = 1; k < 40; k++) if (feat[k] > feat[arg]) arg = k;

    /* Find which mel band nominally contains 1 kHz (center frequencies). */
    float mel_min = melspec_hz_to_mel(20.0f), mel_max = melspec_hz_to_mel(8000.0f);
    int expected = 0; float bestd = 1e30f;
    for (int mch = 0; mch < 40; mch++) {
        float mel = mel_min + (mel_max - mel_min) * (float)(mch + 1) / 41.0f;
        float hz = melspec_mel_to_hz(mel);
        float d = fabsf(hz - f0);
        if (d < bestd) { bestd = d; expected = mch; }
    }
    /* Peak should be at or adjacent to the expected band. */
    CHECK(abs(arg - expected) <= 2);
}

static void test_melspec_silence_is_floor(void)
{
    melspec_t m;
    melspec_cfg_t cfg = { 16000, 400, 512, 40, 20.0f, 8000.0f, 0.97f };
    CHECK(melspec_init(&m, &cfg));
    float frame[400];
    memset(frame, 0, sizeof frame);
    float feat[40];
    melspec_frame(&m, frame, feat);
    /* All bands hit the log(eps) floor and are equal. */
    for (int k = 1; k < 40; k++) {
        CHECK(approx(feat[k], feat[0], 1e-3f));
    }
    CHECK(feat[0] < -10.0f);
}

TEST_MAIN_BEGIN("dsp")
    RUN(test_pow2);
    RUN(test_fft_impulse);
    RUN(test_fft_dc);
    RUN(test_fft_nyquist);
    RUN(test_fft_inverse_roundtrip);
    RUN(test_fft_rejects_non_pow2);
    RUN(test_mel_hz_roundtrip);
    RUN(test_melspec_init_validation);
    RUN(test_melspec_tone_peaks_expected_band);
    RUN(test_melspec_silence_is_floor);
TEST_MAIN_END()
