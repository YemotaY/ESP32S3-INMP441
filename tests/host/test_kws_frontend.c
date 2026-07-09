/* Wake-word front-end test: raw PCM -> log-mel -> int8 DS-CNN decision.
 *
 * Uses the committed parity model (a real quantized DS-CNN) and drives the full
 * kws_frontend pipeline with synthetic audio. Asserts the pipeline runs, respects the
 * model geometry, produces an in-range class, and that a louder tone is not rejected for
 * being too short. This exercises the same code path the firmware hook uses on-device.
 */
#include <math.h>

#include "core/kws_frontend.h"
#include "parity_model.h"
#include "test.h"

static void fill_tone(int16_t *pcm, size_t n, double freq, double amp)
{
    for (size_t i = 0; i < n; i++) {
        double v = amp * sin(2.0 * M_PI * freq * (double)i / 16000.0);
        pcm[i] = (int16_t)(v * 32767.0);
    }
}

static void test_frontend_geometry(void)
{
    kws_frontend_t fe;
    CHECK(kws_frontend_init(&fe, kws_model_get(), 16000));
    /* need = (in_h-1)*hop + frame_len. */
    size_t expect = (size_t)(kws_model_get()->in_h - 1) * KWS_FE_FRAME_STEP
                    + KWS_FE_FRAME_LEN;
    CHECK_EQ_INT((long)kws_frontend_need_samples(&fe), (long)expect);
}

static void test_frontend_runs(void)
{
    kws_frontend_t fe;
    CHECK(kws_frontend_init(&fe, kws_model_get(), 16000));

    size_t need = kws_frontend_need_samples(&fe);
    static int16_t pcm[8192];
    CHECK(need <= sizeof(pcm) / sizeof(pcm[0]));

    fill_tone(pcm, need, 440.0, 0.6);

    kws_result_t res;
    CHECK(kws_frontend_run(&fe, pcm, need, &res));
    CHECK(res.class_id >= 0 && res.class_id < kws_model_get()->num_classes);
    CHECK(res.confidence >= 0.0f && res.confidence <= 1.0001f);
}

static void test_frontend_rejects_short(void)
{
    kws_frontend_t fe;
    CHECK(kws_frontend_init(&fe, kws_model_get(), 16000));
    int16_t pcm[16] = {0};
    kws_result_t res;
    /* Fewer samples than one window: must refuse rather than read out of bounds. */
    CHECK(!kws_frontend_run(&fe, pcm, 16, &res));
}

TEST_MAIN_BEGIN("kws_frontend")
    RUN(test_frontend_geometry);
    RUN(test_frontend_runs);
    RUN(test_frontend_rejects_short);
TEST_MAIN_END()
