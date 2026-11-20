// Audio output through XAudio2.
#include "../../audio/audio_out.h"
#include "../../audio/music_mod.h"
#include "../../log.h"

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <deque>
#include <mutex>
#include <string>
#include <vector>
#include <windows.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <xaudio2.h>

static std::string s_deviceName;
static std::wstring s_deviceId;   // WASAPI endpoint id for CreateMasteringVoice
static std::mutex s_audioMu;
static std::atomic<bool> s_needRemaster{false};
static std::atomic<bool> s_suspended{false};

static IXAudio2* s_xa = nullptr;
static IXAudio2MasteringVoice* s_master = nullptr;
static IXAudio2SourceVoice* s_voice = nullptr;
static uint32_t s_rate = 0;
static std::vector<int16_t> s_staging;
static std::deque<std::vector<int16_t>> s_inFlight;   // buffers owned until played
static constexpr uint32_t BATCH_FRAMES_PER_SEC_DIV = 100;   // 10 ms batches
float g_audioVolume = 1.0f;
int g_audioPeak = 0;
int g_audioUnderruns = 0, g_audioDrops = 0;   // since last status line
uint64_t g_audioFrames = 0;
int g_audioRepeats = 0;                        // AI blocks identical to the previous one                    // stereo frames produced (total)   // max |sample| since last status line
FILE* g_audioDump = nullptr;   // --dump-audio: raw interleaved s16 stereo (host order)

static std::string Utf8FromWide(const wchar_t* w)
{
    if (!w || !w[0]) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    std::string out(n > 0 ? n - 1 : 0, '\0');
    if (n > 1) WideCharToMultiByte(CP_UTF8, 0, w, -1, out.data(), n, nullptr, nullptr);
    return out;
}

static bool LookupDeviceId(const std::string& friendlyName, std::wstring& outId)
{
    outId.clear();
    if (friendlyName.empty()) return true;
    IMMDeviceEnumerator* enumer = nullptr;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                __uuidof(IMMDeviceEnumerator), (void**)&enumer))
        || !enumer)
        return false;
    IMMDeviceCollection* coll = nullptr;
    bool found = false;
    if (SUCCEEDED(enumer->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &coll)) && coll)
    {
        UINT count = 0;
        coll->GetCount(&count);
        for (UINT i = 0; i < count; i++)
        {
            IMMDevice* dev = nullptr;
            if (FAILED(coll->Item(i, &dev)) || !dev) continue;
            IPropertyStore* props = nullptr;
            std::string name;
            if (SUCCEEDED(dev->OpenPropertyStore(STGM_READ, &props)) && props)
            {
                PROPVARIANT pv;
                PropVariantInit(&pv);
                if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &pv)) && pv.vt == VT_LPWSTR)
                    name = Utf8FromWide(pv.pwszVal);
                PropVariantClear(&pv);
                props->Release();
            }
            if (name == friendlyName)
            {
                LPWSTR id = nullptr;
                if (SUCCEEDED(dev->GetId(&id)) && id)
                {
                    outId = id;
                    CoTaskMemFree(id);
                    found = true;
                }
            }
            dev->Release();
            if (found) break;
        }
        coll->Release();
    }
    enumer->Release();
    return found;
}

static void CreateVoice(uint32_t rate)
{
    if (s_voice)
    {
        s_voice->DestroyVoice();
        s_voice = nullptr;
        s_inFlight.clear();
    }
    WAVEFORMATEX wf{};
    wf.wFormatTag = WAVE_FORMAT_PCM;
    wf.nChannels = 2;
    wf.nSamplesPerSec = rate;
    wf.wBitsPerSample = 16;
    wf.nBlockAlign = 4;
    wf.nAvgBytesPerSec = rate * 4;
    if (FAILED(s_xa->CreateSourceVoice(&s_voice, &wf)))
    {
        LOG_ERROR("XAudio2: CreateSourceVoice failed");
        s_voice = nullptr;
        return;
    }
    s_voice->Start();
    s_rate = rate;
}

