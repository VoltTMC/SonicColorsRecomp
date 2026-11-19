#pragma once
#include <cstdint>

// Lightweight frame-pipeline diagnostics (--perf). Separates guest sim, VI, GX sync,
// host render/present so an in-stage 30 Hz cap can be attributed correctly.

void Perf_SetEnabled(bool on);
bool Perf_Enabled();
bool Perf_UncappedFrameWaitActive();   // true during FrameLoopWait when --frame-rate 0

void Perf_OnGuestFrameBegin();
void Perf_OnGuestFrameEnd();
void Perf_OnViSpinSample(bool spinning);   // called from VIGetRetraceCount override
void Perf_OnGxSyncBegin();
void Perf_OnGxSyncEnd();
void Perf_OnPresentBegin();
void Perf_OnPresentEnd();
void Perf_OnXfbCopy();                     // guest requested XFB present

// Watchdog / status line: append rates when --perf is on.
void Perf_LogStatus(double secs);
