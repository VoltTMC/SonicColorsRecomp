// Audio output through SDL3 audio streams (PipeWire, PulseAudio or ALSA). Same batching
// and latency limits as the XAudio2 version: 10 ms batches, a 30 ms cushion after
// running dry, batches dropped beyond 80 ms of queued audio.
#include "../../audio/audio_out.h"
#include "../../audio/music_mod.h"
#include "../../log.h"
#include "../../ui/overlay.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdlib>
#include <mutex>
#include <string>
#include <vector>

static std::mutex s_audioMutex;
static SDL_AudioDeviceID s_device = 0;
static SDL_AudioStream* s_stream = nullptr;
static uint32_t s_rate = 0;
static std::vector<int16_t> s_staging;
static constexpr uint32_t BATCH_FRAMES_PER_SEC_DIV = 100;   // 10 ms batches
static std::string s_deviceName;   // empty = system default
float g_audioVolume = 1.0f;
int g_audioPeak = 0;                           // max |sample| since last status line
int g_audioUnderruns = 0, g_audioDrops = 0;   // since last status line
uint64_t g_audioFrames = 0;                    // stereo frames produced (total)
int g_audioRepeats = 0;                        // AI blocks identical to the previous one
FILE* g_audioDump = nullptr;   // --dump-audio: raw interleaved s16 stereo (host order)
bool g_audioMute = false;      // --mute (test runs)

static constexpr uint32_t SPEAKER_RATE = 6000;
float g_speakerVolume = 0.6f;
static SDL_AudioStream* s_spkStream[4];
static std::vector<int16_t> s_spkStaging[4];

static void DestroyStreamsLocked()
{
    if (s_stream)
    {
        SDL_DestroyAudioStream(s_stream);
        s_stream = nullptr;
    }
    s_rate = 0;
    s_staging.clear();
    for (int i = 0; i < 4; i++)
    {
        if (s_spkStream[i])
        {
            SDL_DestroyAudioStream(s_spkStream[i]);
            s_spkStream[i] = nullptr;
        }
        s_spkStaging[i].clear();
    }
}

static SDL_AudioStream* MakeStreamLocked(int channels, int rate)
{
    if (!s_device) return nullptr;
    SDL_AudioSpec spec{ SDL_AUDIO_S16, channels, rate };
    SDL_AudioStream* st = SDL_CreateAudioStream(&spec, nullptr);
    if (!st || !SDL_BindAudioStream(s_device, st))
    {
        LOG_ERROR("SDL audio: cannot create a stream ({})", SDL_GetError());
        if (st) SDL_DestroyAudioStream(st);
        return nullptr;
    }
    if (g_audioMute) SDL_SetAudioStreamGain(st, 0.0f);
    return st;
}

static SDL_AudioDeviceID FindDeviceByName(const std::string& name)
{
    if (name.empty()) return SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK;
    int n = 0;
    SDL_AudioDeviceID* ids = SDL_GetAudioPlaybackDevices(&n);
    if (!ids) return 0;
    SDL_AudioDeviceID found = 0;
    for (int i = 0; i < n; i++)
    {
        const char* nm = SDL_GetAudioDeviceName(ids[i]);
        if (nm && name == nm)
        {
            found = ids[i];
            break;
        }
    }
    SDL_free(ids);
    return found;
}

static bool OpenDeviceLocked(const std::string& name)
{
    DestroyStreamsLocked();
    if (s_device)
    {
        SDL_CloseAudioDevice(s_device);
        s_device = 0;
    }
    SDL_AudioDeviceID want = FindDeviceByName(name);
    if (!want && !name.empty())
    {
        LOG_WARN("SDL audio: device '{}' not found — using default", name);
        want = SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK;
        s_deviceName.clear();
    }
    else
        s_deviceName = name;
    s_device = SDL_OpenAudioDevice(want, nullptr);
    if (!s_device)
    {
        LOG_ERROR("SDL audio: no output device ({}); audio disabled", SDL_GetError());
        return false;
    }
    const char* openName = SDL_GetAudioDeviceName(s_device);
    LOG_INFO("SDL audio device: {}", openName && openName[0] ? openName : (s_deviceName.empty() ? "(default)" : s_deviceName.c_str()));
    return true;
}

void AudioOut_ApplyVolumes(float master, float speaker)
{
    g_audioVolume = std::clamp(master, 0.0f, 1.0f);
    g_speakerVolume = std::clamp(speaker, 0.0f, 1.0f);
}

void AudioOut_Init()
{
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
    {
        LOG_ERROR("SDL audio init failed ({}); audio disabled", SDL_GetError());
        return;
    }
    std::lock_guard lk(s_audioMutex);
    if (!OpenDeviceLocked(s_deviceName)) return;
    LOG_INFO("SDL audio initialised ({}){}", SDL_GetCurrentAudioDriver() ? SDL_GetCurrentAudioDriver() : "?",
             g_audioMute ? " (muted)" : "");
}

std::vector<AudioDeviceInfo> AudioOut_ListDevices()
{
    std::vector<AudioDeviceInfo> out;
    out.push_back({ "" });   // default sentinel; UI shows "System default"
    if (!SDL_WasInit(SDL_INIT_AUDIO) && !SDL_InitSubSystem(SDL_INIT_AUDIO)) return out;
    int n = 0;
    SDL_AudioDeviceID* ids = SDL_GetAudioPlaybackDevices(&n);
    if (!ids) return out;
    for (int i = 0; i < n; i++)
    {
        const char* nm = SDL_GetAudioDeviceName(ids[i]);
        if (nm && nm[0]) out.push_back({ nm });
    }
    SDL_free(ids);
    return out;
}

