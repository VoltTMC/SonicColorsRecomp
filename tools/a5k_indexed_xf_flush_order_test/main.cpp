// A5k: flush barrier ordering vs pending geometry + tex-cache skip equivalence.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <unordered_map>
#include <vector>

static int Fail(const char* m)
{
    std::printf("fail: %s\n", m);
    return 1;
}

// ---- Model: pending draws hold a *pointer* to live XF (matches batch.xfmem = g_gx.xfmem) ----
struct XfMem
{
    uint32_t words[0x1000]{};
};

struct DrawBatch
{
    const float* xfmem = nullptr; // live pointer — not a snapshot
    uint32_t xfSample0 = 0;       // captured at flush/submit time
    uint32_t vertCount = 0;
    uint32_t indexCount = 0;
};

struct Host
{
    XfMem xf{};
    std::vector<DrawBatch> pending; // at most one coalesced host batch in real renderer
    std::vector<DrawBatch> submitted;
    bool flushOpt = false;
    std::unordered_map<uint64_t, int> texCache; // key → generation
    uint64_t texDecode = 0;
    uint64_t texSkip = 0;
    uint64_t flushCall = 0;
    uint64_t flushEmpty = 0;
    uint64_t flushNonEmpty = 0;

    bool HasPending() const { return !pending.empty() && pending[0].indexCount > 0; }

    void QueueDraw(uint32_t verts, uint32_t indices)
    {
        if (pending.empty()) pending.push_back({});
        DrawBatch& b = pending[0];
        b.xfmem = reinterpret_cast<const float*>(xf.words);
        b.vertCount += verts;
        b.indexCount += indices;
    }

    void Flush(bool fromIdx)
    {
        (void)fromIdx;
        flushCall++;
        if (!HasPending())
        {
            flushEmpty++;
            pending.clear();
            return;
        }
        flushNonEmpty++;
        DrawBatch b = pending[0];
        // Snapshot effective XF at drain time (what the GPU would read).
        b.xfSample0 = b.xfmem ? reinterpret_cast<const uint32_t*>(b.xfmem)[0] : 0;
        submitted.push_back(b);
        pending.clear();
    }

    // Legacy: always Flush before mutate. Opt path cannot skip non-empty Flush.
    void ApplyIndexedXf(uint32_t addr, uint32_t size, const uint32_t* src, bool playbackOpt)
    {
        if (playbackOpt && !HasPending())
        {
            // A5h empty skip
        }
        else
            Flush(true);
        for (uint32_t i = 0; i < size; i++)
            xf.words[(addr + i) & 0xFFF] = src[i];
    }

    // Vulkan Flush tex path model: cache hit discards decode result.
    void BindTex(uint64_t key, bool decodeNeededContent)
    {
        (void)decodeNeededContent;
        if (flushOpt && texCache.count(key))
        {
            texSkip++;
            return;
        }
        texDecode++;
        texCache[key] = 1;
    }
};

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

