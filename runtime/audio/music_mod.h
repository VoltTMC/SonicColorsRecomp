#pragma once
#include <cstdint>
#include <filesystem>
#include <string>

// File-based stage BGM replacement: WAV → CRI AAX → patched sonic2010_0.cpk.
// The original CRI/AX engine plays the replacement. No host MixInto / AX mute.

void MusicMod_SetDir(const std::string& dir);
void MusicMod_SetDebug(bool on); // --music-debug
void MusicMod_Init();            // prepare assets + register CPK override (before Disc_Open)
void MusicMod_Shutdown();
void MusicMod_Tick();            // no-op (kept for call sites)

// Legacy hooks retained as no-ops so older call sites compile; they must not
// drive playback or silence AX voices.
void MusicMod_OnStreamActivity(bool, int, int);
void MusicMod_OnAxVoiceStats(int, int, int);
void MusicMod_OnDiscPath(const std::string&);
// Called when the host-backed disc extent for the patched CPK is read.
void MusicMod_OnCpkHostRead(const std::string& hostPath, uint64_t fileOffset, uint32_t length, const uint8_t* data);
void MusicMod_OnStageName(const char*, const char*);
void MusicMod_OnGuestString(const char*);
void MusicMod_ForceStage(const std::string&); // ignored (overlay removed)

bool MusicMod_SilenceStreams();
bool MusicMod_SilenceVoice(bool isStream, bool looping, uint32_t loopSpanSamples = 0, int16_t curVolume = 0x7FFF);
void MusicMod_MixInto(int16_t* inout, uint32_t frames, uint32_t sampleRate);

bool MusicMod_Active();   // true if a patched CPK is registered
bool MusicMod_Debug();
const char* MusicMod_CurrentCue();
const char* MusicMod_CurrentStage();
const char* MusicMod_PatchedCpk(); // host path or empty
bool MusicMod_UsingOriginalCpk();