const std::string& AudioOut_CurrentDeviceName() { return s_deviceName; }

bool AudioOut_SetDevice(const std::string& name)
{
    std::lock_guard lk(s_audioMutex);
    if (!SDL_WasInit(SDL_INIT_AUDIO) && !SDL_InitSubSystem(SDL_INIT_AUDIO)) return false;
    if (name == s_deviceName && s_device) return true;
    return OpenDeviceLocked(name);
}

void AudioOut_HandleEvent(const void* sdlEvent)
{
    if (!sdlEvent) return;
    const SDL_Event& e = *static_cast<const SDL_Event*>(sdlEvent);
    if (e.type != SDL_EVENT_AUDIO_DEVICE_ADDED && e.type != SDL_EVENT_AUDIO_DEVICE_REMOVED)
        return;
    if (e.adevice.recording) return;   // capture devices ignored

    std::lock_guard lk(s_audioMutex);
    if (e.type == SDL_EVENT_AUDIO_DEVICE_REMOVED)
    {
        if (!s_device || e.adevice.which != s_device) return;
        LOG_WARN("SDL audio: output device removed — reopening {}",
                 s_deviceName.empty() ? "(default)" : s_deviceName.c_str());
        // Prefer configured name; fall back to system default if that device is gone.
        bool ok = OpenDeviceLocked(s_deviceName);
        if (!ok && !s_deviceName.empty())
            ok = OpenDeviceLocked("");
        Overlay_Toast(ok ? "Audio output reconnected" : "Audio output lost");
        return;
    }
    // ADDED: if we have no device (previous remove failed), try preferred again.
    if (!s_device)
    {
        LOG_INFO("SDL audio: output device added — retrying open");
        bool ok = OpenDeviceLocked(s_deviceName);
        if (ok) Overlay_Toast("Audio output restored");
    }
}

static int Batches(SDL_AudioStream* st, uint32_t batchBytes) { return SDL_GetAudioStreamQueued(st) / (int)batchBytes; }

void AudioOut_PushDSP(const uint8_t* data, uint32_t bytes, uint32_t sampleRate)
{
    std::lock_guard lk(s_audioMutex);
    if (!s_device) return;
    if (!s_stream || s_rate != sampleRate)
    {
        if (s_stream) SDL_DestroyAudioStream(s_stream);
        s_stream = MakeStreamLocked(2, (int)sampleRate);
        s_rate = sampleRate;
    }
    if (!s_stream) return;
    uint32_t frames = bytes / 4;
    g_audioFrames += frames;
    for (uint32_t i = 0; i < frames; i++)
    {
        int16_t r = (int16_t)((data[4 * i] << 8) | data[4 * i + 1]);
        int16_t l = (int16_t)((data[4 * i + 2] << 8) | data[4 * i + 3]);
        g_audioPeak = std::max(g_audioPeak, std::max(std::abs((int)l), std::abs((int)r)));
        s_staging.push_back((int16_t)(l * g_audioVolume));
        s_staging.push_back((int16_t)(r * g_audioVolume));
        if (g_audioDump)
        {
            int16_t lr[2] = { l, r };
            fwrite(lr, 2, 2, g_audioDump);
        }
    }
    uint32_t batch = sampleRate / BATCH_FRAMES_PER_SEC_DIV;
    if (s_staging.size() / 2 < batch) return;

    int queued = Batches(s_stream, batch * 4);
    if (queued == 0)
    {
        if (s_staging.size() / 2 < batch * 3) return;
        g_audioUnderruns++;
    }
    if (queued > 8)
    {
        g_audioDrops++;
        s_staging.clear();
        return;
    }
    MusicMod_MixInto(s_staging.data(), (uint32_t)(s_staging.size() / 2), sampleRate);
    SDL_PutAudioStreamData(s_stream, s_staging.data(), (int)(s_staging.size() * sizeof(int16_t)));
    s_staging.clear();
}

void AudioOut_PushSpeaker(int chan, const int16_t* pcm, uint32_t samples)
{
    std::lock_guard lk(s_audioMutex);
    if (!s_device || chan < 0 || chan > 3) return;
    if (!s_spkStream[chan] && !(s_spkStream[chan] = MakeStreamLocked(1, SPEAKER_RATE))) return;
    if (g_audioDump)
    {
        static FILE* f = fopen("speaker_dump.raw", "wb");
        if (f)
        {
            fwrite(pcm, 2, samples, f);
            fflush(f);
        }
    }
    auto& st = s_spkStaging[chan];
    for (uint32_t i = 0; i < samples; i++) st.push_back((int16_t)(pcm[i] * g_speakerVolume * g_audioVolume));
    uint32_t batch = SPEAKER_RATE / 50;
    if (st.size() < batch) return;
    if (Batches(s_spkStream[chan], batch * 2) > 6)
    {
        st.clear();
        return;
    }
    SDL_PutAudioStreamData(s_spkStream[chan], st.data(), (int)(st.size() * sizeof(int16_t)));
    st.clear();
}
