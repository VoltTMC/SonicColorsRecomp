#pragma once
#include "a3a_draw_attr.h"
#include "a3i_model_ownership.h"
#include "a3j_model_palette.h"
#include <cstddef>
#include <cstdint>

// Approach A3k — persistent ownership lifecycle + body/root/world/view composition.
// SCR_A3K_OWNER_LIFETIME=1     CModelImpl create registry + reverse body→impl search
// SCR_A3K_BODY_TRANSFORM=1     body+0x0C / +0x168 samples (via BodyGetWorldMtx)
// SCR_A3K_MODEL_ROOT=1         ScnMdl+0x3C root mtx (CalcWorld passes this+0x3C)
// SCR_A3K_WORLD_VIEW_COMPOSE=1 World[i]/ViewPos[i]/V·W / root residuals
// Disabled by default. No G3dProc / ViewA PPC_OVERRIDE. No interpolation.

constexpr uint32_t kA3kScn_RootMtx = 0x3C;  // CalcWorld → WorldHier arg (this+0x3C)
constexpr uint32_t kA3kScn_ResMdl = 0xE8;
constexpr uint32_t kA3kScn_World = 0xEC;
constexpr uint32_t kA3kScn_ViewPos = 0xF4;
constexpr uint32_t kA3kScn_ViewNrm = 0xF8;
constexpr uint32_t kA3kScn_ViewEnv = 0xFC;
constexpr uint32_t kA3kScn_Byte100 = 0x100;
constexpr uint32_t kA3kScn_Byte101 = 0x101;
constexpr uint32_t kA3kScn_NumView = 0x102;
constexpr uint32_t kA3kModelImpl_ScnMdl = 0x1C;
constexpr uint32_t kA3kBody_WorldMtx = 0x0C;
constexpr uint32_t kA3kBody_Pos = 0x168;
constexpr uint32_t kA3kBody_Dirty = 0x2DD8;

constexpr int kA3kRingSize = 4;
constexpr int kA3kMaxCreates = 256;
constexpr int kA3kMaxPathHops = 8;
constexpr int kA3kMaxComposeNodes = 8;
constexpr int kA3kMaxBodySnaps = 32;

enum A3kStatus : uint8_t
{
    A3kStatus_Fail = 0,
    A3kStatus_Partial = 1,
    A3kStatus_Pass = 2,
    A3kStatus_Unverified = 3,
    A3kStatus_Rejected = 4,
};

enum A3kOwnKind : uint8_t
{
    A3kOwn_Unresolved = 0,
    A3kOwn_DirectPtr = 1,          // body/comp word == ScnMdl*
    A3kOwn_ModelImplPersistent = 2, // ModelImpl+0x1C == ScnMdl, ModelImpl reachable from body
    A3kOwn_ModelImplOrphan = 3,    // create registry matches ScnMdl; not reachable from body
    A3kOwn_CallTimeOnly = 4,       // CalcWorld this under live shell; no stored path
    A3kOwn_Scheduler = 5,          // scene/registration path (reserved)
};

enum A3kComposeRel : uint8_t
{
    A3kRel_Unknown = 0,
    A3kRel_ViewEqViewTimesWorld = 1,  // ViewPos ≈ V · World (A1b/XF V)
    A3kRel_WorldEqRoot = 2,           // World[0] ≈ ScnMdl+0x3C
    A3kRel_WorldEqRootTimesLocal = 3, // World[0] ≈ Root with small local residual
    A3kRel_BodyTEqRootT = 4,
    A3kRel_BodyPosEqRootT = 5,
    A3kRel_Rejected = 6,
    A3kRel_ViewEqEstVTimesWorld = 7,  // ViewPos ≈ V̂·World, V̂ from node0
};

struct A3kPathHop
{
    uint32_t ea = 0;
    uint16_t offset = 0;
    uint8_t kind = 0;  // 1=body 2=shellInner 3=shell 4=comp 5=modelImpl 6=scn
    uint8_t pad = 0;
};

