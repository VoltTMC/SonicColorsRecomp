// A5n: XF dirty-range exact compare + generation / slot lifetime model.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static int Fail(const char* m)
{
    std::printf("fail: %s\n", m);
    return 1;
}

static uint64_t Hash64(const void* data, size_t n, uint64_t seed = 0x9E3779B97F4A7C15ull)
{
    const uint8_t* p = (const uint8_t*)data;
    const uint64_t M = 0x9FB21C651E98DF25ull;
    uint64_t h = seed ^ (n * 0xff51afd7ed558ccdull);
    for (size_t i = 0; i < n; i++)
    {
        h ^= p[i];
        h *= M;
        h ^= h >> 29;
    }
    return h;
}

struct XfModel
{
    static constexpr size_t kFloatN = 0x680;
    static constexpr size_t kSlotBytes = 16u * 1024u;
    static constexpr size_t kPayload = kFloatN * sizeof(float);

    std::vector<float> xfmem;
    bool dirty = true;
    uint32_t dirtyBegin = 0;
    uint32_t dirtyEnd = 0; // 0/0 ⇒ full
    uint64_t generation = 1;

    std::vector<uint8_t> ring;
    size_t ringPos = 0;
    bool uploaded = false;
    size_t lastOff = 0;
    uint32_t lastBytes = 0;
    uint64_t lastHash = 0;
    uint64_t lastGen = 0;

    bool opt = false;
    uint64_t hashCalls = 0, hashBytes = 0;
    uint64_t memcmpCalls = 0, memcmpBytes = 0;
    uint64_t uploads = 0, dedupHits = 0;
    uint64_t drHits = 0, drMisses = 0, fullFb = 0;
    uint64_t cleanSkips = 0;
    uint64_t bytesUploaded = 0;

    explicit XfModel(bool o) : xfmem(kFloatN, 0.f), ring(16u * 1024u * 1024u), opt(o) {}

    void MarkDirty(uint32_t addr, uint32_t count)
    {
        dirty = true;
        generation++;
        if (addr >= 0x680u) return;
        uint32_t end = addr + count;
        if (end > 0x680u) end = 0x680u;
        if (dirtyEnd == 0 && dirtyBegin == 0)
        {
            dirtyBegin = addr;
            dirtyEnd = end;
            return;
        }
        if (addr < dirtyBegin) dirtyBegin = addr;
        if (end > dirtyEnd) dirtyEnd = end;
    }

    void FullInvalidate()
    {
        dirty = true;
        dirtyBegin = 0;
        dirtyEnd = 0x680u;
        generation++;
    }

    void WriteRange(uint32_t addr, uint32_t count, float v)
    {
        for (uint32_t i = 0; i < count && addr + i < kFloatN; i++)
            xfmem[addr + i] = v;
        MarkDirty(addr, count);
    }

    void WriteIdentical(uint32_t addr, uint32_t count)
    {
        // Re-write same values but still mark dirty (no A5i skip).
        MarkDirty(addr, count);
    }

    void ResetRing()
    {
        ringPos = 0;
        uploaded = false;
        lastOff = 0;
        lastBytes = 0;
        lastHash = 0;
        lastGen = 0;
    }

