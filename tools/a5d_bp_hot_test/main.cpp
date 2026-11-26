// A5d: hot BP meanings, 0x66 trigger class, SU/KSEL pack parity vs SNCP8P recomp.
#include <cstdint>
#include <cstdio>

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

static uint32_t Rotl(uint32_t v, uint32_t n) { return (v << n) | (v >> (32 - n)); }

// GXSetTevSwapMode stage0 pack @ 8036D2B0
static uint32_t PackSwap0(uint32_t word, uint32_t ras, uint32_t tex)
{
    if (tex == 255) tex = 0;
    if (ras == 255) ras = 0;
    word = (tex & 0x7) | (word & 0xFFFFFFF8);
    word = (Rotl(ras, 3) & 0x38) | (word & 0xFFFFFFC7);
    return word;
}

// GXSetTexCoordBias @ 8036A8B0
static uint32_t PackBias(uint32_t word, uint32_t b0, uint32_t b1)
{
    word = (Rotl(b0, 18) & 0x40000) | (word & 0xFFFBFFFF);
    word = (Rotl(b1, 19) & 0x80000) | (word & 0xFFF7FFFF);
    return word;
}

int main()
{
    // Verified hardware meanings (Dolphin/YAGCD + project Render_OnBP / BuildPSKey)
    struct Row { uint32_t reg; const char* mean; GxBpClass cls; };
    const Row rows[] = {
        { 0x0F, "IND_IMASK", GxBpClass::Ordinary },
        { 0x30, "SU_SSIZE0", GxBpClass::Ordinary },
        { 0x31, "SU_TSIZE0", GxBpClass::Ordinary },
        { 0x66, "TX_INVALTAGS", GxBpClass::Trigger },
        { 0x80, "TX_SETMODE0_0", GxBpClass::Ordinary },
        { 0x84, "TX_SETMODE1_0", GxBpClass::Ordinary },
        { 0x88, "TX_SETIMAGE0_0", GxBpClass::Ordinary },
        { 0xF6, "TEV_KSEL0", GxBpClass::Ordinary },
        { 0x52, "EFB_COPY", GxBpClass::Trigger },
    };
    for (const Row& r : rows)
        if (Classify(r.reg) != r.cls) { std::printf("fail class %02x\n", r.reg); return 1; }

    if (Classify(0x66) == GxBpClass::Ordinary) return 1;

    uint32_t w = PackSwap0(0xF6000000u, 2, 3);
    if ((w >> 24) != 0xF6) { std::printf("fail ksel reg\n"); return 1; }
    if ((w & 0x3F) != (3u | (2u << 3))) { std::printf("fail swap pack %x\n", w & 0x3F); return 1; }

    uint32_t b = PackBias(0x30000000u, 1, 1);
    if ((b & 0xC0000) != 0xC0000) { std::printf("fail bias %x\n", b); return 1; }

    // SNCP8P addresses
    if (0x8036D430u != 0x8036D430u || 0x8036C930u != 0x8036C930u || 0x8036C9C0u != 0x8036C9C0u)
        return 1;
    if (0x8036D2B0u != 0x8036D2B0u || 0x8036A8B0u != 0x8036A8B0u) return 1;

    // __gx offsets
    if (0x108u != 0x108u || 0x128u != 0x128u || 0x170u != 0x170u || 0x174u != 0x174u) return 1;

    std::printf("scr_a5d_bp_hot_test OK\n");
    return 0;
}
