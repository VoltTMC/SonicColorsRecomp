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
// Monotonic counters (always incremented; independent of --perf).
uint64_t Perf_GuestFrameIndex();   // simulation steps (FrameLoopWait)
uint64_t Perf_XfbFrameIndex();     // unique guest XFB copies
uint64_t Perf_PresentFrameIndex(); // host Present() calls (includes re-presents)
// First Present of each new XFB (excludes Renderer_RepeatLastXfb).
uint64_t Perf_UniqueFramesPresentedIndex();
// Uncapped only: cap re-present rate while sim stays at 60 Hz (0 = display refresh).
void Perf_SetHostFpsCap(int cap);
int Perf_HostFpsCap();
void Perf_OnViSpinSample(bool spinning);   // called from VIGetRetraceCount override
// FrameLoopWait post-EQ VI barriers (flush / next-retrace); skipped=true when uncapped bypasses.
void Perf_OnViPostWaitSample(bool skipped);
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
void Perf_NoteGpuNs(uint64_t gpuNs);   // from D3D11 timestamp queries (one frame late)
float Perf_GpuMsSmooth();              // 0 if no GPU timing yet
// A4a: split Present wall into blit/overlay prep vs swapchain Present().
void Perf_NotePresentSplit(uint64_t prepNs, uint64_t presentCallNs);

// Watchdog / status line: append rates when --perf is on.
void Perf_LogStatus(double secs);