    bool Upload()
    {
        if (!dirty && uploaded)
        {
            cleanSkips++;
            return true;
        }
        const size_t n = kPayload;
        if (uploaded && lastBytes == (uint32_t)n)
        {
            const bool rangeOk = dirtyEnd > dirtyBegin && dirtyEnd <= kFloatN;
            if (opt && rangeOk)
            {
                const size_t offB = (size_t)dirtyBegin * sizeof(float);
                const size_t nb = (size_t)(dirtyEnd - dirtyBegin) * sizeof(float);
                memcmpCalls++;
                memcmpBytes += nb;
                if (std::memcmp((const uint8_t*)xfmem.data() + offB, ring.data() + lastOff + offB, nb) != 0)
                {
                    drMisses++;
                    // fall through to upload without pre-hash
                }
                else
                {
                    memcmpCalls++;
                    memcmpBytes += n;
                    if (std::memcmp(xfmem.data(), ring.data() + lastOff, n) == 0)
                    {
                        dedupHits++;
                        drHits++;
                        lastGen = generation;
                        dirty = false;
                        dirtyBegin = dirtyEnd = 0;
                        return true;
                    }
                    drMisses++;
                    fullFb++;
                }
            }
            else if (opt)
            {
                fullFb++;
                memcmpCalls++;
                memcmpBytes += n;
                if (std::memcmp(xfmem.data(), ring.data() + lastOff, n) == 0)
                {
                    dedupHits++;
                    lastGen = generation;
                    dirty = false;
                    dirtyBegin = dirtyEnd = 0;
                    return true;
                }
            }
            else
            {
                hashCalls++;
                hashBytes += n;
                const uint64_t h = Hash64(xfmem.data(), n);
                if (h == lastHash)
                {
                    memcmpCalls++;
                    memcmpBytes += n;
                    if (std::memcmp(xfmem.data(), ring.data() + lastOff, n) == 0)
                    {
                        dedupHits++;
                        lastGen = generation;
                        dirty = false;
                        dirtyBegin = dirtyEnd = 0;
                        return true;
                    }
                }
            }
        }
        size_t off = (ringPos + kSlotBytes - 1) / kSlotBytes * kSlotBytes;
        if (off + kSlotBytes > ring.size()) return false;
        std::memcpy(ring.data() + off, xfmem.data(), n);
        if (n < kSlotBytes) std::memset(ring.data() + off + n, 0, kSlotBytes - n);
        if (!opt)
        {
            hashCalls++;
            hashBytes += n;
            lastHash = Hash64(ring.data() + off, n);
        }
        else
            lastHash = 0;
        lastOff = off;
        ringPos = off + kSlotBytes;
        lastBytes = (uint32_t)n;
        lastGen = generation;
        uploaded = true;
        uploads++;
        bytesUploaded += n;
        dirty = false;
        dirtyBegin = dirtyEnd = 0;
        return true;
    }

    bool SlotMatchesCanonical() const
    {
        if (!uploaded) return false;
        return std::memcmp(xfmem.data(), ring.data() + lastOff, kPayload) == 0;
    }
};

