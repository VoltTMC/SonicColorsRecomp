#include "a2g_natural_trace.h"
#include "../log.h"
#include "../platform/platform.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <unordered_map>

namespace {

bool EnvOn()
{
    static int c = -1;
    if (c < 0)
    {
        const char* v = std::getenv("SCR_A2G_NATURAL_TRACE");
        c = (v && v[0] && v[0] != '0') ? 1 : 0;
        if (c)
            LOG_INFO("A2g-Natural: live EFB lifetime trace enabled (SCR_A2G_NATURAL_TRACE); "
                     "no behaviour change / not the synthetic selftest");
    }
    return c != 0;
}

struct GenRec
{
    uint32_t destKey = 0;
    uint64_t genId = 0;
    uint32_t createPresent = 0;
    uint64_t createXfb = 0;
    uint64_t createSim = 0;
    uint32_t w = 0, h = 0, fmt = 0;
    uint32_t sampleCount = 0;
    uint32_t firstSamplePresent = ~0u;
    uint64_t firstSampleXfb = 0;
    uint64_t firstSampleSim = 0;
    uint32_t lastSamplePresent = ~0u;
    bool replacedUnsampled = false;
    bool invalidatedUnsampled = false;
    bool alive = true;
};

constexpr uint32_t kMaxEdges = 128;
constexpr uint32_t kMaxLiveGens = 4096;

std::mutex s_mu;
A2gNatCounters s_ctr{};
A2gNatEdge s_edges[kMaxEdges]{};
uint32_t s_edgeCount = 0;
std::unordered_map<uint64_t, GenRec> s_gens;  // genId → rec
uint64_t s_lastSummaryMs = 0;

void NoteEdgeLocked(const GenRec& g, uint32_t cls)
{
    for (uint32_t i = 0; i < s_edgeCount; i++)
    {
        if (s_edges[i].genId == g.genId)
        {
            s_edges[i].sampleCount = g.sampleCount;
            s_edges[i].sampleXfb = g.firstSampleXfb;
            s_edges[i].sampleSim = g.firstSampleSim;
            s_edges[i].samplePresent = g.firstSamplePresent;
            s_edges[i].cls = cls;
            return;
        }
    }
    if (s_edgeCount >= kMaxEdges) return;
    A2gNatEdge& e = s_edges[s_edgeCount++];
    e = A2gNatEdge{};
    e.genId = g.genId;
    e.destKey = g.destKey;
    e.createXfb = g.createXfb;
    e.createSim = g.createSim;
    e.createPresent = g.createPresent;
    e.sampleXfb = g.firstSampleXfb;
    e.sampleSim = g.firstSampleSim;
    e.samplePresent = g.firstSamplePresent;
    e.cls = cls;
    e.sampleCount = g.sampleCount;
}

uint32_t ClassifyLocked(const GenRec& g, uint32_t curPresent, uint64_t curXfb)
{
    if (g.createPresent == curPresent)
        return A2gNat_SameFrame;
    const uint64_t dx = (curXfb >= g.createXfb) ? (curXfb - g.createXfb) : 0;
    if (dx <= 1)
        return A2gNat_NextXfb;
    return A2gNat_MultiXfb;
}

}  // namespace

bool A2gNat_Enabled() { return EnvOn(); }

void A2gNat_OnCreate(uint32_t destKey, uint64_t genId, uint32_t presentFrame, uint64_t xfb,
                     uint64_t simFrame, uint32_t w, uint32_t h, uint32_t fmt, bool replacedPrior)
{
    if (!EnvOn() || !genId) return;
    std::lock_guard lk(s_mu);
    s_ctr.creates++;
    if (replacedPrior)
        s_ctr.replacements++;
    if (s_gens.size() >= kMaxLiveGens)
    {
        // Drop oldest never-sampled to bound memory; count as retained-never-sampled.
        for (auto it = s_gens.begin(); it != s_gens.end();)
        {
            if (it->second.sampleCount == 0 && !it->second.alive)
            {
                s_ctr.gensNeverSampled++;
                it = s_gens.erase(it);
                if (s_gens.size() < kMaxLiveGens / 2)
                    break;
            }
            else
                ++it;
        }
    }
    GenRec& g = s_gens[genId];
    g = GenRec{};
    g.destKey = destKey;
    g.genId = genId;
    g.createPresent = presentFrame;
    g.createXfb = xfb;
    g.createSim = simFrame;
    g.w = w;
    g.h = h;
    g.fmt = fmt;
    g.alive = true;
    s_ctr.uniqueGens++;
}