static bool CreateMaster(const std::wstring& deviceId)
{
    if (s_master)
    {
        s_master->DestroyVoice();
        s_master = nullptr;
    }
    const wchar_t* id = deviceId.empty() ? nullptr : deviceId.c_str();
    if (FAILED(s_xa->CreateMasteringVoice(&s_master, XAUDIO2_DEFAULT_CHANNELS, XAUDIO2_DEFAULT_SAMPLERATE,
                                          0, id, nullptr, AudioCategory_GameEffects)))
    {
        if (id && SUCCEEDED(s_xa->CreateMasteringVoice(&s_master)))
        {
            LOG_WARN("XAudio2: falling back to default output device");
            s_deviceId.clear();
            s_deviceName.clear();
            return true;
        }
        return false;
    }
    return true;
}

bool g_audioMute = false;   // --mute (test runs)

void AudioOut_ApplyVolumes(float master, float speaker)
{
    g_audioVolume = std::clamp(master, 0.0f, 1.0f);
    g_speakerVolume = std::clamp(speaker, 0.0f, 1.0f);
}

void AudioOut_SetSuspended(bool suspended)
{
    s_suspended.store(suspended, std::memory_order_relaxed);
    std::lock_guard lk(s_audioMu);
    if (!s_master) return;
    extern bool g_audioMute;
    s_master->SetVolume((suspended || g_audioMute) ? 0.0f : 1.0f);
}

std::vector<AudioDeviceInfo> AudioOut_ListDevices()
{
    std::vector<AudioDeviceInfo> out;
    out.push_back({ "" });   // default sentinel; UI shows "System default"
    IMMDeviceEnumerator* enumer = nullptr;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                __uuidof(IMMDeviceEnumerator), (void**)&enumer))
        || !enumer)
        return out;
    IMMDeviceCollection* coll = nullptr;
    if (SUCCEEDED(enumer->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &coll)) && coll)
    {
        UINT count = 0;
        coll->GetCount(&count);
        for (UINT i = 0; i < count; i++)
        {
            IMMDevice* dev = nullptr;
            if (FAILED(coll->Item(i, &dev)) || !dev) continue;
            IPropertyStore* props = nullptr;
            if (SUCCEEDED(dev->OpenPropertyStore(STGM_READ, &props)) && props)
            {
                PROPVARIANT pv;
                PropVariantInit(&pv);
                if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &pv)) && pv.vt == VT_LPWSTR)
                {
                    std::string name = Utf8FromWide(pv.pwszVal);
                    if (!name.empty()) out.push_back({ name });
                }
                PropVariantClear(&pv);
                props->Release();
            }
            dev->Release();
        }
        coll->Release();
    }
    enumer->Release();
    return out;
}

const std::string& AudioOut_CurrentDeviceName() { return s_deviceName; }

static bool SetDeviceLocked(const std::string& name, bool force)
{
    if (!s_xa) { s_deviceName = name; return name.empty(); }
    std::wstring id;
    if (!name.empty() && !LookupDeviceId(name, id))
    {
        LOG_WARN("XAudio2: audio device not found: {}", name);
        return false;
    }
    if (!force && name == s_deviceName && id == s_deviceId && s_master) return true;
    uint32_t keepRate = s_rate;
    if (s_voice)
    {
        s_voice->DestroyVoice();
        s_voice = nullptr;
        s_inFlight.clear();
        s_staging.clear();
    }
    if (!CreateMaster(id))
    {
        LOG_ERROR("XAudio2: CreateMasteringVoice failed for selected device");
        return false;
    }
    s_deviceName = name;
    s_deviceId = std::move(id);
    extern bool g_audioMute;
    if (g_audioMute && s_master) s_master->SetVolume(0.0f);
    if (keepRate) CreateVoice(keepRate);
    LOG_INFO("XAudio2 output device: {}", name.empty() ? "(default)" : name.c_str());
    return true;
}

