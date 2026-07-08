#include "server/vad.h"

uint32_t vad_frame_energy(const int16_t *pcm, size_t n)
{
    if (n == 0) {
        return 0;
    }
    uint64_t sum = 0;
    for (size_t i = 0; i < n; i++) {
        int32_t s = pcm[i];
        sum += (uint64_t)(s < 0 ? -s : s);
    }
    return (uint32_t)(sum / n);
}

int vad_is_speech(const vad_cfg_t *cfg, const int16_t *pcm, size_t n)
{
    return vad_frame_energy(pcm, n) > cfg->energy_thresh;
}
