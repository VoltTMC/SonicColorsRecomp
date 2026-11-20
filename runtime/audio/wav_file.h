#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct WavData
{
    std::vector<float> samples;   // interleaved planar stereo (-1..1)
    int channels = 0;             // stereo after load (always 2 when OK)
    int sourceChannels = 0;       // channels in the file before upmix
    int bitsPerSample = 0;        // original bit depth
    int sampleRate = 0;
    std::string error;
};

// Load PCM / IEEE-float WAV (8/16/24/32-bit int, 32-bit float; mono or stereo).
// Converts to stereo float. sampleRate is the file's native rate (no resample yet).
bool Wav_Load(const std::string& path, WavData& out);

// Header-only probe (no sample decode). For UI status lists — do not use Wav_Load per frame.
struct WavProbe
{
    int sampleRate = 0;
    int channels = 0;
    int bitsPerSample = 0;
    uint32_t dataBytes = 0;
    double durationSec = 0;
    std::string error;
};
bool Wav_Probe(const std::string& path, WavProbe& out);

// Resample stereo float to targetRate (linear). In-place replace of samples.
void Wav_ResampleStereo(WavData& wav, int targetRate);
