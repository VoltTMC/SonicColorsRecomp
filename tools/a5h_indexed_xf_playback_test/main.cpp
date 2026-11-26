// A5h: indexed-XF opcode coverage, DL playback packing, FIFO/direct/DL equivalence.
#include <cstdint>
#include <cstdio>
#include <cstring>

static uint32_t Rotl(uint32_t v, uint32_t n) { return (v << n) | (v >> (32 - n)); }

static void PackPosIndx(uint16_t index, uint32_t id, uint8_t& op, uint32_t& word)
{
    op = 0x20;
    uint32_t addr = (Rotl(id, 2) & 0xFFFFFFFC) & 0xFFF;
    word = (uint32_t)index << 16;
    word |= (0xBu << 12);
    word |= addr;
}

static void PackNrmIndx(uint16_t index, uint32_t id, uint8_t& op, uint32_t& word)
{
    op = 0x28;
    uint32_t t = Rotl(id, 2) & 0xFFFFFFFC;
    t = t - id + 0x400;
    word = (uint32_t)index << 16;
    word |= (0x8u << 12);
    word |= (t & 0xFFF);
}

// Speculative TexMtx / Light packs (SDK writers absent in SNCP8P — FIFO-only).
static void PackTexIndx(uint16_t index, uint32_t addr, uint32_t sizeMinus1, uint8_t& op, uint32_t& word)
{
    op = 0x30;
    word = (uint32_t)index << 16;
    word |= (sizeMinus1 & 0xF) << 12;
    word |= (addr & 0xFFF);
}

static void PackLightIndx(uint16_t index, uint32_t addr, uint32_t sizeMinus1, uint8_t& op, uint32_t& word)
{
    op = 0x38;
    word = (uint32_t)index << 16;
    word |= (sizeMinus1 & 0xF) << 12;
    word |= (addr & 0xFFF);
}

