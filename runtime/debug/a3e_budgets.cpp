#include "a3e_budgets.h"
#include "../log.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>

namespace
{

std::atomic<int> s_dense{ -1 };
std::atomic<int> s_armed{ 1 };  // default armed (backward compatible)
std::atomic<uint32_t> s_armDraws{ 0 };
std::atomic<uint32_t> s_peakObs{ 0 };
std::mutex s_mu;
char s_summary[256]{};
char s_stage[16]{};

uint32_t EnvU32(const char* name, uint32_t def)
{
    const char* e = std::getenv(name);
    if (!e || !e[0]) return def;
    char* end = nullptr;
    unsigned long v = std::strtoul(e, &end, 0);
    if (end == e) return def;
    if (v > 0xFFFFFFFFul) return 0xFFFFFFFFu;
    return (uint32_t)v;
}

bool DenseOn()
{
    int v = s_dense.load(std::memory_order_relaxed);
    if (v < 0)
    {
        const char* e = std::getenv("SCR_A3E_DENSE");
        v = (e && e[0] && e[0] != '0') ? 1 : 0;
        s_dense.store(v, std::memory_order_relaxed);
        if (v)
        {
            // Soft-arm unless explicitly disabled.
            const uint32_t armDraws = EnvU32("SCR_A3E_ARM_ON_DRAWS", 150);
            s_armDraws.store(armDraws, std::memory_order_relaxed);
            if (armDraws > 0)
                s_armed.store(0, std::memory_order_relaxed);
            LOG_INFO("A3e: dense archive budgets enabled (SCR_A3E_DENSE); {}", A3e_BudgetSummary());
            if (armDraws > 0)
                LOG_INFO("A3e: A2b soft-arm until observed draws>={} (SCR_A3E_ARM_ON_DRAWS); "
                         "stage enter also arms",
                         armDraws);
        }
    }
    return v != 0;
}

}  // namespace

bool A3e_DenseEnabled() { return DenseOn(); }

bool A3e_CaptureArmed()
{
    DenseOn();
    return s_armed.load(std::memory_order_relaxed) != 0;
}

void A3e_NoteObservedDraws(uint32_t drawCount)
{
    if (!DenseOn()) return;
    if (drawCount > s_peakObs.load(std::memory_order_relaxed))
        s_peakObs.store(drawCount, std::memory_order_relaxed);
    const uint32_t need = s_armDraws.load(std::memory_order_relaxed);
    if (need == 0 || A3e_CaptureArmed()) return;
    if (drawCount >= need)
    {
        s_armed.store(1, std::memory_order_relaxed);
        LOG_INFO("A3e: capture ARMED (observed draws={} >= {})", drawCount, need);
    }
}

void A3e_OnStageEntered(const char* stage)
{
    if (!DenseOn()) return;
    {
        std::lock_guard lk(s_mu);
        if (stage && stage[0])
            std::snprintf(s_stage, sizeof(s_stage), "%s", stage);
    }
    if (!A3e_CaptureArmed())
    {
        s_armed.store(1, std::memory_order_relaxed);
        LOG_INFO("A3e: capture ARMED (stage '{}' entered)", stage ? stage : "?");
    }
    else
        LOG_INFO("A3e: stage '{}' noted (already armed)", stage ? stage : "?");
}

uint32_t A2b_CapMaxSubs()
{
    const uint32_t def = DenseOn() ? 2048u : 512u;
    return EnvU32("SCR_A2B_MAX_SUBS", def);
}

uint32_t A2b_CapMaxBytes()
{
    const uint32_t def = DenseOn() ? (16u << 20) : (8u << 20);
    return EnvU32("SCR_A2B_MAX_BYTES", def);
}

uint32_t A2b_CapMaxBytesPerSub()
{
    return EnvU32("SCR_A2B_MAX_BYTES_PER_SUB", 256u << 10);
}

uint32_t A2b_CapMaxResources()
{
    // Busy TR1 soft-armed archives peaked at deps=8192 (previous dense default) with
    // Incomplete → not PassSelected. 16384 clears that ceiling for observed ~6.6k-draw frames.
    const uint32_t def = DenseOn() ? 16384u : 2048u;
    return EnvU32("SCR_A2B_MAX_RESOURCES", def);
}

uint32_t A2b_CapMaxResBytes()
{
    const uint32_t def = DenseOn() ? (128u << 20) : (24u << 20);
    return EnvU32("SCR_A2B_MAX_RES_BYTES", def);
}

uint32_t A2b_CapMaxResCopy()
{
    return EnvU32("SCR_A2B_MAX_RES_COPY", 512u << 10);
}

uint32_t A3a_CapMaxDraws()
{
    const uint32_t def = DenseOn() ? 16384u : 1536u;
    return EnvU32("SCR_A3A_MAX_DRAWS", def);
}

const char* A3e_BudgetSummary()
{
    std::snprintf(s_summary, sizeof(s_summary),
                  "a2b res=%u resB=%uMiB cmd=%uMiB subs=%u | a3a draws=%u | armed=%d peakObs=%u stage=%s",
                  A2b_CapMaxResources(), A2b_CapMaxResBytes() >> 20, A2b_CapMaxBytes() >> 20,
                  A2b_CapMaxSubs(), A3a_CapMaxDraws(), A3e_CaptureArmed() ? 1 : 0,
                  s_peakObs.load(std::memory_order_relaxed),
                  s_stage[0] ? s_stage : "-");
    return s_summary;
}

void A3e_OverlayLine(char* buf, size_t bufSize)
{
    if (!buf || !bufSize) return;
    buf[0] = 0;
    if (!DenseOn()) return;
    std::snprintf(buf, bufSize, "A3e dense armed=%d peakObs=%u resCap=%u",
                  A3e_CaptureArmed() ? 1 : 0, s_peakObs.load(std::memory_order_relaxed),
                  A2b_CapMaxResources());
}
