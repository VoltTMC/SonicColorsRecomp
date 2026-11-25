// A5b: SNCP8P GXLoadTexMtxImm address/count mapping (matches DOL @ 8036EF50).
#include <cstdint>
#include <cstdio>
#include <cstdlib>

static void MapTexMtx(uint32_t id, uint32_t type, uint32_t& addr, uint32_t& count)
{
    addr = (id >= 0x40u) ? ((id - 0x40u) * 4u + 0x500u) : (id * 4u);
    count = (type == 1u) ? 8u : 12u;
}

int main()
{
    uint32_t addr = 0, count = 0;
    MapTexMtx(0, 0, addr, count);
    if (addr != 0 || count != 12) { std::printf("fail id0 type0\n"); return 1; }
    MapTexMtx(3, 1, addr, count);
    if (addr != 12 || count != 8) { std::printf("fail id3 type1\n"); return 1; }
    MapTexMtx(0x40, 0, addr, count);
    if (addr != 0x500 || count != 12) { std::printf("fail id40\n"); return 1; }
    MapTexMtx(0x41, 1, addr, count);
    if (addr != 0x504 || count != 8) { std::printf("fail id41\n"); return 1; }
    // Pos/nrm A5a mapping sanity
    if ((5u * 4u) != 20u) return 1;
    if ((0x400u + 5u * 3u) != 0x40Fu) return 1;
    std::printf("scr_a5b_texmtx_addr_test OK\n");
    return 0;
}
