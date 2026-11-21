#pragma once
#include <cstdint>

// A2g-Natural — observe unmodified live EFB→tex lifetimes (SCR_A2G_NATURAL_TRACE=1).
// Does not alter copy/cache/sample behaviour. Distinct from A2g synthetic selftest.

enum A2gNatClass : uint32_t
{
    A2gNat_SameFrame = 0,
    A2gNat_NextXfb = 1,       // sampled on the immediately next present/XFB
    A2gNat_MultiXfb = 2,      // sampled ≥2 presents later
    A2gNat_ReplacedBeforeSample = 3,
    A2gNat_InvalidatedBeforeSample = 4,
    A2gNat_RetainedNeverSampled = 5,
    A2gNat_Ambiguous = 6,
};

struct A2gNatCounters
{
    uint64_t creates = 0;
    uint64_t replacements = 0;      // same dest, new gen (old may be unsampled)
    uint64_t invalidations = 0;     // ram-hash fail / erase before reuse
    uint64_t samples = 0;           // GetTexture returned live EFB copy SRV (draw bind)
    uint64_t sameFrame = 0;
    uint64_t nextXfb = 0;
    uint64_t multiXfb = 0;
    uint64_t uniqueGens = 0;
    uint64_t gensSampled = 0;
    uint64_t gensNeverSampled = 0;
    uint64_t longestLifeXfb = 0;    // max (sampleXfb - createXfb) among cross samples
    uint64_t presents = 0;
    uint64_t guestFrames = 0;
};

struct A2gNatEdge
{
    uint64_t genId = 0;
    uint32_t destKey = 0;
    uint64_t createXfb = 0;
    uint64_t createSim = 0;
    uint32_t createPresent = 0;
    uint64_t sampleXfb = 0;
    uint64_t sampleSim = 0;
    uint32_t samplePresent = 0;
    uint32_t cls = A2gNat_Ambiguous;
    uint32_t sampleCount = 0;
};

bool A2gNat_Enabled();
void A2gNat_OnCreate(uint32_t destKey, uint64_t genId, uint32_t presentFrame, uint64_t xfb,
                     uint64_t simFrame, uint32_t w, uint32_t h, uint32_t fmt, bool replacedPrior);
void A2gNat_OnSample(uint32_t destKey, uint64_t genId, uint32_t createPresent, uint64_t createXfb,
                     uint64_t createSim, uint32_t curPresent, uint64_t curXfb, uint64_t curSim);
void A2gNat_OnInvalidate(uint32_t destKey, uint64_t genId, bool hadSample);
void A2gNat_OnPresent();
void A2gNat_OnGuestFrame();
// Stage / disc hints for gameplay coverage (no behaviour change).
void A2gNat_OnStageHint(const char* stage, const char* via);
void A2gNat_OnDiscPathHint(const char* path);
void A2gNat_GetCounters(A2gNatCounters& out);
uint32_t A2gNat_GetEdgeCount();
bool A2gNat_GetEdge(uint32_t index, A2gNatEdge& out);
void A2gNat_OverlayLine(char* buf, size_t bufSize);
void A2gNat_LogSummary(const char* tag);
