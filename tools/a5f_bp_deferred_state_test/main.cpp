// A5f: deferred BP family classification, flush boundaries, KSEL dirty model.
#include <cstdint>
#include <cstdio>
#include <cstring>

enum class GxBpClass : uint8_t { Ordinary = 0, Special, Trigger, Unknown };

static GxBpClass Classify(uint32_t reg)
{
    switch (reg)
    {
    case 0x45: case 0x47: case 0x48: case 0x52: case 0x65: case 0x66:
        return GxBpClass::Trigger;
    default:
        if (reg >= 0xE0 && reg <= 0xE7) return GxBpClass::Special;
        if (reg > 0xFF) return GxBpClass::Unknown;
        return GxBpClass::Ordinary;
    }
}

static bool NeedsImmediateOnBp(uint32_t reg)
{
    return reg == 0x52 || reg == 0x65 || (reg >= 0xE0 && reg <= 0xE7);
}

enum Fam : uint32_t
{
    F_None = 0,
    F_GenMode = 1u << 0,
    F_Tev = 1u << 1,
    F_Ksel = 1u << 2,
    F_Fog = 1u << 3,
    F_Alpha = 1u << 4,
    F_TexUnit = 1u << 5,
    F_Blend = 1u << 6,
    F_Depth = 1u << 7,
    F_Ras = 1u << 8,
    F_Light = 1u << 9,
    F_All = 0xFFFFFFFFu,
};

static uint32_t FamiliesForBp(uint32_t reg)
{
    if (reg == 0x52 || reg == 0x65 || reg == 0x66) return F_All;
    if (reg == 0x45 || reg == 0x47 || reg == 0x48) return F_None;
    if (reg >= 0xE0 && reg <= 0xE7) return F_None;
    if (reg == 0x00) return F_GenMode | F_Tev | F_Ras;
    if ((reg >= 0xC0 && reg <= 0xDF) || (reg >= 0x10 && reg <= 0x2F)) return F_Tev;
    if (reg >= 0xF6 && reg <= 0xFD) return F_Ksel | F_Tev;
    if (reg == 0xE8 || reg == 0xF0 || reg == 0xF1 || reg == 0xF2) return F_Fog;
    if (reg == 0xF3) return F_Alpha;
    if (reg == 0xF4 || reg == 0xF5) return F_Alpha | F_Fog;
    if ((reg >= 0x80 && reg <= 0xBB) || (reg >= 0x30 && reg <= 0x3F)) return F_TexUnit;
    if (reg == 0x41 || reg == 0x42 || reg == 0x43) return F_Blend;
    if (reg == 0x40 || reg == 0x44) return F_Depth;
    if (reg == 0x20 || reg == 0x21 || reg == 0x59 || reg == 0x5A) return F_Ras;
    return F_Tev | F_Fog | F_Alpha;
}

// Minimal deferred model for equivalence checks.
struct Model
{
    uint32_t bp[0x100]{};
    uint8_t valid[0x100]{};
    uint32_t psDirty = F_All;
    uint32_t vsDirty = F_All;
    bool psValid = false;
    bool vsValid = false;
    uint32_t psKeyKsel[8]{};
    uint32_t flushCalls = 0;
    uint32_t onBpCalls = 0;
    uint32_t flushSkip = 0;
    uint32_t onBpSkip = 0;
    bool pendingDraw = false;
    bool deferred = false;
    bool texInvalidated = false;
    bool efbCopied = false;
};

static void RebuildPs(Model& m)
{
    for (int i = 0; i < 8; i++)
        m.psKeyKsel[i] = m.bp[0xF6 + i] & 0xFFFFFF;
    m.psDirty = 0;
    m.psValid = true;
}

static void ApplyBp(Model& m, uint32_t word, bool immediateMode)
{
    const uint32_t reg = word >> 24;
    const uint32_t val = word & 0xFFFFFF;
    if (reg >= 0x100) return;
    const uint32_t old = m.bp[reg];
    const uint32_t nv = val; // mask=FFFFFF for tests
    const bool special = Classify(reg) != GxBpClass::Ordinary;
    const bool wasValid = m.valid[reg] != 0;
    if (nv == old && !special && wasValid)
        return;

    if (nv != old || special)
    {
        if (m.deferred && !immediateMode && !m.pendingDraw)
            m.flushSkip++;
        else
            m.flushCalls++;
    }
    m.bp[reg] = nv;
    m.valid[reg] = 1;

    if (reg == 0x66)
        m.texInvalidated = true;
    if (reg == 0x52)
        m.efbCopied = true;

    if (m.deferred && !immediateMode)
    {
        const uint32_t fam = FamiliesForBp(reg);
        m.psDirty |= (fam == F_All) ? (F_GenMode | F_Tev | F_Ksel | F_Fog | F_Alpha | F_TexUnit | F_Blend | F_Depth | F_Ras)
                                    : (fam & (F_GenMode | F_Tev | F_Ksel | F_Fog | F_Alpha | F_TexUnit | F_Blend | F_Depth | F_Ras));
        m.vsDirty |= (fam == F_All) ? (F_GenMode | F_Ras | F_Light) : (fam & (F_GenMode | F_Ras | F_Light));
        m.psValid = false;
        m.vsValid = false;
        if (NeedsImmediateOnBp(reg))
            m.onBpCalls++;
        else
            m.onBpSkip++;
    }
    else
    {
        m.onBpCalls++;
        RebuildPs(m); // immediate: derived follows every write
    }
}