void A2gNat_OnSample(uint32_t destKey, uint64_t genId, uint32_t createPresent, uint64_t createXfb,
                     uint64_t createSim, uint32_t curPresent, uint64_t curXfb, uint64_t curSim)
{
    if (!EnvOn() || !genId) return;
    std::lock_guard lk(s_mu);
    s_ctr.samples++;
    GenRec* gp = nullptr;
    auto it = s_gens.find(genId);
    if (it != s_gens.end())
        gp = &it->second;
    else
    {
        // Sample of a gen created before trace started — record lightweight.
        GenRec& g = s_gens[genId];
        g.destKey = destKey;
        g.genId = genId;
        g.createPresent = createPresent;
        g.createXfb = createXfb;
        g.createSim = createSim;
        g.alive = true;
        gp = &g;
    }
    GenRec& g = *gp;
    const bool first = (g.sampleCount == 0);
    g.sampleCount++;
    if (first)
    {
        g.firstSamplePresent = curPresent;
        g.firstSampleXfb = curXfb;
        g.firstSampleSim = curSim;
        s_ctr.gensSampled++;
        const uint32_t cls = ClassifyLocked(g, curPresent, curXfb);
        if (cls == A2gNat_SameFrame)
            s_ctr.sameFrame++;
        else if (cls == A2gNat_NextXfb)
            s_ctr.nextXfb++;
        else
            s_ctr.multiXfb++;
        if (cls != A2gNat_SameFrame)
        {
            const uint64_t life = (curXfb >= g.createXfb) ? (curXfb - g.createXfb) : 0;
            if (life > s_ctr.longestLifeXfb)
                s_ctr.longestLifeXfb = life;
            NoteEdgeLocked(g, cls);
            LOG_INFO("A2g-Natural CROSS: gen={} dest={:08x} createXfb={} sampleXfb={} "
                     "createSim={} sampleSim={} present={}->{} class={} lifeXfb={}",
                     g.genId, g.destKey, g.createXfb, curXfb, g.createSim, curSim,
                     g.createPresent, curPresent,
                     cls == A2gNat_NextXfb ? "next" : "multi", life);
        }
    }
    g.lastSamplePresent = curPresent;
}

void A2gNat_OnInvalidate(uint32_t destKey, uint64_t genId, bool hadSample)
{
    if (!EnvOn()) return;
    std::lock_guard lk(s_mu);
    s_ctr.invalidations++;
    auto it = s_gens.find(genId);
    if (it == s_gens.end()) return;
    GenRec& g = it->second;
    g.alive = false;
    if (!hadSample && g.sampleCount == 0)
    {
        g.invalidatedUnsampled = true;
        s_ctr.gensNeverSampled++;
    }
    (void)destKey;
}

void A2gNat_OnPresent()
{
    if (!EnvOn()) return;
    bool doSummary = false;
    {
        std::lock_guard lk(s_mu);
        s_ctr.presents++;
        const uint64_t now = Platform_TickMs();
        if (now - s_lastSummaryMs >= 5000 || (s_ctr.presents % 1000) == 0)
        {
            s_lastSummaryMs = now;
            LOG_INFO("A2g-Natural: presents={} creates={} samples={} same={} next={} multi={} "
                     "repl={} inv={} gensSamp={}/{} lifeMax={} edges={}",
                     s_ctr.presents, s_ctr.creates, s_ctr.samples, s_ctr.sameFrame, s_ctr.nextXfb,
                     s_ctr.multiXfb, s_ctr.replacements, s_ctr.invalidations, s_ctr.gensSampled,
                     s_ctr.uniqueGens, s_ctr.longestLifeXfb, s_edgeCount);
            doSummary = ((s_ctr.presents % 1000) == 0);
        }
    }
    if (doSummary)
        A2gNat_LogSummary("milestone");
}

void A2gNat_OnGuestFrame()
{
    if (!EnvOn()) return;
    std::lock_guard lk(s_mu);
    s_ctr.guestFrames++;
}

void A2gNat_GetCounters(A2gNatCounters& out)
{
    std::lock_guard lk(s_mu);
    out = s_ctr;
}

uint32_t A2gNat_GetEdgeCount()
{
    std::lock_guard lk(s_mu);
    return s_edgeCount;
}

bool A2gNat_GetEdge(uint32_t index, A2gNatEdge& out)
{
    std::lock_guard lk(s_mu);
    if (index >= s_edgeCount) return false;
    out = s_edges[index];
    return true;
}

void A2gNat_OverlayLine(char* buf, size_t bufSize)
{
    if (!buf || bufSize == 0) return;
    buf[0] = 0;
    if (!EnvOn()) return;
    A2gNatCounters c{};
    A2gNat_GetCounters(c);
    std::snprintf(buf, bufSize, "A2gNat same=%llu next=%llu multi=%llu",
                  (unsigned long long)c.sameFrame, (unsigned long long)c.nextXfb,
                  (unsigned long long)c.multiXfb);
}

void A2gNat_LogSummary(const char* tag)
{
    if (!EnvOn()) return;
    A2gNatCounters c{};
    A2gNat_GetCounters(c);
    LOG_INFO("A2g-Natural SUMMARY [{}]: presents={} guestFrames={} creates={} samples={} "
             "sameFrame={} nextXfb={} multiXfb={} replacements={} invalidations={} "
             "gensSampled={}/{} neverSamp≈{} longestLifeXfb={} crossEdges={}",
             tag ? tag : "-", c.presents, c.guestFrames, c.creates, c.samples, c.sameFrame,
             c.nextXfb, c.multiXfb, c.replacements, c.invalidations, c.gensSampled, c.uniqueGens,
             c.gensNeverSampled, c.longestLifeXfb, A2gNat_GetEdgeCount());
    const uint32_t n = A2gNat_GetEdgeCount();
    for (uint32_t i = 0; i < n && i < 32; i++)
    {
        A2gNatEdge e{};
        if (!A2gNat_GetEdge(i, e)) break;
        LOG_INFO("A2g-Natural edge[{}]: gen={} dest={:08x} xfb={}->{} sim={}->{} class={} n={}",
                 i, e.genId, e.destKey, e.createXfb, e.sampleXfb, e.createSim, e.sampleSim,
                 e.cls == A2gNat_NextXfb ? "next" : (e.cls == A2gNat_MultiXfb ? "multi" : "?"),
                 e.sampleCount);
    }
}
