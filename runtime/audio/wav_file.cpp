#include "wav_file.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>

static uint16_t RU16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t RU32(const uint8_t* p) { return (uint32_t)(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24)); }

static float S24(const uint8_t* p)
{
    int32_t v = (int32_t)(p[0] | (p[1] << 8) | (p[2] << 16));
    if (v & 0x800000) v |= ~0xFFFFFF;
    return (float)v / 8388608.f;
}

bool Wav_Load(const std::string& path, WavData& out)
{
    out = {};
    std::ifstream f(path, std::ios::binary);
    if (!f)
    {
        out.error = "cannot open file";
        return false;
    }
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (data.size() < 44)
    {
        out.error = "file too small";
        return false;
    }
    if (std::memcmp(data.data(), "RIFF", 4) != 0 || std::memcmp(data.data() + 8, "WAVE", 4) != 0)
    {
        out.error = "not a RIFF/WAVE file";
        return false;
    }

    uint16_t audioFormat = 0, channels = 0, bits = 0;
    uint32_t sampleRate = 0, dataOff = 0, dataSize = 0;
    size_t i = 12;
    while (i + 8 <= data.size())
    {
        const char* id = (const char*)data.data() + i;
        uint32_t sz = RU32(data.data() + i + 4);
        size_t body = i + 8;
        if (body + sz > data.size()) break;
        if (std::memcmp(id, "fmt ", 4) == 0 && sz >= 16)
        {
            audioFormat = RU16(data.data() + body);
            channels = RU16(data.data() + body + 2);
            sampleRate = RU32(data.data() + body + 4);
            bits = RU16(data.data() + body + 14);
            // WAVE_FORMAT_EXTENSIBLE: real format GUID at body+24
            if (audioFormat == 0xFFFE && sz >= 40)
            {
                const uint8_t* guid = data.data() + body + 24;
                // KSDATAFORMAT_SUBTYPE_PCM / IEEE_FLOAT (first 4 bytes LE)
                uint32_t sub = RU32(guid);
                if (sub == 0x00000001) audioFormat = 1;
                else if (sub == 0x00000003) audioFormat = 3;
            }
        }
        else if (std::memcmp(id, "data", 4) == 0)
        {
            dataOff = (uint32_t)body;
            dataSize = sz;
            // Keep scanning only until we have both fmt and data (JUNK/LIST may precede fmt).
            if (sampleRate && channels && bits) break;
        }
        i = body + sz + (sz & 1);
    }
    if (!dataOff || !sampleRate || !channels || !bits)
    {
        out.error = "missing fmt/data chunk";
        return false;
    }
    if (channels < 1 || channels > 2)
    {
        out.error = "only mono/stereo supported";
        return false;
    }
    // 1 = PCM, 3 = IEEE float, 0xFFFE = extensible (treat as PCM if subtype ok — require PCM/float bits)
    if (audioFormat != 1 && audioFormat != 3 && audioFormat != 0xFFFE)
    {
        out.error = "unsupported WAV format tag";
        return false;
    }
    if (audioFormat == 3 && bits != 32)
    {
        out.error = "float WAV must be 32-bit";
        return false;
    }
    if (audioFormat != 3 && bits != 8 && bits != 16 && bits != 24 && bits != 32)
    {
        out.error = "unsupported bit depth";
        return false;
    }

    const uint8_t* p = data.data() + dataOff;
    const uint8_t* end = p + dataSize;
    const int bytesPerSamp = bits / 8;
    const int frameBytes = bytesPerSamp * channels;
    if (frameBytes <= 0 || dataSize < (uint32_t)frameBytes)
    {
        out.error = "empty audio data";
        return false;
    }
    const size_t frames = dataSize / (uint32_t)frameBytes;
    out.samples.resize(frames * 2);
    out.channels = 2;
    out.sourceChannels = (int)channels;
    out.bitsPerSample = (int)bits;
    out.sampleRate = (int)sampleRate;

    for (size_t fr = 0; fr < frames; fr++)
    {
        const uint8_t* fp = p + fr * frameBytes;
        float L = 0.f, R = 0.f;
        auto readCh = [&](int ch) -> float {
            const uint8_t* s = fp + ch * bytesPerSamp;
            if (audioFormat == 3)
            {
                float v;
                std::memcpy(&v, s, 4);
                return v;
            }
            if (bits == 8) return ((float)s[0] - 128.f) / 128.f;
            if (bits == 16)
            {
                int16_t v = (int16_t)RU16(s);
                return (float)v / 32768.f;
            }
            if (bits == 24) return S24(s);
            // 32-bit PCM
            int32_t v = (int32_t)RU32(s);
            return (float)v / 2147483648.f;
        };
        L = readCh(0);
        R = (channels == 2) ? readCh(1) : L;
        out.samples[fr * 2] = L;
        out.samples[fr * 2 + 1] = R;
        (void)end;
    }
    return true;
}

void Wav_ResampleStereo(WavData& wav, int targetRate)
{
    if (wav.sampleRate <= 0 || targetRate <= 0 || wav.sampleRate == targetRate) return;
    const size_t inFrames = wav.samples.size() / 2;
    if (inFrames == 0) return;
    const double ratio = (double)wav.sampleRate / (double)targetRate;
    const size_t outFrames = std::max<size_t>(1, (size_t)std::llround((double)inFrames / ratio));
    std::vector<float> out(outFrames * 2);
    for (size_t i = 0; i < outFrames; i++)
    {
        const double src = (double)i * ratio;
        const size_t i0 = (size_t)src;
        const size_t i1 = std::min(i0 + 1, inFrames - 1);
        const float t = (float)(src - (double)i0);
        for (int c = 0; c < 2; c++)
        {
            const float a = wav.samples[i0 * 2 + c];
            const float b = wav.samples[i1 * 2 + c];
            out[i * 2 + c] = a + (b - a) * t;
        }
    }
    wav.samples = std::move(out);
    wav.sampleRate = targetRate;
}
