#pragma once
#include "a3a_draw_attr.h"
#include "a3g_pose_aggregate.h"
#include "a3h_skeleton_palette.h"
#include "a3i_model_ownership.h"
#include <cstddef>
#include <cstdint>

// Approach A3j — player→ScnMdl ownership + dual palette provenance (opt-in).
// SCR_A3J_PLAYER_MODEL_OWNER=1   deep body/component → ScnMdl / CModelImpl path
// SCR_A3J_PALETTE_A_TRACE=1      ViewPos multi-buffer join (group A)
// SCR_A3J_PALETTE_B_TRACE=1      secondary array (+0xF8/+0xFC) + unresolved twin
// SCR_A3J_ROOT_COMPOSITION=1     body/world/view matrix samples vs M=V·W
// Disabled by default. No G3dProc PPC_OVERRIDE. No interpolation.

constexpr uint32_t kA3jScn_ResMdl = 0xE8;
constexpr uint32_t kA3jScn_World = 0xEC;
constexpr uint32_t kA3jScn_WorldAttr = 0xF0;
constexpr uint32_t kA3jScn_ViewPos = 0xF4;
constexpr uint32_t kA3jScn_ViewNrm = 0xF8;   // mpViewNrmMtxArray (when allocated)
constexpr uint32_t kA3jScn_ViewEnv = 0xFC;   // tertiary view-related array
constexpr uint32_t kA3jScn_Byte100 = 0x100;  // curView / related
constexpr uint32_t kA3jScn_Byte101 = 0x101;  // view-buffer index / count
constexpr uint32_t kA3jScn_NumView = 0x102;  // u16

// SonicUSA::Graphics::Model::CModelImpl* stores ScnMdl* at +0x1C (create @ 803C6D4C).
constexpr uint32_t kA3jModelImpl_ScnMdl = 0x1C;

constexpr int kA3jMaxModels = 64;
constexpr int kA3jMaxPals = 64;
constexpr int kA3jMaxViewWrites = 256;
constexpr int kA3jMaxPathHops = 8;
constexpr int kA3jRingSize = 4;
constexpr int kA3jRegistrySize = 256;

enum A3jStatus : uint8_t
{
    A3jStatus_Fail = 0,
    A3jStatus_Partial = 1,
    A3jStatus_Pass = 2,
    A3jStatus_Unverified = 3,
    A3jStatus_NA = 4,
};

enum A3jOwnClass : uint8_t
{
    A3jOwn_Unclassified = 0,
    A3jOwn_PlayerProven = 1,      // stored pointer path body/comp → ScnMdl
    A3jOwn_ModelImplProven = 2,   // CModelImpl+0x1C == ScnMdl under player graph
    A3jOwn_PaletteMatch = 3,
    A3jOwn_Associated = 4,        // call-time under live shell; no stored ptr
    A3jOwn_NonPlayer = 5,
};

enum A3jArrayKind : uint8_t
{
    A3jArr_Unknown = 0,
    A3jArr_ViewPos = 1,
    A3jArr_World = 2,
    A3jArr_ViewNrm = 3,   // this+0xF8
    A3jArr_ViewEnv = 4,   // this+0xFC
    A3jArr_ViewSlot = 5,  // ViewPos/Nrm + k*slotBytes (double-buffer)
};

struct A3jPathHop
{
    uint32_t ea = 0;
    uint16_t offset = 0;
    uint8_t kind = 0;  // 1=body 2=shellInner 3=shell 4=comp 5=modelImpl
    uint8_t pad = 0;
};

struct A3jModelRec
{
    uint32_t scnMdlEa = 0;
    uint32_t resMdlEa = 0;
    uint32_t worldEa = 0;
    uint32_t worldAttrEa = 0;
    uint32_t viewPosEa = 0;
    uint32_t viewNrmEa = 0;
    uint32_t viewEnvEa = 0;
    uint16_t numViewMtx = 0;
    uint8_t byte100 = 0;
    uint8_t byte101 = 0;
    uint32_t slotBytes = 0;  // align32(nView*0x30)
    uint8_t ownClass = A3jOwn_Unclassified;
    uint8_t pathHops = 0;
    uint8_t stableFrames = 0;
    uint8_t pad0 = 0;
    uint32_t modelImplEa = 0;  // CModelImpl* if found
    A3jPathHop path[kA3jMaxPathHops]{};
};

