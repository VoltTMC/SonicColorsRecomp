#pragma once
#include <cstdint>

// Lightweight frame-pipeline diagnostics (--perf). Separates guest sim, VI, GX sync,
// host render/present so an in-stage 30 Hz cap can be attributed correctly.

void Perf_SetEnabled(bool on);
bool Perf_Enabled();
bool Perf_UncappedFrameWaitActive();   // true during FrameLoopWait when --frame-rate 0
// VI wait fields published each FrameLoopWait (0=uncapped, 1=guest-60, 2=native 30Hz).
extern "C" uint32_t Perf_WaitFields();
// Dt published to E19B0; same as WaitFields (1 when uncapped).
extern "C" uint32_t Perf_DtFields();
// True only when SCR_GUEST_60 / --guest-60 forces wait=1.
extern "C" bool Perf_Guest60Forced();
void Renderer_RepeatLastXfb();

void Perf_OnGuestFrameBegin();
void Perf_OnGuestFrameEnd();
// Monotonic guest FrameLoopWait count (always incremented; independent of --perf).
uint64_t Perf_GuestFrameIndex();
// When frame rate is uncapped: sleep so guest updates stay near `cap` Hz (0 = off).
void Perf_SetHostFpsCap(int cap);
int Perf_HostFpsCap();
void Perf_OnViSpinSample(bool spinning);   // called from VIGetRetraceCount override
void Perf_OnGxSyncBegin();
void Perf_OnGxSyncEnd();
void Perf_OnPresentBegin();
void Perf_OnPresentEnd();
void Perf_OnXfbCopy();                     // guest requested XFB present
void Perf_NoteXfbDraws(uint32_t draws);    // 3D draw calls since previous XFB

// Last Present() wall time in ms (includes VSync wait). Always sampled.
float Perf_LastPresentMs();
float Perf_PresentMsSmooth();
uint32_t Perf_LastXfbDraws();

// Watchdog / status line: append rates when --perf is on.
void Perf_LogStatus(double secs);
