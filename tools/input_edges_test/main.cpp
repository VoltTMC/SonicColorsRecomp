// I1: short press/release must survive until guest publish even if level is already up.
#include "input/input_edges.h"
#include "input/gamepad.h"
#include "platform/input.h"

#include <cstdio>
#include <string>

// Stubs — edges test does not link the full platform/runtime.
bool Key_Down(int vk)
{
    (void)vk;
    return false;
}
bool Pad_Read(int index, PadState& out)
{
    (void)index;
    out = PadState();
    return false;
}
void Log_Write(int level, const std::string& s)
{
    (void)level;
    (void)s;
}

static int Failures = 0;
#define CHECK(cond)                                                                                                    \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(cond))                                                                                                   \
        {                                                                                                              \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                                                 \
            Failures++;                                                                                                \
        }                                                                                                              \
    } while (0)

static void TestPadShortPress(uint16_t bit, uint64_t frame)
{
    InputEdges_ResetAll();
    InputEdges_NotePadButtons(0, 0);
    // Rising edge while "held".
    InputEdges_NotePadButtons(0, bit);
    // Released before guest poll.
    InputEdges_NotePadButtons(0, 0);
    const uint16_t pub = InputEdges_PublishPad(0, 0, frame);
    CHECK((pub & bit) == bit);
    // Next guest frame: must not remain stuck.
    const uint16_t pub2 = InputEdges_PublishPad(0, 0, frame + 1);
    CHECK((pub2 & bit) == 0);
}

static void TestPadHoldNoDuplicate(uint16_t bit)
{
    InputEdges_ResetAll();
    const uint64_t rise0 = InputEdges_Stats().risingLatched;
    InputEdges_NotePadButtons(0, 0);
    InputEdges_NotePadButtons(0, bit);
    CHECK((InputEdges_PublishPad(0, bit, 10) & bit) == bit);
    // Still held: next frame is level only, not a new edge flood.
    InputEdges_NotePadButtons(0, bit);
    CHECK((InputEdges_PublishPad(0, bit, 11) & bit) == bit);
    CHECK(InputEdges_Stats().risingLatched == rise0 + 1);
}

static void TestKeyShortTap(int vk)
{
    InputEdges_ResetAll();
    InputEdges_KeyDown(vk);
    InputEdges_KeyUp(vk);
    // Live Key_Down is false (stub); sticky must still report down once.
    CHECK(InputEdges_KeyEffectiveDown(vk));
    InputEdges_PublishKeys(100);
    CHECK(!InputEdges_KeyEffectiveDown(vk));
}

static void TestWpadPadSameFrame()
{
    InputEdges_ResetAll();
    InputEdges_NotePadButtons(0, 0);
    InputEdges_NotePadButtons(0, PAD_A);
    InputEdges_NotePadButtons(0, 0);
    const uint16_t a = InputEdges_PublishPad(0, 0, 50);
    const uint16_t b = InputEdges_PublishPad(0, 0, 50);
    CHECK(a == b);
    CHECK((a & PAD_A) == PAD_A);
}

static void TestFocusClearsSticky()
{
    InputEdges_ResetAll();
    InputEdges_KeyDown('A');
    InputEdges_NotePadButtons(0, 0);
    InputEdges_NotePadButtons(0, PAD_B);
    InputEdges_NotePadButtons(0, 0);
    InputEdges_OnFocusChange(false);
    CHECK(!InputEdges_KeyEffectiveDown('A'));
    CHECK(InputEdges_PublishPad(0, 0, 7) == 0);
}

static void TestDurationsSynthetic()
{
    // Durations are host-wall concepts; here we only assert the latch model for
    // press+release before a single guest sample (covers 1–16 ms class misses).
    const uint16_t bits[] = { PAD_A, PAD_B, PAD_X, PAD_Y, PAD_START };
    for (uint16_t bit : bits)
        TestPadShortPress(bit, 1000 + bit);
}

int main()
{
    TestDurationsSynthetic();
    TestPadHoldNoDuplicate(PAD_A);
    TestKeyShortTap(' ');
    TestKeyShortTap('A');
    TestWpadPadSameFrame();
    TestFocusClearsSticky();
    auto st = InputEdges_Stats();
    std::printf("scr_input_edges_test: failures=%d rising=%llu sticky=%llu key↓=%llu key↑=%llu\n", Failures,
                (unsigned long long)st.risingLatched, (unsigned long long)st.stickyConsumed,
                (unsigned long long)st.hostKeyDown, (unsigned long long)st.hostKeyUp);
    if (Failures) return 1;
    std::puts("scr_input_edges_test OK");
    return 0;
}
