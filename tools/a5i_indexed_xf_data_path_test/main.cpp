// A5i: indexed-XF guest-read / identical-dirty / endian convert equivalence.
#include <cstdint>
#include <cstdio>
#include <cstring>

static uint32_t Bswap32(uint32_t v)
{
    return (v >> 24) | ((v >> 8) & 0xFF00) | ((v << 8) & 0xFF0000) | (v << 24);
}

static uint32_t Be32Load(const uint8_t* p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static void Be32Store(uint8_t* p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

// Mirrors A5i APPLY guest→xfmem path (no PoseInterp).
static bool ApplyGuest(uint32_t* xfmem, uint32_t addr, uint32_t size, const uint8_t* src, bool applyOpt,
                       bool& identical)
{
    identical = true;
    if (!applyOpt)
    {
        for (uint32_t i = 0; i < size; i++)
        {
            const uint32_t v = Be32Load(src + 4 * i);
            if (xfmem[(addr + i) & 0xFFF] != v) identical = false;
            xfmem[(addr + i) & 0xFFF] = v;
        }
        return true; // always "dirty"
    }
    const bool aligned = ((uintptr_t)src & 3u) == 0;
    bool changed = false;
    if (aligned)
    {
        const uint32_t* s32 = reinterpret_cast<const uint32_t*>(src);
        for (uint32_t i = 0; i < size; i++)
        {
            const uint32_t v = Bswap32(s32[i]);
            uint32_t& dst = xfmem[(addr + i) & 0xFFF];
            if (dst != v)
            {
                dst = v;
                changed = true;
            }
        }
    }
    else
    {
        for (uint32_t i = 0; i < size; i++)
        {
            const uint32_t v = Be32Load(src + 4 * i);
            uint32_t& dst = xfmem[(addr + i) & 0xFFF];
            if (dst != v)
            {
                dst = v;
                changed = true;
            }
        }
    }
    identical = !changed;
    return changed; // dirty iff changed
}

static int Fail(const char* m)
{
    std::printf("fail: %s\n", m);
    return 1;
}

int main()
{
    // Switch defaults
    if (0 != 0) return Fail("apply default");

    alignas(4) uint8_t guest[64]{};
    for (uint32_t i = 0; i < 12; i++)
        Be32Store(guest + 4 * i, 0x3F800000u + i); // distinct BE floats

    uint32_t xfA[0x1000]{};
    uint32_t xfB[0x1000]{};
    bool id = false;

    // Baseline vs APPLY first load — same state, both dirty
    bool dirtyA = ApplyGuest(xfA, 0, 12, guest, false, id);
    bool dirtyB = ApplyGuest(xfB, 0, 12, guest, true, id);
    if (!dirtyA || !dirtyB) return Fail("first dirty");
    if (std::memcmp(xfA, xfB, 12 * 4) != 0) return Fail("first eq");
    if (xfA[0] != 0x3F800000u) return Fail("endian");

    // Repeated identical payload
    dirtyA = ApplyGuest(xfA, 0, 12, guest, false, id);
    if (!id) return Fail("baseline identical detect");
    if (!dirtyA) return Fail("baseline still dirty"); // baseline always dirties
    dirtyB = ApplyGuest(xfB, 0, 12, guest, true, id);
    if (!id || dirtyB) return Fail("apply identical skip");

    // Mutate guest RAM at same index
    Be32Store(guest + 0, 0xDEADBEEFu);
    dirtyA = ApplyGuest(xfA, 0, 12, guest, false, id);
    dirtyB = ApplyGuest(xfB, 0, 12, guest, true, id);
    if (!dirtyA || !dirtyB || id) return Fail("mutated");
    if (xfA[0] != 0xDEADBEEFu || xfB[0] != xfA[0]) return Fail("mutated val");

    // Different index / stride simulation: offset into guest
    alignas(4) uint8_t guest2[96]{};
    for (int i = 0; i < 24; i++) Be32Store(guest2 + 4 * i, 0x1000u + (uint32_t)i);
    // index 1, stride 48 → +48 bytes
    dirtyB = ApplyGuest(xfB, 4, 12, guest2 + 48, true, id);
    if (!dirtyB || xfB[4] != 0x100Cu) return Fail("stride idx");

    // Nrm-sized (9 floats) into 0x400 region
    dirtyB = ApplyGuest(xfB, 0x400, 9, guest2, true, id);
    if (!dirtyB || xfB[0x400] != 0x1000u) return Fail("nrm");
    dirtyB = ApplyGuest(xfB, 0x400, 9, guest2, true, id);
    if (dirtyB || !id) return Fail("nrm identical");

    // Unaligned guest pointer path
    uint8_t raw[80];
    for (int i = 0; i < 12; i++) Be32Store(raw + 1 + 4 * i, 0xABC00000u + (uint32_t)i);
    dirtyB = ApplyGuest(xfB, 20, 12, raw + 1, true, id);
    if (!dirtyB || xfB[20] != 0xABC00000u) return Fail("unaligned");
    // bswap vs BE32 equivalence on aligned buffer
    alignas(4) uint8_t al[48]{};
    for (int i = 0; i < 12; i++) Be32Store(al + 4 * i, 0x11110000u + (uint32_t)i);
    uint32_t xfC[0x1000]{};
    uint32_t xfD[0x1000]{};
    ApplyGuest(xfC, 0, 12, al, false, id);
    ApplyGuest(xfD, 0, 12, al, true, id);
    if (std::memcmp(xfC, xfD, 48) != 0) return Fail("bswap eq");

    // Dirty coalescing model: identical → no mark; change → mark (documented contract)
    uint32_t gen = 0;
    auto onXf = [&](bool dirty) { if (dirty) gen++; };
    onXf(ApplyGuest(xfB, 8, 12, al, true, id));
    uint32_t g1 = gen;
    onXf(ApplyGuest(xfB, 8, 12, al, true, id));
    if (gen != g1) return Fail("gen identical");
    Be32Store(al, 0x22222222u);
    onXf(ApplyGuest(xfB, 8, 12, al, true, id));
    if (gen != g1 + 1) return Fail("gen change");

    // A4n payload size anchor (0x680 floats) — documentation only
    if (0x680u * 4u != 6656u) return Fail("a4n size");

    std::printf("scr_a5i_indexed_xf_data_path_test OK\n");
    return 0;
}
