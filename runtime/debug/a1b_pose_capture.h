#pragma once
#include <cstddef>
#include <cstdint>

// Approach A1b — guest pose / camera-state capture (opt-in).
// SCR_A1B_POSE_CAPTURE=1 enables snapshots at sim-frame end + XF fill at XFB.
// Does not emit extra XFBs, run guest draw, or mutate guest memory.

// 3×4 affine, row-major (GameCube MTX): rows [r0 r1 r2 tx], [r3 r4 r5 ty], [r6 r7 r8 tz].
struct A1bMtx3x4
{
    float m[12];
};

// Extensible snapshot: player + camera + XF today; room for actors/bones later
// without reshaping the ring (append fields / parallel arrays in later phases).
struct A1bPoseSnap
{
    uint64_t seq = 0;            // snapshot sequence (advances once per sim capture)
    uint64_t simFrameId = 0;     // Perf_GuestFrameIndex at capture
    uint64_t xfbIndex = 0;       // Perf_XfbFrameIndex when XF filled (0 if not yet)

    bool playerValid = false;
    uint32_t playerShellEa = 0;
    uint32_t playerBodyEa = 0;
    uint32_t playerDirtyFlags = 0;  // body+0x2DD8 at capture
    A1bMtx3x4 playerWorld{};        // body+0x0C (fn_80053718 / fn_80033C78)
    float playerPos[3]{};           // body+0x168 (fn_80033C34)

    bool cameraMgrValid = false;
    uint32_t cameraMgrEa = 0;       // cCameraManager* from CameraManagerTick r3
    bool cameraNodeValid = false;
    uint32_t cameraNodeEa = 0;      // active node (see FRAME_RATE A1b)
    float cameraPos[3]{};           // node+0x90 (written by fn_80570FF0 from src+0x78)
    float cameraPosAlt[3]{};        // node+0xA8 (read by CameraManagerTick)
    A1bMtx3x4 cameraLocal{};        // node+0x60 (12 floats; layout from fn_80570FF0 copy)

    bool viewMtxValid = false;
    A1bMtx3x4 viewMtx{};            // first XF pos-matrix-0 load of the GPU frame

    bool xfValid = false;
    float xfProj[7]{};              // XF_PROJECTION (reuses A1 registers)
    float xfViewport[6]{};          // XF_VIEWPORT (not a view matrix)
    A1bMtx3x4 gxPosMtx0{};          // g_gx.xfmem[0..11] at XFB (last draw mtx 0)
    uint32_t gxMatIndexA = 0;       // CP_MATINDEX_A
};

bool A1b_PoseCaptureEnabled();
void A1b_OnSimFrameEnd();   // guest thread — Perf_OnGuestFrameEnd
void A1b_OnXfbCopy();       // GPU thread — after XF regs stable for the frame
// GPU thread: first LoadXF into pos matrix 0 (addr 0, count>=12) each XFB frame.
void A1b_OnXfPosMtx0(const float m[12]);
// Guest thread: cache player shell from another verified hook (e.g. BoostPluginTick r4).
void A1b_CachePlayerShell(uint32_t shellEa);

uint64_t A1b_SnapCount();
bool A1b_HasPair();
bool A1b_GetCurrent(A1bPoseSnap& out);
bool A1b_GetPrevious(A1bPoseSnap& out);
void A1b_OverlayLine(char* buf, size_t bufSize);
