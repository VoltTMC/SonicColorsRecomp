#pragma once
#include "a3a_draw_attr.h"
#include <cstddef>
#include <cstdint>

// A3e Part Two — stable player skeletal palette capture (opt-in, no interpolation).
// SCR_A3E_PLAYER_PALETTE=1 (+ SCR_A3A_DRAW_ATTR=1 + SCR_A1B_POSE_CAPTURE=1).
// Selects one skinned draw whose identityKey stays near the A1b player body across
// consecutive XFBs, then records the draw-time PNMTXIDX XF rows consumed.

struct A3ePaletteMtx
{
    uint8_t rowIdx = 0;
    uint32_t contentHash = 0;
    float m[12]{};
};

struct A3ePlayerPaletteSnap
{
    uint64_t xfbIndex = 0;
    uint64_t simFrameId = 0;
    uint32_t playerBodyEa = 0;
    uint32_t playerShellEa = 0;
    float playerPos[3]{};
    uint32_t identityKey = 0;
    uint32_t geomSig = 0;
    uint32_t drawSeq = 0;
    uint32_t arrayBase0 = 0;
    uint16_t vtxCount = 0;
    uint8_t vat = 0;
    uint8_t prim = 0;
    uint8_t mtxCount = 0;
    uint8_t stableFrames = 0;  // consecutive XFBs with same identityKey+body
    float bestDist = 1e9f;
    A3ePaletteMtx mtx[kA3aMaxMtxPerDraw]{};
    bool valid = false;
    bool changedFromPrev = false;  // any matrix hash differed
};

bool A3e_PlayerPaletteEnabled();
void A3e_PlayerPalette_OnFrame(const A3aFrameSnap& cur, const A3aFrameSnap* prev);
bool A3e_PlayerPalette_GetCurrent(A3ePlayerPaletteSnap& out);
bool A3e_PlayerPalette_GetPrevious(A3ePlayerPaletteSnap& out);
void A3e_PlayerPalette_OverlayLine(char* buf, size_t bufSize);
