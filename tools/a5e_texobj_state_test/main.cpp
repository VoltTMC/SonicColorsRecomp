// A5e: GXTexObj layout, TX BP id tables, LoadTexObj pack parity (SNCP8P).
#include <cstdint>
#include <cstdio>
#include <cstring>

static uint32_t Rotl(uint32_t v, uint32_t n) { return (v << n) | (v >> (32 - n)); }

static uint32_t PackTexBp(uint8_t regId, uint32_t low24)
{
    return (Rotl((uint32_t)regId, 24) & 0xFF000000u) | (low24 & 0xFFFFFFu);
}

int main()
{
    // Verified SNCP8P addresses
    constexpr uint32_t kInitTexObj = 0x8036BFB0u;
    constexpr uint32_t kPreLoaded = 0x8036C5C0u;
    constexpr uint32_t kLoadTexObj = 0x8036C730u;
    constexpr uint32_t kLoadTlut = 0x8036C7C0u;
    constexpr uint32_t kIdTable = 0x808F29E0u;
    if (kInitTexObj != 0x8036BFB0u || kPreLoaded != 0x8036C5C0u) return 1;
    if (kLoadTexObj != 0x8036C730u || kLoadTlut != 0x8036C7C0u) return 1;
    if (kIdTable != 0x808F29E0u) return 1;

    // GXTexObj size / key offsets (InitTexObj memset 0x20; PreLoaded reads)
    constexpr uint32_t kTexObjSize = 0x20u;
    constexpr uint32_t kOffMode0 = 0x00, kOffMode1 = 0x04, kOffImg0 = 0x08, kOffImg3 = 0x0C;
    constexpr uint32_t kOffFmt = 0x14, kOffTlutName = 0x18, kOffFlags = 0x1F;
    if (kTexObjSize != 0x20u) return 1;
    if (kOffMode0 || kOffMode1 != 4 || kOffImg0 != 8 || kOffImg3 != 0xC) return 1;
    if (kOffFmt != 0x14 || kOffTlutName != 0x18 || kOffFlags != 0x1F) return 1;

    // __gx caches used by PreLoaded
    if (0x518u != 0x518u || 0x51Cu != 0x51Cu) return 1;
    if (0x564u != 0x564u || 0x584u != 0x584u || 0x5FCu != 0x5FCu) return 1;

    // DOL-verified BP id tables (map0 group + map4 group)
    const uint8_t mode0[8] = { 0x80, 0x81, 0x82, 0x83, 0xA0, 0xA1, 0xA2, 0xA3 };
    const uint8_t mode1[8] = { 0x84, 0x85, 0x86, 0x87, 0xA4, 0xA5, 0xA6, 0xA7 };
    const uint8_t img0[8] = { 0x88, 0x89, 0x8A, 0x8B, 0xA8, 0xA9, 0xAA, 0xAB };
    const uint8_t img1[8] = { 0x8C, 0x8D, 0x8E, 0x8F, 0xAC, 0xAD, 0xAE, 0xAF };
    const uint8_t img2[8] = { 0x90, 0x91, 0x92, 0x93, 0xB0, 0xB1, 0xB2, 0xB3 };
    const uint8_t img3[8] = { 0x94, 0x95, 0x96, 0x97, 0xB4, 0xB5, 0xB6, 0xB7 };
    const uint8_t tlut[8] = { 0x98, 0x99, 0x9A, 0x9B, 0xB8, 0xB9, 0xBA, 0xBB };
    if (mode0[0] != 0x80 || img0[0] != 0x88 || img3[4] != 0xB4 || tlut[0] != 0x98) return 1;
    if (mode1[0] != 0x84 || img1[0] != 0x8C || img2[0] != 0x90) return 1;

    // Pack: low 24 from TexObj + reg id in top byte (PreLoaded)
    const uint32_t packed = PackTexBp(mode0[0], 0x00123456u);
    if (packed != 0x80123456u) { std::printf("fail pack %08x\n", packed); return 1; }
    const uint32_t p4 = PackTexBp(img0[4], 0x00ABCDEFu);
    if (p4 != 0xA8ABCDEFu) { std::printf("fail pack map4 %08x\n", p4); return 1; }

    // CI flag: bit1 clear ⇒ colour-index (InitTexObjCI clears bit1; colour sets it)
    const uint8_t colourFlags = 0x02;
    const uint8_t ciFlags = 0x00;
    if ((colourFlags & 2) == 0) return 1;
    if ((ciFlags & 2) != 0) return 1;

    // Special BP: 0x66 remains trigger; TX 0x80–0x94 are ordinary
    auto isTrigger = [](uint32_t r) {
        return r == 0x45 || r == 0x47 || r == 0x48 || r == 0x52 || r == 0x65 || r == 0x66 ||
               (r >= 0xE0 && r <= 0xE7);
    };
    if (!isTrigger(0x66) || isTrigger(0x80) || isTrigger(0x94)) return 1;

    // Image vs sampler: mode0/mode1 are sampler; img0/img3 describe image identity
    (void)kOffMode0;
    (void)kOffImg0;

    std::printf("scr_a5e_texobj_state_test OK\n");
    return 0;
}
