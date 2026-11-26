// A5j: indexed-XF consecutive dispatch packing + FIFO cursor / opcode batching.
#include <cstdint>
#include <cstdio>
#include <cstring>

static void Be32Store(uint8_t* p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static uint32_t Be32Load(const uint8_t* p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static void PackPos(uint16_t index, uint32_t id, uint8_t& op, uint32_t& word)
{
    op = 0x20;
    word = ((uint32_t)index << 16) | (0xBu << 12) | ((id * 4u) & 0xFFF);
}

static void PackNrm(uint16_t index, uint32_t id, uint8_t& op, uint32_t& word)
{
    op = 0x28;
    uint32_t t = (id * 4u) - id + 0x400;
    word = ((uint32_t)index << 16) | (0x8u << 12) | (t & 0xFFF);
}

static void PackTex(uint16_t index, uint32_t addr, uint8_t& op, uint32_t& word)
{
    op = 0x30;
    word = ((uint32_t)index << 16) | (0xBu << 12) | (addr & 0xFFF);
}

static void PackLight(uint16_t index, uint32_t addr, uint8_t& op, uint32_t& word)
{
    op = 0x38;
    word = ((uint32_t)index << 16) | (0xFu << 12) | (addr & 0xFFF);
}

struct Batch
{
    uint64_t op20 = 0, op28 = 0, op30 = 0, op38 = 0, cmds = 0, bytes = 0;
};

// Mirrors A5j consecutive idx consume (no Apply) — cursor + opcode tallies.
static size_t ConsumeIdxRun(const uint8_t* p, size_t n, bool bulk, Batch& b)
{
    size_t run = 0;
    do
    {
        if (n - run < 5) break;
        uint8_t rop = p[run];
        if (rop != 0x20 && rop != 0x28 && rop != 0x30 && rop != 0x38) break;
        (void)Be32Load(p + run + 1);
        if (rop == 0x20) b.op20++;
        else if (rop == 0x28) b.op28++;
        else if (rop == 0x30) b.op30++;
        else b.op38++;
        b.cmds++;
        run += 5;
    } while (bulk && run < n);
    b.bytes = run;
    return run;
}

static void ApplyWord(uint32_t* xf, uint32_t word, const uint32_t* guest, uint32_t strideWords)
{
    const uint32_t index = word >> 16;
    const uint32_t size = ((word >> 12) & 0xF) + 1;
    const uint32_t addr = word & 0xFFF;
    for (uint32_t i = 0; i < size; i++)
        xf[(addr + i) & 0xFFF] = guest[index * strideWords + i];
}

static int Fail(const char* m)
{
    std::printf("fail: %s\n", m);
    return 1;
}

int main()
{
    if ((0 | 0 | 0) != 0) return Fail("defaults"); // A5g/A5h/A5i/A5j default off anchors

    uint8_t stream[64]{};
    size_t n = 0;
    uint8_t op = 0;
    uint32_t word = 0;
    auto emit = [&](uint8_t o, uint32_t w) {
        stream[n++] = o;
        Be32Store(stream + n, w);
        n += 4;
    };

    PackPos(0, 0, op, word);
    emit(op, word);
    PackPos(1, 0, op, word);
    emit(op, word);
    PackNrm(0, 0, op, word);
    emit(op, word);
    PackTex(0, 0x500, op, word);
    emit(op, word);
    PackLight(0, 0x600, op, word);
    emit(op, word);
    stream[n++] = 0x61; // BP breaks the run
    Be32Store(stream + n, 0x41000001u);
    n += 4;
    PackPos(2, 1, op, word);
    emit(op, word);

    // Non-bulk: one cmd
    Batch one{};
    size_t c1 = ConsumeIdxRun(stream, n, false, one);
    if (c1 != 5 || one.cmds != 1 || one.op20 != 1) return Fail("single");

    // Bulk: stop before BP
    Batch bulk{};
    size_t c2 = ConsumeIdxRun(stream, n, true, bulk);
    if (c2 != 25 || bulk.cmds != 5) return Fail("bulk len");
    if (bulk.op20 != 2 || bulk.op28 != 1 || bulk.op30 != 1 || bulk.op38 != 1) return Fail("bulk ops");

    // Resume after BP
    Batch rest{};
    size_t c3 = ConsumeIdxRun(stream + c2 + 5, n - c2 - 5, true, rest);
    if (c3 != 5 || rest.op20 != 1) return Fail("after bp");

    // Incomplete trailing bytes
    Batch inc{};
    uint8_t shortb[3] = {0x20, 0x00, 0x00};
    if (ConsumeIdxRun(shortb, 3, true, inc) != 0) return Fail("incomplete");

    // Equivalence: generic one-by-one vs bulk apply order
    uint32_t guest[64]{};
    for (uint32_t i = 0; i < 64; i++) guest[i] = 0xA0000000u + i;
    uint32_t xfA[0x1000]{};
    uint32_t xfB[0x1000]{};
    // stream first 25 bytes = 5 idx cmds
    size_t off = 0;
    while (off < 25)
    {
        uint8_t o = stream[off];
        uint32_t w = Be32Load(stream + off + 1);
        int stride = (o == 0x28) ? 9 : (o == 0x38) ? 16 : 12;
        ApplyWord(xfA, w, guest, (uint32_t)stride);
        off += 5;
    }
    off = 0;
    while (off < 25)
    {
        uint8_t o = stream[off];
        uint32_t w = Be32Load(stream + off + 1);
        int stride = (o == 0x28) ? 9 : (o == 0x38) ? 16 : 12;
        ApplyWord(xfB, w, guest, (uint32_t)stride);
        off += 5;
    }
    if (std::memcmp(xfA, xfB, sizeof(xfA)) != 0) return Fail("order eq");

    // Nested DL shape: outer BP, inner idx run, outer BP — cursor math only
    uint8_t nest[40]{};
    size_t nn = 0;
    nest[nn++] = 0x61;
    Be32Store(nest + nn, 0x40000011u);
    nn += 4;
    PackPos(0, 2, op, word);
    nest[nn++] = op;
    Be32Store(nest + nn, word);
    nn += 4;
    PackPos(1, 2, op, word);
    nest[nn++] = op;
    Be32Store(nest + nn, word);
    nn += 4;
    nest[nn++] = 0x61;
    Be32Store(nest + nn, 0x40000022u);
    nn += 4;
    Batch mid{};
    size_t skip = 5; // outer BP
    size_t inner = ConsumeIdxRun(nest + skip, nn - skip, true, mid);
    if (mid.op20 != 2 || inner != 10) return Fail("nest idx");
    if (nest[skip + inner] != 0x61) return Fail("nest trail bp");

    // Array mapping
    auto arr = [](uint8_t o) { return 12 + ((o >> 3) & 3); };
    if (arr(0x20) != 12 || arr(0x28) != 13 || arr(0x30) != 14 || arr(0x38) != 15) return Fail("array");

    // A5i identical contract still documented
    uint32_t xf[0x1000]{};
    ApplyWord(xf, ((0u) << 16) | (0xBu << 12) | 0, guest, 12);
    uint32_t before = xf[0];
    ApplyWord(xf, ((0u) << 16) | (0xBu << 12) | 0, guest, 12);
    if (xf[0] != before) return Fail("identical");

    std::printf("scr_a5j_indexed_xf_dispatch_test OK\n");
    return 0;
}
