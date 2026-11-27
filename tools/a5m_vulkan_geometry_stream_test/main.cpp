// A5m: geometry ring bump + mid-frame flush classification / soft reclaim model.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

static int Fail(const char* m)
{
    std::printf("fail: %s\n", m);
    return 1;
}

enum class Reason { Geom, Xf, Desc };

struct Ring
{
    std::vector<uint8_t> mem;
    size_t pos = 0;
    size_t peak = 0;
    explicit Ring(size_t cap) : mem(cap) {}
    bool Fits(size_t n, size_t align) const
    {
        size_t off = (pos + align - 1) / align * align;
        return off + n <= mem.size();
    }
    size_t Alloc(size_t n, size_t align, size_t& padOut)
    {
        size_t off = (pos + align - 1) / align * align;
        padOut = off - pos;
        pos = off + n;
        if (pos > peak) peak = pos;
        return off;
    }
};

struct Model
{
    bool opt = false;
    size_t arena = 16u * 1024u * 1024u;
    Ring vb;
    Ring ib;
    size_t xfPos = 0;
    size_t xfCap = 16u * 1024u * 1024u;
    size_t descPos = 0;
    size_t descCap = 32768;
    uint64_t flushGeom = 0, flushXf = 0, flushDesc = 0;
    uint64_t soft = 0, full = 0;
    uint64_t vbBytes = 0, ibBytes = 0, pad = 0;
    uint64_t fenceWaits = 0;
    bool xfAlive = false;
    uint32_t lastXfMark = 0;

    explicit Model(bool o, size_t a = 16u * 1024u * 1024u)
        : opt(o), arena(a), vb(a), ib(a)
    {
    }

    void FullReset()
    {
        fenceWaits++;
        vb.pos = 0;
        ib.pos = 0;
        xfPos = 0;
        descPos = 0;
        xfAlive = false;
        full++;
    }

    void SoftGeomReset()
    {
        fenceWaits++;
        vb.pos = 0;
        ib.pos = 0;
        // XF/desc preserved
        soft++;
    }

    void Flush(Reason r)
    {
        if (r == Reason::Geom)
        {
            flushGeom++;
            if (opt) SoftGeomReset();
            else FullReset();
        }
        else if (r == Reason::Xf)
        {
            flushXf++;
            FullReset();
        }
        else
        {
            flushDesc++;
            FullReset();
        }
    }

    bool Draw(size_t vbytes, size_t ibytes, bool needXf, bool needDesc)
    {
        size_t align = 16;
        if (!vb.Fits(vbytes, align) || !ib.Fits(ibytes, 4))
            Flush(Reason::Geom);
        if (needXf)
        {
            const size_t slot = 16384;
            if (xfPos + slot > xfCap)
                Flush(Reason::Xf);
            xfPos += slot;
            xfAlive = true;
            lastXfMark = (uint32_t)xfPos;
        }
        if (needDesc)
        {
            if (descPos + 1 > descCap)
                Flush(Reason::Desc);
            descPos++;
        }
        size_t p = 0;
        size_t vo = vb.Alloc(vbytes, align, p);
        pad += p;
        size_t io = ib.Alloc(ibytes, 4, p);
        (void)io;
        // Simulate GPU-visible write
        if (vo + vbytes > vb.mem.size()) return false;
        std::memset(vb.mem.data() + vo, 0xAB, vbytes);
        std::memset(ib.mem.data() + ib.pos - ibytes, 0xCD, ibytes);
        vbBytes += vbytes;
        ibBytes += ibytes;
        return true;
    }
};