struct A3jViewWrite
{
    uint32_t destEa = 0;     // r3 into CalcView*
    uint32_t worldEa = 0;    // r5
    uint32_t attrEa = 0;     // r6
    uint32_t scnMdlEa = 0;   // resolved
    uint32_t lr = 0;
    uint16_t count = 0;      // r7
    uint8_t hook = 0;        // 1=A 2=B 3=C
    uint8_t arrKind = A3jArr_Unknown;
};

struct A3jPalLink
{
    uint32_t paletteBase = 0;
    uint32_t scnMdlEa = 0;
    uint32_t arrayBase = 0;  // ViewPos/Nrm/World base (slot 0)
    uint32_t loadHits = 0;
    uint16_t mtxID = 0xFFFF;
    uint8_t xfRow = 0;
    uint8_t arrKind = A3jArr_Unknown;
    uint8_t slotIndex = 0xFF;
    uint8_t ownClass = A3jOwn_Unclassified;
    uint8_t group = 0;  // 1=A 2=B 0=other
    uint8_t pad = 0;
};

struct A3jRootSample
{
    uint32_t bodyMtxEa = 0;
    uint32_t world0Ea = 0;
    uint32_t view0Ea = 0;
    uint32_t scnMdlEa = 0;
    float bodyT[3]{};
    float worldT[3]{};
    float viewT[3]{};
    uint8_t bodyOk = 0;
    uint8_t worldOk = 0;
    uint8_t viewOk = 0;
    uint8_t vwRelation = 0;  // 0=unk 1=equal_world 2=equal_view 3=neither
};

struct A3jFrameSnap
{
    uint64_t simFrameId = 0;
    uint64_t xfbIndex = 0;
    uint64_t seq = 0;
    uint32_t shellEa = 0;
    uint32_t bodyEa = 0;
    uint32_t instanceKey = 0;

    uint16_t modelCount = 0;
    uint16_t modelOverflow = 0;
    uint16_t palCount = 0;
    uint16_t palUnresolved = 0;
    uint16_t viewWriteCount = 0;
    uint16_t viewWriteOvf = 0;
    uint16_t registryCount = 0;
    uint16_t provenCount = 0;
    uint16_t groupACount = 0;
    uint16_t groupBCount = 0;
    uint16_t attempted = 0;
    uint16_t rejected = 0;

    uint32_t primaryScn = 0;
    uint32_t primaryViewPos = 0;
    uint32_t primaryViewNrm = 0;
    uint32_t primaryWorld = 0;
    uint16_t primaryNView = 0;
    uint8_t primaryOwn = A3jOwn_Unclassified;
    uint8_t ownershipStatus = A3jStatus_Unverified;
    uint8_t paletteAStatus = A3jStatus_Unverified;
    uint8_t paletteBStatus = A3jStatus_Unverified;
    uint8_t rootStatus = A3jStatus_Unverified;
    uint8_t pad0[3]{};

    A3jRootSample root{};
    A3jModelRec models[kA3jMaxModels]{};
    A3jPalLink pals[kA3jMaxPals]{};
    A3jViewWrite views[kA3jMaxViewWrites]{};
};

bool A3j_AnyEnabled();
void A3j_OnCalcWorld(uint32_t scnMdlEa, uint32_t param);
void A3j_OnFrame(const A3aFrameSnap& cur, const A3gPoseSnap* pose, const A3hFrameSnap* a3h,
                 const A3iFrameSnap* a3i);
void A3j_OnXfbCopyStandalone();
bool A3j_GetLatest(A3jFrameSnap& out);
void A3j_OverlayLine(char* buf, size_t bufSize);
