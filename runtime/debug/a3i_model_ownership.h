#pragma once
#include "a3a_draw_attr.h"
#include "a3g_pose_aggregate.h"
#include "a3h_skeleton_palette.h"
#include <cstddef>
#include <cstdint>

// Approach A3i — live Sonic ScnMdl ownership + world/view calc provenance (opt-in).
// SCR_A3I_MODEL_OWNERSHIP=1  capture G3dProc/CalcWorld this + body→ScnMdl path
// SCR_A3I_CALC_WORLD=1       record CalcWorld / G3dProc task1 matrix outputs
// SCR_A3I_CALC_VIEW=1        record G3dProc task4/5 ViewPos materialization
// SCR_A3I_PALETTE_PROVENANCE=1  join A3h indexed-XF pals → producing ScnMdl/stage
// Disabled by default. No interpolation. Preserves A3h layouts/semantics.

// nw4r G3dProc task ids (SNCP8P ScnMdlSimple::G3dProc @ 805B8DC0).
constexpr uint32_t kA3iTaskCalcWorld = 1;
constexpr uint32_t kA3iTaskCalcMat = 2;
constexpr uint32_t kA3iTaskCalcVtx = 3;
constexpr uint32_t kA3iTaskCalcViewCb = 4;   // callback-only path in Simple
constexpr uint32_t kA3iTaskViewDraw = 5;     // builds ViewPos then draw opa
constexpr uint32_t kA3iTaskDrawXlu = 6;

constexpr int kA3iMaxCalls = 512;       // G3dProc/CalcWorld samples / XFB
constexpr int kA3iMaxModels = 64;       // distinct ScnMdl this pointers / XFB
constexpr int kA3iMaxPalLinks = 96;     // palette → model joins / XFB
constexpr int kA3iMaxPathOffs = 8;      // body/shell offsets that hold ScnMdl*
constexpr int kA3iRingSize = 4;

enum A3iStatus : uint8_t
{
    A3iStatus_Fail = 0,
    A3iStatus_Partial = 1,
    A3iStatus_Pass = 2,
    A3iStatus_Unverified = 3,
    A3iStatus_NA = 4,
};

enum A3iOwnClass : uint8_t
{
    A3iOwn_Unclassified = 0,
    A3iOwn_PlayerProven = 1,     // ScnMdl* found at verified body/shell offset
    A3iOwn_PlayerReachable = 2,  // reachable via shell walk (not a stored ptr)
    A3iOwn_PaletteMatch = 3,     // World/ViewPos base == high-hit A3h palette
    A3iOwn_Rejected = 4,
};

enum A3iCalcStage : uint8_t
{
    A3iStage_Unknown = 0,
    A3iStage_CalcWorld = 1,
    A3iStage_CalcViewCb = 2,
    A3iStage_ViewDraw = 3,       // G3dProc task5 ViewPos materialization
    A3iStage_CalcMat = 4,
    A3iStage_Other = 5,
};

enum A3iHookSrc : uint8_t
{
    A3iHook_G3dProcSimple = 0,
    A3iHook_CalcWorld = 1,
};

struct A3iOwnPath
{
    uint32_t holderEa = 0;   // body or shell+0x28 object
    uint32_t offset = 0;     // byte offset of ScnMdl* slot
    uint32_t scnMdlEa = 0;
    uint8_t holderKind = 0;  // 1=body 2=shellInner 3=shell
    uint8_t pad[3]{};
};

struct A3iCalcCall
{
    uint32_t scnMdlEa = 0;       // r3=this
    uint32_t task = 0;           // r4 for G3dProc; 1 for CalcWorld hook
    uint32_t param = 0;          // r5
    uint32_t resMdlEa = 0;       // this+0xE8
    uint32_t worldMtxEa = 0;     // this+0xEC
    uint32_t viewPosEa = 0;      // this+0xF4
    uint16_t numViewMtx = 0;     // this+0x102
    uint16_t pad0 = 0;
    uint32_t shellEa = 0;
    uint32_t bodyEa = 0;
    uint32_t instanceKey = 0;
    uint8_t hook = A3iHook_G3dProcSimple;
    uint8_t stage = A3iStage_Unknown;
    uint8_t ownClass = A3iOwn_Unclassified;
    uint8_t playerAssociated = 0;
    uint32_t pathOffset = 0;     // if proven, offset in holder
    uint8_t pathHolderKind = 0;
    uint8_t pad1[3]{};
};