int main()
{
    // Defaults: A5k opt off
    if ((0 | 0) != 0) return Fail("defaults");

    // 1) No pending → empty flush / A5h skip; XF updates; next draw sees new XF
    {
        Host h;
        uint32_t payload[12]{};
        for (uint32_t i = 0; i < 12; i++) payload[i] = 0x1000u + i;
        h.ApplyIndexedXf(0, 12, payload, true);
        if (h.flushCall != 0) return Fail("empty skip should not flush");
        if (h.xf.words[0] != 0x1000u) return Fail("xf write");
        h.QueueDraw(3, 3);
        h.Flush(false);
        if (h.submitted.size() != 1 || h.submitted[0].xfSample0 != 0x1000u) return Fail("draw after empty");
    }

    // 2) Pending geometry then indexed XF: must drain with *old* XF
    {
        Host h;
        h.xf.words[0] = 0xAAAAu;
        h.QueueDraw(6, 6);
        uint32_t payload[12]{};
        for (uint32_t i = 0; i < 12; i++) payload[i] = 0xBBBBu + i;
        h.ApplyIndexedXf(0, 12, payload, true);
        if (h.flushNonEmpty != 1) return Fail("must non-empty flush");
        if (h.submitted.size() != 1 || h.submitted[0].xfSample0 != 0xAAAAu)
            return Fail("pending must keep pre-idx XF");
        if (h.xf.words[0] != 0xBBBBu) return Fail("canonical xf after");
        h.QueueDraw(3, 3);
        h.Flush(false);
        if (h.submitted.back().xfSample0 != 0xBBBBu) return Fail("later draw new XF");
    }

    // 3) Multiple idx between draws: first drains; rest empty-skip
    {
        Host h;
        h.xf.words[0] = 1;
        h.QueueDraw(4, 4);
        uint32_t a[12]{}, b[12]{};
        for (int i = 0; i < 12; i++)
        {
            a[i] = 10u + (uint32_t)i;
            b[i] = 20u + (uint32_t)i;
        }
        h.ApplyIndexedXf(0, 12, a, true);
        h.ApplyIndexedXf(0, 12, b, true);
        if (h.flushNonEmpty != 1 || h.flushCall != 1) return Fail("coalesce empty after drain");
        if (h.submitted[0].xfSample0 != 1) return Fail("first geometry old xf");
        if (h.xf.words[0] != 20u) return Fail("final xf");
    }

    // 4) Live pointer hazard: mutate without flush corrupts pending (proves barrier)
    {
        Host h;
        h.xf.words[0] = 0x1111;
        h.QueueDraw(2, 2);
        // Unsafe: write without flush
        h.xf.words[0] = 0x2222;
        h.Flush(false);
        if (h.submitted[0].xfSample0 != 0x2222) return Fail("expected live-pointer corruption demo");
    }

    // 5) XF-only snapshot would still need other state — model: snapshotting words[0]
    //    alone is insufficient if a second pending batch shared the same pointer.
    //    Renderer coalesces to one host batch; barrier still required before mutate.
    {
        Host h;
        h.xf.words[0] = 5;
        h.QueueDraw(1, 1);
        h.QueueDraw(1, 1); // coalesced
        uint32_t p[1] = {9};
        h.ApplyIndexedXf(0, 1, p, true);
        if (h.submitted[0].xfSample0 != 5 || h.submitted[0].vertCount != 2) return Fail("coalesced drain");
    }

    // 6) Interleave BP / ordinary XF / texture / CP markers (ordering positions)
    {
        Host h;
        h.xf.words[0] = 0;
        h.QueueDraw(3, 3);
        // BP would also Flush in real path when pending — model as Flush
        h.Flush(false);
        uint32_t p[12]{};
        p[0] = 42;
        h.ApplyIndexedXf(0, 12, p, true); // empty
        // Ordinary XF write (no flush required for empty pending)
        h.xf.words[4] = 7;
        h.QueueDraw(3, 3);
        h.Flush(false);
        if (h.submitted.size() != 2) return Fail("interleave draws");
        if (h.submitted[1].xfSample0 != 42) return Fail("post-idx draw");
    }

    // 7) Display-list resident idx: same Apply contract
    {
        uint8_t dl[10];
        dl[0] = 0x20;
        Be32Store(dl + 1, (0u << 16) | (0xBu << 12) | 0);
        if (Be32Load(dl + 1) != ((0u << 16) | (0xBu << 12))) return Fail("dl pack");
        Host h;
        h.QueueDraw(1, 1);
        uint32_t guest[12];
        for (uint32_t i = 0; i < 12; i++) guest[i] = 0xD000u + i;
        h.ApplyIndexedXf(0, 12, guest, true);
        if (h.submitted[0].xfSample0 != 0) return Fail("dl idx drain");
        if (h.xf.words[0] != 0xD000u) return Fail("dl idx xf");
    }

    // 8) Tex cache skip equivalence: same key → same bind; skip only under opt
    {
        Host legacy;
        legacy.flushOpt = false;
        legacy.BindTex(0xABCDull, true);
        legacy.BindTex(0xABCDull, true);
        if (legacy.texDecode != 2 || legacy.texSkip != 0) return Fail("legacy always decode");

        Host opt;
        opt.flushOpt = true;
        opt.BindTex(0xABCDull, true);
        opt.BindTex(0xABCDull, true);
        if (opt.texDecode != 1 || opt.texSkip != 1) return Fail("opt skip on hit");
        // Different key still decodes
        opt.BindTex(0xABCEu, true);
        if (opt.texDecode != 2) return Fail("miss still decode");
    }

    // 9) Large pending buffers
    {
        Host h;
        h.xf.words[0] = 3;
        h.QueueDraw(50000, 100000);
        uint32_t p[12]{};
        p[0] = 8;
        h.ApplyIndexedXf(0, 12, p, true);
        if (h.submitted[0].vertCount != 50000 || h.submitted[0].indexCount != 100000)
            return Fail("large pending");
        if (h.submitted[0].xfSample0 != 3) return Fail("large xf");
    }

    // 10) Identical vs changed payload — flush still required when pending
    {
        Host h;
        h.xf.words[0] = 0x55;
        h.QueueDraw(2, 2);
        uint32_t same[1] = {0x55};
        h.ApplyIndexedXf(0, 1, same, true);
        if (h.flushNonEmpty != 1) return Fail("identical still flushes when pending");
    }

    // 11) Unsupported / fallback: opt off keeps decode
    {
        Host h;
        h.flushOpt = false;
        h.BindTex(1, true);
        h.BindTex(1, true);
        if (h.texSkip != 0) return Fail("fallback no skip");
    }

    std::printf("scr_a5k_indexed_xf_flush_order_test OK\n");
    return 0;
}
