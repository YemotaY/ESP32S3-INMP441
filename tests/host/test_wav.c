#include "wav.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

static void test_write_then_read(void)
{
    const char *path = "test_tmp.wav";
    const int sr = 16000;
    const uint32_t n = 800;
    int16_t *buf = (int16_t *)malloc(sizeof(int16_t) * n);
    for (uint32_t i = 0; i < n; i++) {
        buf[i] = (int16_t)(30000.0 * sin(2.0 * M_PI * 440.0 * i / sr));
    }
    CHECK_EQ_INT(wav_write_mono(path, buf, n, sr), 0);

    wav_t w;
    CHECK_EQ_INT(wav_read(path, &w), 0);
    CHECK_EQ_INT(w.sample_rate, sr);
    CHECK_EQ_INT(w.channels, 1);
    CHECK_EQ_INT((int)w.num_frames, (int)n);

    int mismatch = 0;
    for (uint32_t i = 0; i < n; i++) {
        if (w.samples[i] != buf[i]) mismatch++;
    }
    CHECK_EQ_INT(mismatch, 0);

    wav_free(&w);
    free(buf);
    remove(path);
}

static void test_read_missing_file(void)
{
    wav_t w;
    CHECK(wav_read("definitely_not_here_12345.wav", &w) != 0);
}

TEST_MAIN_BEGIN("wav")
    RUN(test_write_then_read);
    RUN(test_read_missing_file);
TEST_MAIN_END()