struct A3iModelRec
{
    uint32_t scnMdlEa = 0;
    uint32_t resMdlEa = 0;
    uint32_t worldMtxEa = 0;
    uint32_t viewPosEa = 0;
    uint16_t numViewMtx = 0;
    uint16_t worldCalls = 0;
    uint16_t viewCalls = 0;
    uint16_t otherCalls = 0;
    uint8_t ownClass = A3iOwn_Unclassified;
    uint8_t playerAssociated = 0;
    uint8_t stableFrames = 0;    // consecutive XFBs with same classification
    uint8_t pad0 = 0;
    uint32_t pathOffset = 0;
    uint8_t pathHolderKind = 0;
    uint8_t pad1[3]{};
    uint32_t shellEa = 0;
    uint32_t bodyEa = 0;
    uint32_t instanceKey = 0;
};

struct A3iPalLink
{
    uint32_t paletteBase = 0;
    uint32_t scnMdlEa = 0;
    uint32_t guestEaSample = 0;  // first high-hit load EA
    uint32_t loadHits = 0;
    uint16_t mtxID = 0xFFFF;
    uint8_t xfRow = 0;
    uint8_t arrayKind = 0;       // 1=viewPos 2=world (matches A3h)
    uint8_t stage = A3iStage_Unknown;
    uint8_t ownClass = A3iOwn_Unclassified;
    uint8_t confidence = 0;      // 0..3
    uint8_t pad = 0;
};

struct A3iFrameSnap
{
    uint64_t simFrameId = 0;
    uint64_t xfbIndex = 0;
    uint64_t seq = 0;
    uint32_t playerShellEa = 0;
    uint32_t playerBodyEa = 0;
    uint32_t playerInstanceKey = 0;

    uint16_t callCount = 0;
    uint16_t callOverflow = 0;
    uint16_t modelCount = 0;
    uint16_t modelOverflow = 0;
    uint16_t palLinkCount = 0;
    uint16_t palUnresolved = 0;
    uint16_t worldCalls = 0;
    uint16_t viewCalls = 0;
    uint16_t playerOwnedCalls = 0;
    uint16_t unclassifiedCalls = 0;
    uint16_t attempted = 0;
    uint16_t captured = 0;
    uint16_t rejected = 0;

    uint32_t primaryScnMdlEa = 0;
    uint32_t primaryWorldEa = 0;
    uint32_t primaryViewPosEa = 0;
    uint16_t primaryNumViewMtx = 0;
    uint8_t primaryOwnClass = A3iOwn_Unclassified;
    uint8_t ownershipStatus = A3iStatus_Unverified;
    uint8_t calcWorldStatus = A3iStatus_Unverified;
    uint8_t calcViewStatus = A3iStatus_Unverified;
    uint8_t paletteStatus = A3iStatus_Unverified;
    uint8_t rootCompStatus = A3iStatus_Unverified;  // M=V·W still separate
    uint8_t pad0[2]{};

    A3iOwnPath provenPath{};
    A3iCalcCall calls[kA3iMaxCalls]{};
    A3iModelRec models[kA3iMaxModels]{};
    A3iPalLink pals[kA3iMaxPalLinks]{};
};

bool A3i_AnyEnabled();
bool A3i_NeedsPlayerIdentity();

// Guest thread: G3dProc / CalcWorld entry (before original).
void A3i_OnG3dProc(uint32_t scnMdlEa, uint32_t task, uint32_t param, uint8_t hook);
void A3i_OnCalcWorld(uint32_t scnMdlEa, uint32_t param);

// GPU/XFB: join with A3h palette map + drain ring.
void A3i_OnFrame(const A3aFrameSnap& cur, const A3gPoseSnap* pose, const A3hFrameSnap* a3h);
// When A3a is off: still drain G3dProc capture ring once per XFB.
void A3i_OnXfbCopyStandalone();

bool A3i_GetLatest(A3iFrameSnap& out);
void A3i_OverlayLine(char* buf, size_t bufSize);
