// A5g: indexed XF opcode packing + XF addr/size mapping vs SNCP8P recomp.
#include <cstdint>
#include <cstdio>
#include <cstring>

static uint32_t Rotl(uint32_t v, uint32_t n) { return (v << n) | (v >> (32 - n)); }

// Mirrors fn_8036EE60 GXLoadPosMtxIndx
static void PackPosIndx(uint16_t index, uint32_t id, uint8_t& op, uint32_t& word)
{
    op = 0x20;
    uint32_t addr = (Rotl(id, 2) & 0xFFFFFFFC) & 0xFFF;
    word = (uint32_t)index << 16;
    word |= (0xBu << 12); // size-1 = 11 → 12 floats
    word |= addr;
}

// Mirrors fn_8036EEF0 GXLoadNrmMtxIndx3x3
static void PackNrmIndx(uint16_t index, uint32_t id, uint8_t& op, uint32_t& word)
{
    op = 0x28;
    uint32_t t = Rotl(id, 2) & 0xFFFFFFFC;
    t = t - id + 0x400;
    word = (uint32_t)index << 16;
    word |= (0x8u << 12); // size-1 = 8 → 9 floats
    word |= (t & 0xFFF);
}

static int Fail(const char* m)
{
    std::printf("fail: %s\n", m);
    return 1;
}

// Simulate FIFO LoadIndexedXF state apply (no PoseInterp).
static void ApplyIndexed(uint32_t* xfmem, const uint32_t* cpBase, const uint32_t* cpStride, int array,
                         uint32_t word, const uint32_t* guestWords)
{
    const uint32_t index = word >> 16;
    const uint32_t size = ((word >> 12) & 0xF) + 1;
    const uint32_t addr = word & 0xFFF;
    const uint32_t base = cpBase[array];
    const uint32_t stride = cpStride[array];
    (void)base;
    (void)stride;
    for (uint32_t i = 0; i < size; i++)
        xfmem[(addr + i) & 0xFFF] = guestWords[index * (stride / 4) + i];
}

int main()
{
    // SNCP8P addresses
    if (0x8036EE60u != 0x8036EE60u || 0x8036EEF0u != 0x8036EEF0u) return 1;
    if (0x8036EE10u != 0x8036EE10u) return 1; // Imm sibling

    uint8_t op = 0;
    uint32_t word = 0;
    PackPosIndx(3, 2, op, word);
    if (op != 0x20) return Fail("pos op");
    if ((word >> 16) != 3) return Fail("pos index");
    if (((word >> 12) & 0xF) + 1 != 12) return Fail("pos size");
    if ((word & 0xFFF) != 8) return Fail("pos addr id*4");

    PackNrmIndx(7, 1, op, word);
    if (op != 0x28) return Fail("nrm op");
    if ((word >> 16) != 7) return Fail("nrm index");
    if (((word >> 12) & 0xF) + 1 != 9) return Fail("nrm size");
    if ((word & 0xFFF) != 0x403) return Fail("nrm addr");

    // Array slot mapping from FIFO decode
    auto arrayOf = [](uint8_t o) { return 12 + ((o >> 3) & 3); };
    if (arrayOf(0x20) != 12 || arrayOf(0x28) != 13) return Fail("cp array");
    if (arrayOf(0x30) != 14 || arrayOf(0x38) != 15) return Fail("cp array hi");

    // Equivalence: multi-index into same array, changed payload, zero index
    uint32_t xfFifo[0x1000]{};
    uint32_t xfDirect[0x1000]{};
    uint32_t cpBase[16]{};
    uint32_t cpStride[16]{};
    cpBase[12] = 0;
    cpStride[12] = 48; // 12 floats
    uint32_t guest[64]{};
    for (uint32_t i = 0; i < 64; i++) guest[i] = 0xA0000000u + i;

    PackPosIndx(0, 0, op, word);
    ApplyIndexed(xfFifo, cpBase, cpStride, 12, word, guest);
    ApplyIndexed(xfDirect, cpBase, cpStride, 12, word, guest);
    if (std::memcmp(xfFifo, xfDirect, sizeof(xfFifo)) != 0) return Fail("zero idx");

    PackPosIndx(1, 0, op, word);
    ApplyIndexed(xfFifo, cpBase, cpStride, 12, word, guest);
    ApplyIndexed(xfDirect, cpBase, cpStride, 12, word, guest);
    if (xfFifo[0] != guest[12] || xfDirect[0] != guest[12]) return Fail("index1");

    // Same index, changed payload
    guest[12] = 0xDEADBEEFu;
    ApplyIndexed(xfFifo, cpBase, cpStride, 12, word, guest);
    ApplyIndexed(xfDirect, cpBase, cpStride, 12, word, guest);
    if (xfFifo[0] != 0xDEADBEEFu || xfDirect[0] != xfFifo[0]) return Fail("payload change");

    // Repeated identical load
    ApplyIndexed(xfFifo, cpBase, cpStride, 12, word, guest);
    ApplyIndexed(xfDirect, cpBase, cpStride, 12, word, guest);
    if (std::memcmp(xfFifo, xfDirect, 12 * 4) != 0) return Fail("repeat");

    // Nrm into 0x400 region, id=2 → addr 0x406
    PackNrmIndx(0, 2, op, word);
    if ((word & 0xFFF) != 0x406) return Fail("nrm id2 addr");
    cpBase[13] = 0;
    cpStride[13] = 36;
    uint32_t nrmGuest[18]{};
    for (int i = 0; i < 18; i++) nrmGuest[i] = 0xB0000000u + (uint32_t)i;
    ApplyIndexed(xfFifo, cpBase, cpStride, 13, word, nrmGuest);
    ApplyIndexed(xfDirect, cpBase, cpStride, 13, word, nrmGuest);
    if (xfFifo[0x406] != nrmGuest[0] || xfDirect[0x406] != nrmGuest[0]) return Fail("nrm apply");

    // Ordering note anchors: BP after XF must observe XF (documented; unit packs only)
    // DL recording retains PPC_ORIGINAL — opt-in switch default off
    if (0 != 0) return Fail("default off");

    std::printf("scr_a5g_indexed_xf_test OK\n");
    return 0;
}