bool AudioOut_SetDevice(const std::string& name)
{
    std::lock_guard lk(s_audioMu);
    return SetDeviceLocked(name, false);
}

void AudioOut_HandleEvent(const void*) {}

struct AudioNotify : IMMNotificationClient
{
    LONG refs = 1;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** pp) override
    {
        if (!pp) return E_POINTER;
        if (riid == __uuidof(IUnknown) || riid == __uuidof(IMMNotificationClient))
        {
            *pp = this;
            AddRef();
            return S_OK;
        }
        *pp = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&refs); }
    ULONG STDMETHODCALLTYPE Release() override
    {
        LONG n = InterlockedDecrement(&refs);
        if (n == 0) delete this;
        return n;
    }
    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole, LPCWSTR) override
    {
        if (flow == eRender) s_needRemaster.store(true);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR) override
    {
        s_needRemaster.store(true);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR, DWORD) override
    {
        s_needRemaster.store(true);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY) override { return S_OK; }
};

static AudioNotify* s_notify = nullptr;
static IMMDeviceEnumerator* s_enumer = nullptr;

static void MaybeRemaster()
{
    if (!s_needRemaster.exchange(false)) return;
    std::lock_guard lk(s_audioMu);
    if (!s_xa) return;
    // Prefer configured device; if gone, fall back to system default.
    if (!SetDeviceLocked(s_deviceName, true) && !s_deviceName.empty())
    {
        LOG_WARN("XAudio2: preferred device unavailable — falling back to default");
        SetDeviceLocked("", true);
    }
}

void AudioOut_Init()
{
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(XAudio2Create(&s_xa, 0, XAUDIO2_DEFAULT_PROCESSOR)))
    {
        LOG_ERROR("XAudio2Create failed; audio disabled");
        return;
    }
    std::wstring id;
    if (!s_deviceName.empty()) LookupDeviceId(s_deviceName, id);
    if (!CreateMaster(id))
    {
        LOG_ERROR("XAudio2: no mastering voice; audio disabled");
        s_xa->Release();
        s_xa = nullptr;
        return;
    }
    s_deviceId = std::move(id);
    extern bool g_audioMute;
    if (g_audioMute) s_master->SetVolume(0.0f);
    if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                   __uuidof(IMMDeviceEnumerator), (void**)&s_enumer))
        && s_enumer)
    {
        s_notify = new AudioNotify();
        if (FAILED(s_enumer->RegisterEndpointNotificationCallback(s_notify)))
        {
            s_notify->Release();
            s_notify = nullptr;
        }
    }
    if (s_deviceName.empty())
        LOG_INFO("XAudio2 initialised{}", g_audioMute ? " (muted)" : "");
    else
        LOG_INFO("XAudio2 initialised{} device={}", g_audioMute ? " (muted)" : "", s_deviceName);
}

