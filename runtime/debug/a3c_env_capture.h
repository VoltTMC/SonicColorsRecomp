#pragma once
#include "a3a_draw_attr.h"
#include "a3b_camera_interp.h"
#include <cstddef>
#include <cstdint>

// A3c — coherent TR1 environment capture + natural camera interpolation.
// SCR_A3C_ENV_CAPTURE=1 : classify env draws, row mask, readiness stats (needs A3a).
// SCR_A3C_ENV_INTERP=1  : scratch-only natural camera interp on A3cEnv archives
//                         (implies camera path; do not use FORCE_VIEW_DX as proof).
// Disabled by default. No second XFB / no unique-FPS claim.

constexpr uint32_t kA3cMaxEnvDraws = 384;

enum A3cDrawKind : uint8_t
{
    A3cKind_Unknown = 0,
    A3cKind_Ui = 1,
    A3cKind_Env = 2,      // static / single-matrix world geometry
    A3cKind_Skinned = 3,  // multi-row PNMTXIDX (actors) — excluded from env group
    A3cKind_Other = 4,
};

enum A3cFail : uint32_t
{
    A3cFail_None = 0,
    A3cFail_Incomplete = 1u << 0,
    A3cFail_Unresolved = 1u << 1,
    A3cFail_NoEnv = 1u << 2,
    A3cFail_CorrWeak = 1u << 3,
    A3cFail_CamInvalid = 1u << 4,
    A3cFail_GeomChanged = 1u << 5,
    A3cFail_Overflow = 1u << 6,
    A3cFail_MissingPair = 1u << 7,
};

struct A3cEnvStats
{
    uint32_t totalDraws = 0;
    uint32_t envDraws = 0;
    uint32_t uiDraws = 0;
    uint32_t skinnedDraws = 0;
    uint32_t otherDraws = 0;
    uint32_t envCorrSame = 0;
    uint32_t envCorrStruct = 0;
    uint32_t envCorrAmb = 0;
    uint32_t envCorrNew = 0;
    uint32_t envRows = 0;
    uint64_t envRowMask = 0;  // PNMTXIDX rows used by env draws (bit = row)
    uint32_t passBytes = 0;
    uint32_t depCount = 0;
    uint32_t failBits = 0;
    bool captureReady = false;
    bool interpReady = false;
    bool naturalCam = false;  // view hashes differ, no FORCE_DX
};

bool A3c_EnvCaptureEnabled();
bool A3c_EnvInterpEnabled();

// Classify A3a draw into env/ui/skinned.
A3cDrawKind A3c_ClassifyDraw(const A3aDrawRec& d);

// Rebuild stats from current A3a snap + optional pass metadata.
void A3c_OnXfbCopy();
void A3c_NotePass(uint32_t passBytes, uint32_t depCount, uint32_t unresolved, uint32_t flags,
                  bool replayReady);

A3cEnvStats A3c_GetStats();
uint64_t A3c_EnvRowMask();
bool A3c_InterpReady();

void A3c_OverlayLine(char* buf, size_t bufSize);

// True when FORCE_VIEW_DX is unset and natural camera pair is interpolable + moving.
bool A3c_NaturalInterpEligible();