int main()
{
    // 1. Initial full upload
    {
        XfModel m(true);
        m.FullInvalidate();
        if (!m.Upload()) return Fail("initial upload");
        if (m.uploads != 1) return Fail("initial upload count");
        if (!m.SlotMatchesCanonical()) return Fail("initial slot contents");
    }

    // 2. Repeated identical XF with dirty mark → dirty-range hit (no upload)
    {
        XfModel m(true);
        m.FullInvalidate();
        m.Upload();
        const uint64_t up0 = m.uploads;
        const uint64_t h0 = m.hashCalls;
        m.WriteIdentical(0, 12);
        if (!m.Upload()) return Fail("identical dirty upload");
        if (m.uploads != up0) return Fail("identical should not upload");
        if (m.drHits != 1) return Fail("expected dirty-range hit");
        if (m.hashCalls != h0) return Fail("identical should not hash");
        if (!m.SlotMatchesCanonical()) return Fail("identical slot");
    }

    // 3. Single-range change → miss + upload
    {
        XfModel m(true);
        m.FullInvalidate();
        m.Upload();
        m.WriteRange(16, 12, 3.5f);
        if (!m.Upload()) return Fail("range change upload");
        if (m.drMisses != 1) return Fail("expected dirty-range miss");
        if (m.uploads != 2) return Fail("range change upload count");
        if (!m.SlotMatchesCanonical()) return Fail("range change slot");
    }

    // 4. Multiple ranges union before draw
    {
        XfModel m(true);
        m.FullInvalidate();
        m.Upload();
        m.WriteRange(0, 4, 1.f);
        m.WriteRange(100, 4, 2.f);
        if (m.dirtyBegin != 0 || m.dirtyEnd != 104) return Fail("dirty union");
        if (!m.Upload()) return Fail("multi-range upload");
        if (!m.SlotMatchesCanonical()) return Fail("multi-range slot");
    }

    // 5. Same range rewritten before draw — one upload
    {
        XfModel m(true);
        m.FullInvalidate();
        m.Upload();
        m.WriteRange(8, 4, 1.f);
        m.WriteRange(8, 4, 2.f);
        const uint64_t up0 = m.uploads;
        if (!m.Upload()) return Fail("same-range upload");
        if (m.uploads != up0 + 1) return Fail("same-range single upload");
        if (m.xfmem[8] != 2.f) return Fail("same-range last write");
        if (!m.SlotMatchesCanonical()) return Fail("same-range slot");
    }

    // 6. Clean skip when !dirty
    {
        XfModel m(true);
        m.FullInvalidate();
        m.Upload();
        if (!m.Upload()) return Fail("clean skip");
        if (m.cleanSkips != 1) return Fail("expected clean skip");
        if (m.uploads != 1) return Fail("clean skip must not upload");
    }

    // 7. Full invalidate (0..0x680) uses full compare fallback under opt when range is full —
    //    still exact-safe; generation bumps.
    {
        XfModel m(true);
        m.FullInvalidate();
        m.Upload();
        const uint64_t gen0 = m.generation;
        m.FullInvalidate();
        if (m.generation <= gen0) return Fail("generation bump on full invalidate");
        if (!m.Upload()) return Fail("full invalidate upload");
        if (!m.SlotMatchesCanonical()) return Fail("full invalidate slot");
    }

    // 8. Opt off: hash+memcmp path; identical dirty still dedups
    {
        XfModel m(false);
        m.FullInvalidate();
        m.Upload();
        const uint64_t up0 = m.uploads;
        m.WriteIdentical(0, 12);
        if (!m.Upload()) return Fail("legacy identical");
        if (m.uploads != up0) return Fail("legacy dedup hit");
        if (m.drHits != 0) return Fail("legacy must not use dirty-range hit");
        if (m.hashCalls < 2) return Fail("legacy should hash");
    }

    // 9. Independent in-flight slots — second upload keeps prior bytes
    {
        XfModel m(true);
        m.FullInvalidate();
        m.Upload();
        const size_t off0 = m.lastOff;
        std::vector<uint8_t> snap(m.ring.begin() + (std::ptrdiff_t)off0,
                                  m.ring.begin() + (std::ptrdiff_t)off0 + (std::ptrdiff_t)XfModel::kPayload);
        m.WriteRange(0, 12, 9.f);
        if (!m.Upload()) return Fail("second slot");
        if (m.lastOff == off0) return Fail("second slot must be new offset");
        if (std::memcmp(m.ring.data() + off0, snap.data(), snap.size()) != 0)
            return Fail("prior slot overwritten");
        if (!m.SlotMatchesCanonical()) return Fail("new slot contents");
    }

    // 10. Frame reset invalidates reuse
    {
        XfModel m(true);
        m.FullInvalidate();
        m.Upload();
        m.ResetRing();
        m.dirty = true;
        m.dirtyBegin = 0;
        m.dirtyEnd = 0x680u;
        if (!m.Upload()) return Fail("after reset");
        if (m.uploads != 2) return Fail("reset forces upload");
        if (!m.SlotMatchesCanonical()) return Fail("after reset slot");
    }

    // 11. Hash collision safety: equal hash must still memcmp (legacy path)
    {
        XfModel m(false);
        m.FullInvalidate();
        m.Upload();
        // Force a dirty mark with changed bytes — must upload even if we only trusted hash.
        m.WriteRange(200, 4, 42.f);
        if (!m.Upload()) return Fail("hash safety upload");
        if (!m.SlotMatchesCanonical()) return Fail("hash safety contents");
    }

    // 12. Opt on with 0/0 dirty encoding → full fallback (ambiguous with high-addr-only)
    {
        XfModel m(true);
        m.FullInvalidate();
        m.Upload();
        m.dirty = true;
        m.dirtyBegin = 0;
        m.dirtyEnd = 0; // ambiguous
        m.generation++;
        // Change a float without updating range (simulates incomplete metadata) — full path
        // must still see a hash/memcmp miss or upload.
        m.xfmem[50] = 7.f;
        if (!m.Upload()) return Fail("0/0 fallback upload");
        if (m.fullFb < 1) return Fail("expected full compare fallback");
        if (!m.SlotMatchesCanonical()) return Fail("0/0 fallback contents");
    }

    std::printf("scr_a5n_vulkan_xf_upload_test: ok\n");
    return 0;
}
