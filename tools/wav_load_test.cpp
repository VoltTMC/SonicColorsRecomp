// Host-side smoke test for runtime/audio/wav_file.cpp (no game link).
#include "../runtime/audio/wav_file.h"
#include <cstdio>
#include <string>

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::fprintf(stderr, "usage: %s <file.wav> [target_rate]\n", argv[0]);
        return 2;
    }
    WavData w;
    if (!Wav_Load(argv[1], w))
    {
        std::fprintf(stderr, "FAIL: %s\n", w.error.c_str());
        return 1;
    }
    std::printf("loaded %s: %d Hz, %zu stereo frames\n", argv[1], w.sampleRate, w.samples.size() / 2);
    if (argc >= 3)
    {
        int rate = std::atoi(argv[2]);
        Wav_ResampleStereo(w, rate);
        std::printf("resampled → %d Hz, %zu frames\n", w.sampleRate, w.samples.size() / 2);
    }
    // Peak check
    float peak = 0.f;
    for (float s : w.samples) peak = std::max(peak, std::fabs(s));
    std::printf("peak=%.4f OK\n", peak);
    return 0;
}