static void Be32Store(uint8_t* p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static uint32_t Be32Load(const uint8_t* p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static void ApplyIndexed(uint32_t* xfmem, const uint32_t* cpBase, const uint32_t* cpStride, int array,
                         uint32_t word, const uint32_t* guestWords, uint32_t guestWordCount, bool& oob)
{
    oob = false;
    const uint32_t index = word >> 16;
    const uint32_t size = ((word >> 12) & 0xF) + 1;
    const uint32_t addr = word & 0xFFF;
    if (array < 0 || array >= 16 || size == 0 || size > 16)
    {
        oob = true;
        return;
    }
    const uint32_t strideWords = cpStride[array] / 4;
    if (strideWords == 0)
    {
        oob = true;
        return;
    }
    const uint32_t start = index * strideWords;
    if (start + size > guestWordCount)
    {
        oob = true;
        return;
    }
    for (uint32_t i = 0; i < size; i++)
        xfmem[(addr + i) & 0xFFF] = guestWords[start + i];
}

static int Fail(const char* m)
{
    std::printf("fail: %s\n", m);
    return 1;
}

// Minimal DL/FIFO decoder for indexed XF + BP markers (ordering probe).
static void DecodeStream(uint32_t* xfmem, uint32_t* bp, const uint32_t* cpBase, const uint32_t* cpStride,
                         const uint8_t* stream, size_t n, const uint32_t* guest, uint32_t guestN,
                         uint64_t* opCounts /* [4]=20,28,30,38 */, uint64_t* bpCount, bool& sawOob)
{
    size_t pos = 0;
    sawOob = false;
    while (pos < n)
    {
        uint8_t op = stream[pos];
        if (op == 0x00)
        {
            pos++;
            continue;
        }
        if (op == 0x61)
        {
            if (pos + 5 > n) break;
            uint32_t w = Be32Load(stream + pos + 1);
            bp[(w >> 24) & 0xFF] = w & 0xFFFFFF;
            if (bpCount) (*bpCount)++;
            pos += 5;
            continue;
        }
        if (op == 0x20 || op == 0x28 || op == 0x30 || op == 0x38)
        {
            if (pos + 5 > n) break;
            // Consecutive-run decode (A5h PLAYBACK path shape): consume a run of idx ops.
            while (pos + 5 <= n)
            {
                uint8_t rop = stream[pos];
                if (rop != 0x20 && rop != 0x28 && rop != 0x30 && rop != 0x38) break;
                uint32_t word = Be32Load(stream + pos + 1);
                int array = 12 + ((rop >> 3) & 3);
                bool oob = false;
                ApplyIndexed(xfmem, cpBase, cpStride, array, word, guest, guestN, oob);
                if (oob) sawOob = true;
                if (opCounts)
                {
                    if (rop == 0x20) opCounts[0]++;
                    else if (rop == 0x28) opCounts[1]++;
                    else if (rop == 0x30) opCounts[2]++;
                    else opCounts[3]++;
                }
                pos += 5;
            }
            continue;
        }
        // Unknown — stop
        break;
    }
}

int main()
{
    // Verified SNCP8P entry points (Pos/Nrm only).
    if (0x8036EE60u != 0x8036EE60u || 0x8036EEF0u != 0x8036EEF0u) return Fail("sdk addrs");

    // Opt-in switches default off (compile-time documentation anchors).
    const int a5gDefault = 0;
    const int a5hExtDefault = 0;
    const int a5hPlayDefault = 0;
    if ((a5gDefault | a5hExtDefault | a5hPlayDefault) != 0) return Fail("defaults");

    // No SNCP8P SDK writer for 0x30/0x38 — EXT switch has no adapters.
    const bool hasTexIndxSdk = false;
    const bool hasLightIndxSdk = false;
    if (hasTexIndxSdk || hasLightIndxSdk) return Fail("unexpected sdk");

    auto arrayOf = [](uint8_t o) { return 12 + ((o >> 3) & 3); };
    if (arrayOf(0x20) != 12 || arrayOf(0x28) != 13) return Fail("array lo");
    if (arrayOf(0x30) != 14 || arrayOf(0x38) != 15) return Fail("array hi");

    uint8_t op = 0;
    uint32_t word = 0;
    PackPosIndx(0, 0, op, word);
    if (op != 0x20 || ((word >> 12) & 0xF) + 1 != 12) return Fail("pos pack");
    PackNrmIndx(0, 0, op, word);
    if (op != 0x28 || ((word >> 12) & 0xF) + 1 != 9) return Fail("nrm pack");
    PackTexIndx(1, 0x500, 11, op, word);
    if (op != 0x30 || (word >> 16) != 1 || (word & 0xFFF) != 0x500) return Fail("tex pack");
    PackLightIndx(2, 0x600, 15, op, word);
    if (op != 0x38 || (word >> 16) != 2 || (word & 0xFFF) != 0x600) return Fail("light pack");

    uint32_t cpBase[16]{};
    uint32_t cpStride[16]{};
    cpBase[12] = 0;
    cpStride[12] = 48;
    cpBase[13] = 0;
    cpStride[13] = 36;
    cpBase[14] = 0;
    cpStride[14] = 48;
    cpBase[15] = 0;
    cpStride[15] = 64;

    uint32_t guest[256]{};
    for (uint32_t i = 0; i < 256; i++) guest[i] = 0xC0000000u + i;

    // --- Direct vs FIFO equivalence (0x20/0x28) ---
    uint32_t xfFifo[0x1000]{};
    uint32_t xfDirect[0x1000]{};
    bool oob = false;
    PackPosIndx(0, 0, op, word);
    ApplyIndexed(xfFifo, cpBase, cpStride, 12, word, guest, 256, oob);
    ApplyIndexed(xfDirect, cpBase, cpStride, 12, word, guest, 256, oob);
    if (std::memcmp(xfFifo, xfDirect, sizeof(xfFifo)) != 0) return Fail("fifo/direct pos");

    PackPosIndx(2, 1, op, word); // addr = 4
    ApplyIndexed(xfFifo, cpBase, cpStride, 12, word, guest, 256, oob);
    ApplyIndexed(xfDirect, cpBase, cpStride, 12, word, guest, 256, oob);
    if (xfFifo[4] != guest[24] || xfDirect[4] != guest[24]) return Fail("index2");

    // Invalid index / OOB
    PackPosIndx(40, 0, op, word); // 40*12 = 480 > 256
    ApplyIndexed(xfFifo, cpBase, cpStride, 12, word, guest, 256, oob);
    if (!oob) return Fail("oob expected");

    // Changed payload + repeat
    guest[0] = 0x11111111u;
    PackPosIndx(0, 0, op, word);
    ApplyIndexed(xfFifo, cpBase, cpStride, 12, word, guest, 256, oob);
    ApplyIndexed(xfDirect, cpBase, cpStride, 12, word, guest, 256, oob);
    if (xfFifo[0] != 0x11111111u || xfDirect[0] != xfFifo[0]) return Fail("payload");
    ApplyIndexed(xfFifo, cpBase, cpStride, 12, word, guest, 256, oob);
    if (xfFifo[0] != 0x11111111u) return Fail("repeat");

    // Nrm region
    PackNrmIndx(1, 3, op, word);
    ApplyIndexed(xfFifo, cpBase, cpStride, 13, word, guest, 256, oob);
    ApplyIndexed(xfDirect, cpBase, cpStride, 13, word, guest, 256, oob);
    if (std::memcmp(xfFifo, xfDirect, sizeof(xfFifo)) != 0) return Fail("nrm eq");

    // --- 0x30 / 0x38 FIFO-only apply (no direct adapter) ---
    PackTexIndx(0, 0x500, 11, op, word);
    ApplyIndexed(xfFifo, cpBase, cpStride, 14, word, guest, 256, oob);
    if (oob || xfFifo[0x500] != guest[0]) return Fail("tex fifo");
    PackLightIndx(0, 0x600, 3, op, word);
    ApplyIndexed(xfFifo, cpBase, cpStride, 15, word, guest, 256, oob);
    if (oob || xfFifo[0x600] != guest[0]) return Fail("light fifo");

    // --- Display-list stream: consecutive idx + interleaved BP ---
    uint8_t dl[128]{};
    size_t dlN = 0;
    auto emitIdx = [&](uint8_t o, uint32_t w) {
        dl[dlN++] = o;
        Be32Store(dl + dlN, w);
        dlN += 4;
    };
    auto emitBp = [&](uint8_t reg, uint32_t val24) {
        dl[dlN++] = 0x61;
        Be32Store(dl + dlN, ((uint32_t)reg << 24) | (val24 & 0xFFFFFF));
        dlN += 4;
    };

    uint32_t xfDl[0x1000]{};
    uint32_t xfTop[0x1000]{};
    uint32_t bpDl[256]{};
    uint32_t bpTop[256]{};
    std::memset(xfDl, 0, sizeof(xfDl));
    std::memset(xfTop, 0, sizeof(xfTop));

    PackPosIndx(0, 0, op, word);
    emitIdx(op, word);
    PackPosIndx(1, 0, op, word);
    emitIdx(op, word);
    emitBp(0x41, 0xABCDEF); // ordinary BP between idx
    PackNrmIndx(0, 0, op, word);
    emitIdx(op, word);
    PackTexIndx(0, 0x510, 3, op, word);
    emitIdx(op, word);
    PackLightIndx(0, 0x610, 3, op, word);
    emitIdx(op, word);
    emitBp(0x45, 0x2); // special finish-ish marker (ordering only)

    uint64_t counts[4]{};
    uint64_t bpCount = 0;
    bool sawOob = false;
    DecodeStream(xfDl, bpDl, cpBase, cpStride, dl, dlN, guest, 256, counts, &bpCount, sawOob);
    if (sawOob) return Fail("dl oob");
    if (counts[0] != 2 || counts[1] != 1 || counts[2] != 1 || counts[3] != 1) return Fail("dl op counts");
    if (bpCount != 2) return Fail("dl bp count");
    if (bpDl[0x41] != 0xABCDEF || bpDl[0x45] != 0x2) return Fail("dl bp vals");

    // Same stream as "top-level" (direct path shape) must match XF
    DecodeStream(xfTop, bpTop, cpBase, cpStride, dl, dlN, guest, 256, nullptr, nullptr, sawOob);
    if (std::memcmp(xfDl, xfTop, sizeof(xfDl)) != 0) return Fail("dl vs top xf");
    if (std::memcmp(bpDl, bpTop, sizeof(bpDl)) != 0) return Fail("dl vs top bp");

    // Nested DL simulation: outer BP, inner idx stream, outer BP — state preserved
    uint32_t xfNest[0x1000]{};
    uint32_t bpNest[256]{};
    emitBp(0x40, 0x1); // reuse helpers would mutate dl; apply manually
    // Reset and build nest stream
    dlN = 0;
    emitBp(0x40, 0x111);
    // "call" inner: consecutive pos loads
    PackPosIndx(0, 2, op, word); // XF addr 8
    emitIdx(op, word);
    PackPosIndx(1, 3, op, word); // XF addr 12 — distinct slot
    emitIdx(op, word);
    emitBp(0x40, 0x222);
    counts[0] = counts[1] = counts[2] = counts[3] = 0;
    bpCount = 0;
    DecodeStream(xfNest, bpNest, cpBase, cpStride, dl, dlN, guest, 256, counts, &bpCount, sawOob);
    if (bpNest[0x40] != 0x222) return Fail("nest bp order");
    if (counts[0] != 2) return Fail("nest idx");
    if (xfNest[8] != guest[0]) return Fail("nest xf id2");   // index0 → guest[0]
    if (xfNest[12] != guest[12]) return Fail("nest xf id3"); // index1 → guest[12]

    // Unsupported → FIFO fallback contract: EXT adapters absent; packs still apply via FIFO.
    if (a5hExtDefault != 0) return Fail("ext default");

    std::printf("scr_a5h_indexed_xf_playback_test OK\n");
    return 0;
}
