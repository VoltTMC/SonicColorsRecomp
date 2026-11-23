#pragma once
#include "a3a_draw_attr.h"
#include "a3g_pose_aggregate.h"
#include <cstddef>
#include <cstdint>

// Approach A3h — guest nw4r::g3d skeleton hierarchy + GX palette provenance (opt-in).
// SCR_A3H_SKELETON_TRACE=1   resolve ScnMdlSimple / ResNode parents for player pose
// SCR_A3H_PALETTE_MAP=1      map indexed-XF loads → guest matrix EA / mtxID
// SCR_A3H_HIERARCHY_DUMP=1   denser bounded logs
// Requires A3a (+ A3g identity). No interpolation. Off → near-zero cost.

// nw4r ScnMdlSimple member offsets (ogws / NW4R G3D Dec 2009 layout).
constexpr uint32_t kA3hScnMdl_ResMdl = 0xE8;
constexpr uint32_t kA3hScnMdl_WorldMtxArray = 0xEC;
constexpr uint32_t kA3hScnMdl_WorldMtxAttrib = 0xF0;
constexpr uint32_t kA3hScnMdl_ViewPosMtxArray = 0xF4;
constexpr uint32_t kA3hScnMdl_NumViewMtx = 0x102;  // u16

// ResNodeData (nw4r::g3d::ResNodeData)
constexpr uint32_t kA3hResNode_MtxID = 0x10;
constexpr uint32_t kA3hResNode_Flags = 0x14;
constexpr uint32_t kA3hResNode_ToParent = 0x5C;
constexpr uint32_t kA3hResNode_Size = 0xD0;

// ResMdlData
constexpr uint32_t kA3hResMdl_ToNodeDic = 0x14;
constexpr uint32_t kA3hResMdl_Info = 0x40;
constexpr uint32_t kA3hResMdlInfo_NumViewMtx = 0x1C;       // relative to info
constexpr uint32_t kA3hResMdlInfo_ToMtxIDMap = 0x24;

constexpr int kA3hMaxIdxLoads = 4096;   // indexed XF pos-mtx loads / XFB
constexpr int kA3hMaxNodes = 256;       // ResNodes captured per skeleton
constexpr int kA3hMaxPalettes = 48;     // distinct palette bases / XFB
constexpr int kA3hMaxRowMaps = 2048;    // PNMTXIDX row → guest mappings
constexpr int kA3hRingSize = 4;         // ~70KiB/snap; keep ring modest

enum A3hStatus : uint8_t
{
    A3hStatus_Fail = 0,
    A3hStatus_Partial = 1,
    A3hStatus_Pass = 2,
    A3hStatus_Unverified = 3,
    A3hStatus_NA = 4,
};

enum A3hNodeFlags : uint8_t
{
    A3hNode_None = 0,
    A3hNode_Root = 1u << 0,
    A3hNode_ParentOk = 1u << 1,
    A3hNode_InPalette = 1u << 2,
    A3hNode_MatchedObs = 1u << 3,
};

struct A3hIdxLoad
{
    uint32_t guestEa = 0;     // PhysPtr source of this mtx
    uint32_t arrayBase = 0;   // CP array base (palette)
    uint32_t contentHash = 0;
    uint16_t index = 0;       // index into palette (mtxID candidate)
    uint16_t stride = 0;
    uint16_t xfAddr = 0;      // XF word address
    uint8_t xfRow = 0;        // xfAddr / 3 for pos mtx rows (0,3,6,…)
    uint8_t pad = 0;
    uint32_t lr = 0;          // guest LR at FIFO write if available
};

struct A3hNode
{
    uint16_t nodeIdx = 0;     // ResDic index
    int16_t parentIdx = -1;   // ResDic index of parent; -1 = root/none
    uint16_t mtxID = 0xFFFF;  // ResNode.mtxID
    uint8_t flags = 0;
    uint8_t pad = 0;
    uint32_t resNodeEa = 0;   // guest EA of ResNodeData
    uint32_t nameOff = 0;     // relative name ofs (0 = none)
};

struct A3hRowMap
{
    uint8_t xfRow = 0;
    uint8_t stage = 0;        // 0=unknown 1=viewPos 2=world 3=directLoad
    uint16_t mtxID = 0xFFFF;
    int16_t nodeIdx = -1;
    uint16_t pad = 0;
    uint32_t guestEa = 0;
    uint32_t contentHash = 0;
    uint32_t drawSeq = 0;
    uint32_t obsKey = 0;
    uint32_t paletteBase = 0;
};

struct A3hPalette
{
    uint32_t baseEa = 0;
    uint32_t scnMdlEa = 0;    // ScnMdlSimple* if resolved
    uint32_t resMdlEa = 0;
    uint16_t numViewMtx = 0;
    uint16_t nodeCount = 0;
    uint16_t parentResolved = 0;
    uint16_t rootCount = 0;
    uint8_t arrayKind = 0;    // 1=viewPos 2=world
    uint8_t hierarchyOk = 0;  // parents from ResNode.toParentNode
    uint8_t pad0 = 0;
    uint8_t pad1 = 0;
    uint32_t loadHits = 0;
};

struct A3hFrameSnap
{
    uint64_t simFrameId = 0;
    uint64_t xfbIndex = 0;
    uint64_t seq = 0;
    uint32_t playerInstanceKey = 0;
    uint32_t playerShellEa = 0;
    uint32_t playerBodyEa = 0;

    uint16_t idxLoadCount = 0;
    uint16_t idxLoadOverflow = 0;
    uint16_t paletteCount = 0;
    uint16_t nodeCount = 0;       // sum across palettes (capped store)
    uint16_t rowMapCount = 0;
    uint16_t rowMapOverflow = 0;
    uint16_t mappedRows = 0;
    uint16_t unresolvedRows = 0;
    uint16_t conflictRows = 0;
    uint16_t nodesWithParent = 0;
    uint16_t rootNodes = 0;
    uint16_t sameSkelPrev = 0;

    uint8_t scnMdlResolved = 0;
    uint8_t hierarchyStatus = A3hStatus_Unverified;
    uint8_t paletteMapStatus = A3hStatus_Unverified;
    uint8_t completeness = A3hStatus_Unverified;

    uint32_t primaryScnMdlEa = 0;
    uint32_t primaryResMdlEa = 0;
    uint32_t primaryPaletteBase = 0;
    uint16_t primaryNumViewMtx = 0;
    uint16_t primaryNodeCount = 0;

    A3hIdxLoad loads[kA3hMaxIdxLoads]{};
    A3hPalette pals[kA3hMaxPalettes]{};
    A3hNode nodes[kA3hMaxNodes]{};
    A3hRowMap rows[kA3hMaxRowMaps]{};
};

bool A3h_SkeletonTraceEnabled();
bool A3h_PaletteMapEnabled();
bool A3h_NeedsPlayerIdentity();

// GPU/FIFO thread: indexed XF pos-matrix load provenance.
void A3h_OnIndexedXf(uint32_t guestEa, uint32_t arrayBase, uint32_t index,
                     uint32_t stride, uint32_t xfAddr, uint32_t sizeWords);

// pose may be null — A3h fetches A3g_GetLatest() into a static buffer when needed.
void A3h_OnFrame(const A3aFrameSnap& cur, const A3gPoseSnap* pose);

bool A3h_GetLatest(A3hFrameSnap& out);
void A3h_OverlayLine(char* buf, size_t bufSize);
