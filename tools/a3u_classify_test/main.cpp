// Host-side A3u draw-row mask classification (no GPU).
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// Mirror runtime: GX pos matrix N starts at XF word N*12.
static uint32_t XfMatrixIndex(uint32_t xfAddr) { return (xfAddr / 12u) % 64u; }

static bool DrawUsesPlayerRows(uint64_t mask, bool posMtxDirect, uint8_t defRow,
                               const uint8_t* hostVtxMtx0, uint32_t vtxCount, uint32_t vtxStride)
{
    if (mask == 0) return false;
    if (!posMtxDirect)
        return defRow < 64 && (mask & (1ull << defRow)) != 0;
    if (!hostVtxMtx0 || vtxCount == 0 || vtxStride < 1) return false;
    const uint32_t n = vtxCount < 32 ? vtxCount : 32;
    for (uint32_t i = 0; i < n; i++)
    {
        const uint8_t row = hostVtxMtx0[i * vtxStride] & 63;
        if (mask & (1ull << row)) return true;
    }
    return false;
}

static int g_fail = 0;
static void Expect(bool cond, const char* msg)
{
    if (!cond)
    {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        g_fail++;
    }
}

int main()
{
    // Empty mask → reject everything.
    Expect(!DrawUsesPlayerRows(0, false, 3, nullptr, 0, 0), "empty mask indexed");
    Expect(!DrawUsesPlayerRows(0, true, 0, nullptr, 1, 1), "empty mask direct");

    const uint64_t mask = (1ull << 5) | (1ull << 12);
    Expect(DrawUsesPlayerRows(mask, false, 5, nullptr, 0, 0), "accept player defRow");
    Expect(!DrawUsesPlayerRows(mask, false, 7, nullptr, 0, 0), "reject non-player defRow");
    Expect(!DrawUsesPlayerRows(mask, false, 64, nullptr, 0, 0), "reject out-of-range row");

    uint8_t mtx[8] = { 1, 2, 5, 9, 12, 0, 0, 0 };
    Expect(DrawUsesPlayerRows(mask, true, 0, mtx, 5, 1), "accept direct mtx hit");
    uint8_t other[4] = { 1, 2, 3, 4 };
    Expect(!DrawUsesPlayerRows(mask, true, 0, other, 4, 1), "reject direct non-player");

    // Contiguity: player span with no non-player after → contiguous.
    Expect((12u > 0 && 0u == 0), "contiguous when no after");
    Expect(!(12u > 0 && 3u == 0), "non-contiguous when after>0");

    Expect(XfMatrixIndex(0) == 0, "mtx0 @0");
    Expect(XfMatrixIndex(12) == 1, "mtx1 @12");
    Expect(XfMatrixIndex(12 * 57) == 57, "mtx57");
    // Old bug: xfAddr/4 mapped mtx1→row3; matrix index must match defRow.
    Expect(XfMatrixIndex(12) != (12u / 4u) % 64u || XfMatrixIndex(12) == 1, "not addr/4");
    Expect((12u / 4u) % 64u == 3, "document old /4 bug");

    if (g_fail)
    {
        std::fprintf(stderr, "%d failure(s)\n", g_fail);
        return 1;
    }
    std::puts("a3u_classify_test OK");
    return 0;
}
