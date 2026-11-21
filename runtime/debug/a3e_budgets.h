#pragma once
#include <cstddef>
#include <cstdint>

// A3e — configurable dense-frame archive budgets (opt-in).
// SCR_A3E_DENSE=1 raises A2b/A3a caps for busy TR1. Individual SCR_A2B_MAX_* /
// SCR_A3A_MAX_DRAWS overrides still apply. Disabled diagnostics → no cost.
//
// Completeness: hitting a hard cap sets Incomplete and records a reject reason.
// Stratified/sampled modes (if used) must never report complete=true.

bool A3e_DenseEnabled();

// Soft arm: when SCR_A3E_ARM_ON_DRAWS>0, A2b stays idle until a frame observes
// that many draws (reduces stage-load host pressure on slow GPUs).
bool A3e_CaptureArmed();
void A3e_NoteObservedDraws(uint32_t drawCount);
void A3e_OnStageEntered(const char* stage);
void A3e_OverlayLine(char* buf, size_t bufSize);

uint32_t A2b_CapMaxSubs();
uint32_t A2b_CapMaxBytes();
uint32_t A2b_CapMaxBytesPerSub();
uint32_t A2b_CapMaxResources();
uint32_t A2b_CapMaxResBytes();
uint32_t A2b_CapMaxResCopy();

uint32_t A3a_CapMaxDraws();

const char* A3e_BudgetSummary();  // static string for one-time log
