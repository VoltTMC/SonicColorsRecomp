#pragma once
#include <cstdint>
#include <string>
#include <vector>

void AudioOut_Init();
// Big-endian interleaved stereo s16 samples as DMA'd by the AI.
void AudioOut_PushDSP(const uint8_t* data, uint32_t bytes, uint32_t sampleRate);
// Mono s16 PCM decoded from a Wii Remote speaker stream (one voice per remote).
void AudioOut_PushSpeaker(int chan, const int16_t* pcm, uint32_t samples);

// Host volume scales (0..1). Applied to newly pushed samples; live-safe.
extern float g_audioVolume;     // master (TV / main mix)
extern float g_speakerVolume;   // relative Wii Remote speaker level (before master)
void AudioOut_ApplyVolumes(float master, float speaker);
// Silence host output while the guest is paused (focus-loss). Live-safe; does not change volumes.
void AudioOut_SetSuspended(bool suspended);

// Playback device selection (Linux/SDL3; Windows/WASAPI+XAudio2). Empty name = system default.
struct AudioDeviceInfo
{
    std::string name;   // human-readable; persisted in config
};
std::vector<AudioDeviceInfo> AudioOut_ListDevices();
const std::string& AudioOut_CurrentDeviceName();   // empty = default
// Open the named device (or default). Recreates streams; safe to call live.
bool AudioOut_SetDevice(const std::string& name);
// Linux/SDL: handle AUDIO_DEVICE_ADDED / REMOVED.
// Windows: WASAPI IMMNotificationClient remasters the default (or falls back) on
// device change; this entry stays a no-op for API symmetry.
// Pass the SDL_Event* as void* to keep this header free of SDL includes.
void AudioOut_HandleEvent(const void* sdlEvent);