static void ConsumeDraw(Model& m)
{
    if (!m.psValid || m.psDirty)
        RebuildPs(m);
    m.pendingDraw = false;
}

static int Fail(const char* msg)
{
    std::printf("fail: %s\n", msg);
    return 1;
}

int main()
{
    // --- classification ---
    if (Classify(0x66) != GxBpClass::Trigger) return Fail("0x66 trigger");
    if (Classify(0xF6) != GxBpClass::Ordinary) return Fail("0xF6 ordinary");
    if (Classify(0xE0) != GxBpClass::Special) return Fail("0xE0 special");
    if (!NeedsImmediateOnBp(0x52) || !NeedsImmediateOnBp(0x65) || !NeedsImmediateOnBp(0xE1))
        return Fail("immediate OnBP");
    if (NeedsImmediateOnBp(0xF6) || NeedsImmediateOnBp(0xC0) || NeedsImmediateOnBp(0x66))
        return Fail("ordinary OnBP skippable");

    if ((FamiliesForBp(0xF6) & F_Ksel) == 0) return Fail("ksel family");
    if ((FamiliesForBp(0xC0) & F_Tev) == 0) return Fail("tev family");
    if (FamiliesForBp(0x52) != F_All) return Fail("efb all");
    if (FamiliesForBp(0xE3) != F_None) return Fail("konst no key dirty");
    if (FamiliesForBp(0x45) != F_None) return Fail("token none");

    // --- 1. Multiple ordinary BP before one draw (deferred vs immediate) ---
    Model imm{}, def{};
    def.deferred = true;
    const uint32_t tevWrites[] = {
        0xC0000011u, 0xC1000022u, 0xC2000033u, 0xF60000AAu, 0xF70000BBu, 0xF80000CCu,
    };
    for (uint32_t w : tevWrites)
    {
        ApplyBp(imm, w, true);
        ApplyBp(def, w, false);
    }
    if (def.flushCalls != 0) return Fail("deferred flush should skip without pending");
    if (def.flushSkip < 6) return Fail("expected flush skips");
    if (def.onBpSkip < 6) return Fail("expected OnBP skips for ordinary");
    if (imm.onBpCalls < 6) return Fail("immediate OnBP");

    // Shadow must match
    for (uint32_t r = 0xC0; r <= 0xC2; r++)
        if (imm.bp[r] != def.bp[r]) return Fail("tev shadow mismatch");
    for (uint32_t r = 0xF6; r <= 0xF8; r++)
        if (imm.bp[r] != def.bp[r]) return Fail("ksel shadow mismatch");

    ConsumeDraw(imm);
    ConsumeDraw(def);
    for (int i = 0; i < 8; i++)
        if (imm.psKeyKsel[i] != def.psKeyKsel[i]) return Fail("ksel key mismatch after draw");

    // --- 2. Special trigger between ordinary writes ---
    Model m2{};
    m2.deferred = true;
    m2.pendingDraw = true;
    ApplyBp(m2, 0xC00000FFu, false);
    if (m2.flushCalls != 1) return Fail("pending draw must flush");
    ApplyBp(m2, 0x66000001u, false); // TX_INVAL even if value repeats later
    if (!m2.texInvalidated) return Fail("0x66 invalidate");
    ApplyBp(m2, 0x66000001u, false); // identical but trigger — must not nop
    if (m2.flushSkip > 0 && m2.pendingDraw) return Fail("trigger must flush when pending");

    // --- 3. EFB copy consumer ---
    Model m3{};
    m3.deferred = true;
    ApplyBp(m3, 0xF1000007u, false);
    ApplyBp(m3, 0x52000001u, false);
    if (!m3.efbCopied) return Fail("efb copy");
    if (m3.onBpCalls < 1) return Fail("0x52 needs OnBP");

    // --- 4. Identical ordinary nop ---
    Model m4{};
    m4.deferred = true;
    ApplyBp(m4, 0x41000012u, false);
    const uint32_t f0 = m4.flushSkip, o0 = m4.onBpSkip;
    ApplyBp(m4, 0x41000012u, false);
    if (m4.flushSkip != f0 || m4.onBpSkip != o0) return Fail("identical ordinary must nop");

    // --- 5. Two draws separated by BP ---
    Model m5a{}, m5b{};
    m5b.deferred = true;
    ApplyBp(m5a, 0xF6000011u, true);
    ApplyBp(m5b, 0xF6000011u, false);
    ConsumeDraw(m5a);
    ConsumeDraw(m5b);
    ApplyBp(m5a, 0xF6000022u, true);
    ApplyBp(m5b, 0xF6000022u, false);
    ConsumeDraw(m5a);
    ConsumeDraw(m5b);
    if (m5a.psKeyKsel[0] != m5b.psKeyKsel[0] || m5a.psKeyKsel[0] != 0x22)
        return Fail("ksel across two draws");

    // --- 6. Mixed special konst + ordinary ---
    Model m6{};
    m6.deferred = true;
    ApplyBp(m6, 0xE0800012u, false); // konst bit set → OnBP
    if (m6.onBpCalls != 1) return Fail("konst OnBP");
    ApplyBp(m6, 0xC0000044u, false);
    if (m6.onBpSkip < 1) return Fail("tev after konst skippable");

    // SNCP8P addresses retained as documentation anchors
    if (0x8036A380u != 0x8036A380u) return 1; // __GXSetDirtyState
    if (0x8036D2B0u != 0x8036D2B0u) return 1; // GXSetTevSwapMode

    std::printf("scr_a5f_bp_deferred_state_test OK\n");
    return 0;
}
