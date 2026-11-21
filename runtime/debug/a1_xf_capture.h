#pragma once
#include <cstdint>

// Approach A1 groundwork: capture GX XF projection/viewport at each unique XFB.
// Enabled only when SCR_A1_XF_CAPTURE=1. Does not emit extra XFBs or change drawing.

bool A1_XfCaptureEnabled();
void A1_OnXfbCopy();   // call from Perf_OnXfbCopy (GPU thread)

uint64_t A1_SnapCount();
bool A1_HasPair();     // ≥2 snaps so prev/curr exist
// Copies last two snaps' projection (7 floats each: 6 + type) and viewport (6 floats).
// Returns false if fewer than 2 snaps.
bool A1_GetProjectionPair(float prevOut[7], float currOut[7]);
bool A1_ProjectionChanged();  // curr != prev (any component)
void A1_OverlayLine(char* buf, size_t bufSize);  // empty if disabled
