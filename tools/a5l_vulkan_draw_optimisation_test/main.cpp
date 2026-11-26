// A5l: sticky in-CB descriptor reuse + bind-skip semantics (no Vulkan required).
#include <cstdint>
#include <cstdio>
#include <cstring>

static int Fail(const char* m)
{
    std::printf("fail: %s\n", m);
    return 1;
}

struct DescKey
{
    uint64_t xfOff = 0;
    uint64_t views[8]{};
    uint64_t samps[8]{};
    bool operator==(const DescKey& o) const
    {
        if (xfOff != o.xfOff) return false;
        for (int i = 0; i < 8; i++)
            if (views[i] != o.views[i] || samps[i] != o.samps[i]) return false;
        return true;
    }
};

struct Ubo
{
    uint32_t vs = 0, ps = 0;
};

struct Sticky
{
    bool valid = false;
    DescKey key{};
    int setId = -1;
    uint32_t dyn[2]{};
    Ubo ubo{};
};

struct Stats
{
    uint64_t full = 0, partial = 0, miss = 0, inval = 0;
    uint64_t pipeSkip = 0, vpSkip = 0;
};

struct Model
{
    bool opt = false;
    Sticky sticky{};
    Stats st{};
    int nextSet = 1;
    uint32_t uboPos = 0;
    uint64_t boundPipe = 0;
    bool haveVp = false;
    float vpX = 0;

    void Invalidate()
    {
        sticky = {};
        boundPipe = 0;
        haveVp = false;
        st.inval++;
    }

    int Alloc(const DescKey& key, const Ubo& ubo)
    {
        if (opt && sticky.valid && sticky.key == key && sticky.ubo.vs == ubo.vs && sticky.ubo.ps == ubo.ps)
        {
            st.full++;
            return sticky.setId;
        }
        uboPos += 2; // new dynamic slots
        if (opt && sticky.valid && sticky.key == key)
        {
            sticky.ubo = ubo;
            sticky.dyn[0] = uboPos - 2;
            sticky.dyn[1] = uboPos - 1;
            st.partial++;
            return sticky.setId;
        }
        const int id = nextSet++;
        sticky.valid = opt;
        sticky.key = key;
        sticky.setId = id;
        sticky.ubo = ubo;
        sticky.dyn[0] = uboPos - 2;
        sticky.dyn[1] = uboPos - 1;
        st.miss++;
        return id;
    }

    void BindPipe(uint64_t pipe)
    {
        if (opt && pipe == boundPipe)
        {
            st.pipeSkip++;
            return;
        }
        boundPipe = pipe;
    }

    void BindVp(float x)
    {
        if (opt && haveVp && x == vpX)
        {
            st.vpSkip++;
            return;
        }
        vpX = x;
        haveVp = true;
    }
};

int main()
{
    // Defaults: opt off
    {
        Model m;
        DescKey k{};
        k.views[0] = 1;
        Ubo u{1, 2};
        if (m.Alloc(k, u) != 1 || m.Alloc(k, u) != 2) return Fail("legacy always new set");
        if (m.st.miss != 2 || m.st.full != 0) return Fail("legacy miss");
    }

    // Full sticky: identical key+ubo
    {
        Model m;
        m.opt = true;
        DescKey k{};
        k.xfOff = 16;
        k.views[0] = 42;
        Ubo u{3, 4};
        int a = m.Alloc(k, u);
        int b = m.Alloc(k, u);
        if (a != b || m.st.full != 1 || m.st.miss != 1) return Fail("full sticky");
    }

    // Partial sticky: same key, changed UBO
    {
        Model m;
        m.opt = true;
        DescKey k{};
        k.views[0] = 7;
        int a = m.Alloc(k, {1, 1});
        int b = m.Alloc(k, {2, 2});
        if (a != b || m.st.partial != 1 || m.st.miss != 1) return Fail("partial sticky");
    }

    // Texture change → miss
    {
        Model m;
        m.opt = true;
        DescKey k1{}, k2{};
        k1.views[0] = 1;
        k2.views[0] = 2;
        int a = m.Alloc(k1, {1, 1});
        int b = m.Alloc(k2, {1, 1});
        if (a == b || m.st.miss != 2) return Fail("tex change");
    }

    // XF offset change → miss
    {
        Model m;
        m.opt = true;
        DescKey k1{}, k2{};
        k1.xfOff = 0;
        k2.xfOff = 16384;
        if (m.Alloc(k1, {0, 0}) == m.Alloc(k2, {0, 0})) return Fail("xf change");
    }

    // Invalidate (frame reset / EFB retire) clears sticky
    {
        Model m;
        m.opt = true;
        DescKey k{};
        k.views[0] = 9;
        int a = m.Alloc(k, {5, 5});
        m.Invalidate();
        int b = m.Alloc(k, {5, 5});
        if (a == b || m.st.full != 0 || m.st.miss != 2) return Fail("invalidate");
    }

    // Bind skips
    {
        Model m;
        m.opt = true;
        m.BindPipe(11);
        m.BindPipe(11);
        m.BindVp(1.0f);
        m.BindVp(1.0f);
        m.BindVp(2.0f);
        if (m.st.pipeSkip != 1 || m.st.vpSkip != 1) return Fail("bind skip");
        m.Invalidate();
        m.BindPipe(11);
        if (m.st.pipeSkip != 1) return Fail("bind skip after inval");
    }

    // Shader-key / pipeline identity: different hash → no skip
    {
        Model m;
        m.opt = true;
        m.BindPipe(1);
        m.BindPipe(2);
        if (m.st.pipeSkip != 0) return Fail("pipe change");
    }

    // Order: draw A (tex1), draw B (tex2), draw A again → miss then miss then miss or sticky only within identical sequence
    {
        Model m;
        m.opt = true;
        DescKey a{}, b{};
        a.views[0] = 1;
        b.views[0] = 2;
        m.Alloc(a, {1, 1});
        m.Alloc(b, {1, 1});
        m.Alloc(a, {1, 1});
        if (m.st.full != 0 || m.st.miss != 3) return Fail("alternating tex");
    }

    // Repeated identical after partial still full-hits
    {
        Model m;
        m.opt = true;
        DescKey k{};
        k.views[0] = 3;
        m.Alloc(k, {1, 1});
        m.Alloc(k, {2, 2}); // partial
        m.Alloc(k, {2, 2}); // full
        if (m.st.partial != 1 || m.st.full != 1) return Fail("partial then full");
    }

    std::printf("scr_a5l_vulkan_draw_optimisation_test OK\n");
    return 0;
}