void AudioOut_PushDSP(const uint8_t* data, uint32_t bytes, uint32_t sampleRate)
{
    if (s_suspended.load(std::memory_order_relaxed)) return;
    MaybeRemaster();
    if (!s_xa) return;
    std::lock_guard lk(s_audioMu);
    if (!s_voice || s_rate != sampleRate) CreateVoice(sampleRate);
    if (!s_voice) return;
    uint32_t frames = bytes / 4;
    g_audioFrames += frames;
    // DMA data is big-endian, right channel first
    for (uint32_t i = 0; i < frames; i++)
    {
        int16_t r = (int16_t)((data[4 * i] << 8) | data[4 * i + 1]);
        int16_t l = (int16_t)((data[4 * i + 2] << 8) | data[4 * i + 3]);
        g_audioPeak = std::max(g_audioPeak, std::max(std::abs((int)l), std::abs((int)r)));
        s_staging.push_back((int16_t)(l * g_audioVolume));
        s_staging.push_back((int16_t)(r * g_audioVolume));
        if (g_audioDump) { int16_t lr[2] = { l, r }; fwrite(lr, 2, 2, g_audioDump); }
    }
    uint32_t batch = sampleRate / BATCH_FRAMES_PER_SEC_DIV;
    if (s_staging.size() / 2 < batch) return;

    XAUDIO2_VOICE_STATE st;
    s_voice->GetState(&st, XAUDIO2_VOICE_NOSAMPLESPLAYED);
    while (s_inFlight.size() > st.BuffersQueued) s_inFlight.pop_front();
    if (st.BuffersQueued == 0)
    {
        // ran dry: rebuild a 30 ms cushion before resuming so jitter is absorbed
        if (s_staging.size() / 2 < batch * 3) return;
        g_audioUnderruns++;
    }
    if (st.BuffersQueued > 8)
    {
        g_audioDrops++;
        // too much latency: drop this batch (do not advance music cursor)
        s_staging.clear();
        return;
    }
    // Mix custom BGM once per submitted batch (not on every partial append).
    MusicMod_MixInto(s_staging.data(), (uint32_t)(s_staging.size() / 2), sampleRate);
    s_inFlight.push_back(std::move(s_staging));
    s_staging.clear();
    auto& buf = s_inFlight.back();
    XAUDIO2_BUFFER xb{};
    xb.AudioBytes = (UINT32)(buf.size() * sizeof(int16_t));
    xb.pAudioData = (const BYTE*)buf.data();
    s_voice->SubmitSourceBuffer(&xb);
}

// ---------------------------------------------------------------- Wii Remote speakers
// The SDK configures the speaker for 4-bit ADPCM at 12 MHz / 2000 = 6000 Hz.
static constexpr uint32_t SPEAKER_RATE = 6000;
float g_speakerVolume = 0.6f;   // the remote speaker is quieter than the TV
static IXAudio2SourceVoice* s_spkVoice[4];
static std::vector<int16_t> s_spkStaging[4];
static std::deque<std::vector<int16_t>> s_spkInFlight[4];

void AudioOut_PushSpeaker(int chan, const int16_t* pcm, uint32_t samples)
{
    if (s_suspended.load(std::memory_order_relaxed)) return;
    MaybeRemaster();
    if (!s_xa || chan < 0 || chan > 3) return;
    std::lock_guard lk(s_audioMu);
    if (!s_spkVoice[chan])
    {
        WAVEFORMATEX wf{};
        wf.wFormatTag = WAVE_FORMAT_PCM;
        wf.nChannels = 1;
        wf.nSamplesPerSec = SPEAKER_RATE;
        wf.wBitsPerSample = 16;
        wf.nBlockAlign = 2;
        wf.nAvgBytesPerSec = SPEAKER_RATE * 2;
        if (FAILED(s_xa->CreateSourceVoice(&s_spkVoice[chan], &wf))) return;
        s_spkVoice[chan]->Start();
    }
    if (g_audioDump)
    {
        static FILE* f = fopen("speaker_dump.raw", "wb");
        if (f) { fwrite(pcm, 2, samples, f); fflush(f); }
    }
    auto& st = s_spkStaging[chan];
    for (uint32_t i = 0; i < samples; i++) st.push_back((int16_t)(pcm[i] * g_speakerVolume * g_audioVolume));
    if (st.size() < SPEAKER_RATE / 50) return;   // 20 ms batches
    XAUDIO2_VOICE_STATE vs;
    s_spkVoice[chan]->GetState(&vs, XAUDIO2_VOICE_NOSAMPLESPLAYED);
    auto& fl = s_spkInFlight[chan];
    while (fl.size() > vs.BuffersQueued) fl.pop_front();
    if (vs.BuffersQueued > 6)
    {
        st.clear();
        return;
    }
    fl.push_back(std::move(st));
    st.clear();
    XAUDIO2_BUFFER xb{};
    xb.AudioBytes = (UINT32)(fl.back().size() * sizeof(int16_t));
    xb.pAudioData = (const BYTE*)fl.back().data();
    s_spkVoice[chan]->SubmitSourceBuffer(&xb);
}