struct A3kCreateRec
{
    uint32_t modelImplEa = 0;
    uint32_t scnMdlEa = 0;
    uint32_t lr = 0;
    uint64_t xfbIndex = 0;
    uint32_t createIndex = 0;
};

struct A3kBodySnap
{
    uint32_t bodyEa = 0;
    uint32_t dirty = 0;
    float world[12]{};
    float pos[3]{};
    uint8_t worldOk = 0;
    uint8_t posOk = 0;
};

struct A3kComposeNode
{
    uint16_t nodeIndex = 0;
    uint32_t worldEa = 0;
    uint32_t viewEa = 0;
    float world[12]{};
    float view[12]{};
    float predView[12]{};   // V · World
    float residualMax = 0;  // max |View - V·World|
    uint8_t worldOk = 0;
    uint8_t viewOk = 0;
    uint8_t predOk = 0;
    uint8_t passVw = 0;  // residual under tolerance
};

struct A3kFrameSnap
{
    uint64_t seq = 0;
    uint64_t simFrameId = 0;
    uint64_t xfbIndex = 0;

    uint32_t shellEa = 0;
    uint32_t bodyEa = 0;
    uint32_t instanceKey = 0;
    uint32_t primaryScn = 0;
    uint32_t modelImplEa = 0;
    uint32_t createLr = 0;
    uint8_t ownKind = A3kOwn_Unresolved;
    uint8_t pathHops = 0;
    uint8_t ownStatus = A3kStatus_Unverified;
    uint8_t cmodel1cStatus = A3kStatus_Unverified;  // +0x1C holds ScnMdl?
    A3kPathHop path[kA3kMaxPathHops]{};

    // Transforms
    float bodyWorld[12]{};
    float bodyPos[3]{};
    uint8_t bodyWorldOk = 0;
    uint8_t bodyPosOk = 0;
    uint32_t rootMtxEa = 0;
    float rootMtx[12]{};
    uint8_t rootOk = 0;
    uint32_t world0Ea = 0;
    float world0[12]{};
    uint8_t world0Ok = 0;
    uint32_t view0Ea = 0;
    float view0[12]{};
    uint8_t view0Ok = 0;
    float viewCam[12]{};  // A1b view / XF pos0
    uint8_t viewCamOk = 0;
    uint32_t viewPosBase = 0;
    uint32_t viewNrmBase = 0;
    uint32_t viewEnvBase = 0;
    uint16_t nView = 0;
    uint8_t byte100 = 0;
    uint8_t byte101 = 0;
    uint32_t slotBytes = 0;

    float rootWorldRes = 0;     // |World0 - Root|
    float bodyRootTRes = 0;     // |bodyWorld.T - root.T|
    float bodyPosRootTRes = 0;  // |bodyPos - root.T|
    float vwResMax = 0;         // max residual View vs A1bV·World
    float vwEstResMax = 0;      // max residual View vs V̂·World (V̂ from node0)
    uint8_t relRootWorld = A3kRel_Unknown;
    uint8_t relBodyRoot = A3kRel_Unknown;
    uint8_t relViewWorld = A3kRel_Unknown;
    uint8_t relViewEst = A3kRel_Unknown;
    uint8_t composeStatus = A3kStatus_Unverified;
    float estView[12]{};  // V̂ from ViewPos[0] · inv(World[0])
    uint8_t estViewOk = 0;

    uint16_t composeCount = 0;
    uint16_t createCount = 0;
    uint16_t bodySnapCount = 0;
    uint16_t stableFrames = 0;
    uint16_t overflow = 0;
    A3kComposeNode nodes[kA3kMaxComposeNodes]{};
};

bool A3k_AnyEnabled();
void A3k_OnCalcWorld(uint32_t scnMdlEa, uint32_t param);
void A3k_OnBodyWorldMtx(uint32_t bodyEa, const float m[12], uint32_t dirty);
void A3k_OnFrame(const A3aFrameSnap& cur, const A3jFrameSnap* a3j);
void A3k_OnXfbCopyStandalone();
bool A3k_GetLatest(A3kFrameSnap& out);
void A3k_OverlayLine(char* buf, size_t bufSize);
