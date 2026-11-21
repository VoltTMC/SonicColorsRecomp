#pragma once
#include "a1b_pose_capture.h"
#include "a3b_camera_math.h"
#include <cstddef>
#include <cstdint>

// A3b — camera-pose interpolation + scratch-only view delta application.
// SCR_A3B_CAMERA_INTERP=1 enables (default off).
// SCR_A3B_CAMERA_ALPHA=<0..1> (default 0.5).
// Does not modify live guest XF / camera / EFB.

struct A3bCameraPose
{
    uint64_t simFrameId = 0;
    uint64_t xfbIndex = 0;
    uint64_t seq = 0;
    uint32_t cameraNodeEa = 0;
    uint32_t cameraMgrEa = 0;
    float cameraPos[3]{};
    A3bMtx3x4 view{};          // world-to-view (first XF PNMTX0 / A1b viewMtx)
    float proj[7]{};
    float viewport[6]{};
    uint32_t viewHash = 0;
    uint32_t projHash = 0;
    bool viewValid = false;
    bool projValid = false;
    bool cameraNodeValid = false;
};

struct A3bCameraPair
{
    A3bCameraPose prev{};
    A3bCameraPose curr{};
    bool havePair = false;
    uint32_t failBits = 0;  // discontinuity / invalid
    bool interpolable = false;
};

struct A3bCameraStats
{
    uint32_t applyCount = 0;
    uint32_t rowsTouched = 0;
    uint32_t xfHookCount = 0;
    float alpha = 0.5f;
    uint32_t view0Hash = 0;
    uint32_t view1Hash = 0;
    uint32_t viewAHash = 0;
    uint32_t deltaHash = 0;
    uint32_t baselineHash = 0;
    uint32_t resultHash = 0;
    uint32_t failBits = 0;
    bool exercised = false;
    bool endpointMatch = false;   // α≈0 or identical poses
    bool midpointDistinct = false;
};

bool A3b_CameraInterpEnabled();
float A3b_CameraAlpha();
// Probe override: NaN clears. Used by A2c α=0/1/0.5 tests without mutating env.
void A3b_SetProbeAlpha(float alphaOrNan);

void A3b_OnSimFrameEnd();  // pull A1b into camera ring
void A3b_OnXfbCopy();      // finalize proj/viewport; detect cuts

bool A3b_GetPair(A3bCameraPair& out);
bool A3b_PairInterpolable(const A3bCameraPair& p);

// Freeze pair for scratch probes (archive-time poses). Clear after probes.
void A3b_FreezePair(const A3bCameraPair& p);
void A3b_ClearFrozenPair();
bool A3b_HasFrozenPair();

// Scratch isolation: build delta from pair at alpha; apply to XF pos matrices.
// Left-multiply D=Vα·inv(Vbase) onto selected PNMTX rows (default: GX_PNMTX0..9).
// baseIsPrev: true → Vbase=prev (A3b FORCE_DX / archives at previous pose);
//             false → Vbase=curr (A3c natural archives captured at current pose).
bool A3b_BeginScratchCamera(float alpha, bool baseIsPrev = true);
void A3b_OnXfLoadIsolation(uint32_t addr, uint32_t count);
void A3b_EndScratchCamera();
bool A3b_ScratchCameraActive();

// Optional row filter (bit i = PNMTXIDX row i). 0 = all PNMTX0..9 slots.
void A3b_SetRowMask(uint64_t mask);
void A3b_ClearRowMask();
uint64_t A3b_GetRowMask();

void A3b_ResetCameraStats();
A3bCameraStats A3b_GetCameraStats();
void A3b_NoteCameraHashes(uint32_t baseline, uint32_t result);

void A3b_OverlayLine(char* buf, size_t bufSize);

// Suspend during primary A2f MATCH (like A3a).
void A3b_SuspendCamera(bool suspend);
bool A3b_CameraSuspended();