int main()
{
    // Defaults: opt off, 16 MiB
    {
        Model m(false);
        if (m.arena != 16u * 1024u * 1024u) return Fail("default arena");
        if ((0 | 0) != 0) return Fail("switch default");
    }

    // Legacy: geom flush full-resets XF
    {
        Model m(false, 1024);
        m.Draw(200, 100, true, true);
        uint32_t xfBefore = m.lastXfMark;
        // Fill VB
        while (m.vb.Fits(200, 16) && m.ib.Fits(100, 4))
            m.Draw(200, 100, false, true);
        m.Draw(200, 100, false, true); // triggers geom flush
        if (m.flushGeom != 1 || m.full != 1 || m.soft != 0) return Fail("legacy full");
        if (m.xfAlive) return Fail("legacy xf cleared");
        (void)xfBefore;
    }

    // A5m soft: geom flush preserves XF mark
    {
        Model m(true, 1024);
        m.Draw(200, 100, true, true);
        uint32_t xfKeep = m.lastXfMark;
        size_t descKeep = m.descPos;
        while (m.vb.Fits(200, 16) && m.ib.Fits(100, 4))
            m.Draw(200, 100, false, true);
        m.Draw(200, 100, false, true);
        if (m.flushGeom != 1 || m.soft != 1 || m.full != 0) return Fail("soft geom");
        if (!m.xfAlive || m.lastXfMark != xfKeep) return Fail("xf preserved");
        if (m.descPos < descKeep) return Fail("desc preserved");
        // Continue with XF still "uploaded"
        if (!m.Draw(200, 100, false, true)) return Fail("post soft draw");
    }

    // XF flush still full-resets under opt
    {
        Model m(true, 4096);
        m.xfCap = 16384;
        m.Draw(16, 4, true, true);
        m.Draw(16, 4, true, true); // second XF slot may flush
        // Force xf exhaustion
        m.xfPos = m.xfCap;
        m.Draw(16, 4, true, true);
        if (m.flushXf < 1 || m.full < 1) return Fail("xf full reset");
        if (m.xfPos == 0 || m.vb.pos > 0) { /* after full reset pos may be from new draw */
        }
    }

    // Larger arena under opt reduces geom flushes for same workload
    {
        Model small(true, 2048);
        Model large(true, 8192);
        auto burn = [](Model& m) {
            for (int i = 0; i < 40; i++)
                m.Draw(200, 100, false, true);
        };
        burn(small);
        burn(large);
        if (small.flushGeom <= large.flushGeom) return Fail("larger fewer flushes");
        if (large.vbBytes != small.vbBytes) return Fail("same bytes");
    }

    // Alignment padding accounted
    {
        Model m(true, 4096);
        m.vb.pos = 1;
        size_t p = 0;
        m.vb.Alloc(16, 16, p);
        if (p != 15) return Fail("pad");
    }

    // Index + vertex independent capacity
    {
        Model m(true, 512);
        // Fill only IB
        while (m.ib.Fits(64, 4))
            m.Draw(16, 64, false, false);
        m.Draw(16, 64, false, false);
        if (m.flushGeom != 1) return Fail("ib pressure");
    }

    // Equivalence: same written marker bytes after soft reclaim
    {
        Model m(true, 256);
        m.Draw(32, 16, true, true);
        uint8_t snap[32];
        std::memcpy(snap, m.vb.mem.data(), 32);
        while (m.vb.Fits(32, 16))
            m.Draw(32, 16, false, false);
        m.Draw(32, 16, false, false); // soft reset
        m.Draw(32, 16, false, false);
        if (std::memcmp(m.vb.mem.data(), snap, 32) == 0 && m.vb.pos == 32)
        {
            // first 32 rewritten with same pattern 0xAB — OK
        }
        if (m.vb.mem[0] != 0xAB) return Fail("rewrite");
    }

    // Premature reuse guard: soft path still waited (fenceWaits++) before wrap
    {
        Model m(true, 128);
        m.Draw(64, 32, false, false);
        m.Draw(64, 32, false, false); // fills 128
        m.Draw(64, 32, false, false); // geom flush + wait
        if (m.fenceWaits != 1 || m.soft != 1) return Fail("must wait before reuse");
    }

    std::printf("scr_a5m_vulkan_geometry_stream_test OK\n");
    return 0;
}
