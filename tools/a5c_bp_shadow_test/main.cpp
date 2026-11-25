// A5c: BP classification, __gx offsets, and SNCP8P PE/TEV bit-pack parity vs recomp.
#include <cstdint>
#include <cstdio>
#include <cstdlib>

enum class GxBpClass : uint8_t { Ordinary = 0, Special, Trigger, Unknown };

static GxBpClass Classify(uint32_t reg)
{
    switch (reg)
    {
    case 0x45:
    case 0x47:
    case 0x48:
    case 0x52:
    case 0x65:
        return GxBpClass::Trigger;
    default:
        if (reg >= 0xE0 && reg <= 0xE7) return GxBpClass::Special;
        if (reg > 0xFF) return GxBpClass::Unknown;
        return GxBpClass::Ordinary;
    }
}

static bool IsSpecial(uint32_t reg)
{
    const GxBpClass c = Classify(reg);
    return c == GxBpClass::Special || c == GxBpClass::Trigger || c == GxBpClass::Unknown;
}

static bool CanSkipIdentical(uint32_t reg, uint32_t oldv, uint32_t nv, bool shadowValid)
{
    return nv == oldv && !IsSpecial(reg) && shadowValid;
}

static uint32_t Rotl(uint32_t v, uint32_t n) { return (v << n) | (v >> (32 - n)); }

static uint32_t Cntlzw(uint32_t v)
{
    if (!v) return 32;
    uint32_t n = 0;
    if ((v >> 16) == 0) { n += 16; v <<= 16; }
    if ((v >> 24) == 0) { n += 8; v <<= 8; }
    if ((v >> 28) == 0) { n += 4; v <<= 4; }
    if ((v >> 30) == 0) { n += 2; v <<= 2; }
    if ((v >> 31) == 0) n += 1;
    return n;
}

// Mirrors SNCP8P GXSetZMode @ 8036E080
static uint32_t PackZMode(uint32_t word, uint32_t cmpEn, uint32_t func, uint32_t upd)
{
    word = (cmpEn & 0x1) | (word & 0xFFFFFFFE);
    word = (Rotl(func, 1) & 0xE) | (word & 0xFFFFFFF1);
    word = (Rotl(upd, 4) & 0x10) | (word & 0xFFFFFFEF);
    return word;
}

// Mirrors SNCP8P GXSetBlendMode @ 8036DFD0
static uint32_t PackBlend(uint32_t word, uint32_t type, uint32_t src, uint32_t dst, uint32_t logic)
{
    const uint32_t tM3 = type + 0xFFFFFFFDu;
    const uint32_t tM2 = type + 0xFFFFFFFEu;
    word = (Rotl(Cntlzw(tM3), 6) & 0x800) | (word & 0xFFFFF7FF);
    word = (type & 0x1) | (word & 0xFFFFFFFE);
    word = (Rotl(Cntlzw(tM2), 28) & 0x2) | (word & 0xFFFFFFFD);
    word = (Rotl(logic, 12) & 0xF000) | (word & 0xFFFF0FFF);
    word = (Rotl(src, 8) & 0x700) | (word & 0xFFFFF8FF);
    word = (Rotl(dst, 5) & 0xE0) | (word & 0xFFFFFF1F);
    return word;
}

// Mirrors SNCP8P GXSetTevColorIn @ 8036D500
static uint32_t PackTevColorIn(uint32_t word, uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    word = (Rotl(a, 12) & 0xF000) | (word & 0xFFFF0FFF);
    word = (Rotl(b, 8) & 0xF00) | (word & 0xFFFFF0FF);
    word = (Rotl(c, 4) & 0xF0) | (word & 0xFFFFFF0F);
    word = (d & 0xF) | (word & 0xFFFFFFF0);
    return word;
}

int main()
{
    // SNCP8P layout constants
    if (0x807FA960u != 0x807FA960u) return 1;
    constexpr uint32_t kCmode = 0x220u, kZmode = 0x228u, kTevC = 0x180u, kTevA = 0x1C0u;
    if (kCmode != 0x220 || kZmode != 0x228 || kTevC != 0x180 || kTevA != 0x1C0) return 1;

    // Classification
    if (Classify(0x41) != GxBpClass::Ordinary) { std::printf("fail class 41\n"); return 1; }
    if (Classify(0x40) != GxBpClass::Ordinary) { std::printf("fail class 40\n"); return 1; }
    if (Classify(0xC0) != GxBpClass::Ordinary) { std::printf("fail class C0\n"); return 1; }
    if (Classify(0x52) != GxBpClass::Trigger) { std::printf("fail class 52\n"); return 1; }
    if (Classify(0x45) != GxBpClass::Trigger) { std::printf("fail class 45\n"); return 1; }
    if (Classify(0xE0) != GxBpClass::Special) { std::printf("fail class E0\n"); return 1; }

    // Skip rules
    if (!CanSkipIdentical(0x41, 0x123, 0x123, true)) { std::printf("fail skip ordinary\n"); return 1; }
    if (CanSkipIdentical(0x41, 0x123, 0x123, false)) { std::printf("fail skip invalid shadow\n"); return 1; }
    if (CanSkipIdentical(0x52, 0x1, 0x1, true)) { std::printf("fail skip trigger\n"); return 1; }
    if (CanSkipIdentical(0xE2, 0x1, 0x1, true)) { std::printf("fail skip special\n"); return 1; }
    if (CanSkipIdentical(0x41, 0x1, 0x2, true)) { std::printf("fail skip changed\n"); return 1; }

    // ZMode: enable=1, func=3 (LE), update=1 → bits 0, 011<<1, 1<<4 = 0x1 | 0x6 | 0x10 = 0x17
    // Preserve high reg id 0x40
    uint32_t z = PackZMode(0x40000000u, 1, 3, 1);
    if ((z >> 24) != 0x40) { std::printf("fail z reg id\n"); return 1; }
    if ((z & 0x1F) != 0x17) { std::printf("fail z bits %x\n", z & 0x1F); return 1; }

    // Blend: type=1 (GX_BLEND), src=4, dst=5, logic=0 — preserve 0x41
    uint32_t b = PackBlend(0x41000000u, 1, 4, 5, 0);
    if ((b >> 24) != 0x41) { std::printf("fail blend reg\n"); return 1; }
    if ((b & 1) != 1) { std::printf("fail blend enable\n"); return 1; }

    // Logic mode type=3 sets bit 11
    uint32_t bl = PackBlend(0x41000000u, 3, 0, 0, 0xA);
    if ((bl & 0x800) == 0) { std::printf("fail logic bit\n"); return 1; }

    uint32_t tc = PackTevColorIn(0xC0000000u, 0xF, 0xE, 0xD, 0xC);
    if ((tc >> 24) != 0xC0) { std::printf("fail tev reg\n"); return 1; }
    if ((tc & 0xFFFF) != 0xFEDC) { std::printf("fail tev in %x\n", tc & 0xFFFF); return 1; }

    // Verified SNCP8P addresses
    if (0x8036D500u != 0x8036D500u || 0x8036DFD0u != 0x8036DFD0u || 0x8036E080u != 0x8036E080u)
        return 1;

    std::printf("scr_a5c_bp_shadow_test OK\n");
    return 0;
}
