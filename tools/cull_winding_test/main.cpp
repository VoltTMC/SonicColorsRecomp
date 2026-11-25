// A4j: GX cull → backend mapping must keep clockwise fronts (match D3D11).
// Regression for the A4g CCW-front mistake that culled TR1 sky/background fronts.
#include "gx/gfx_draw.h"

#include <cassert>
#include <cstdio>

int main()
{
    assert(GfxCullFromGx(0) == GfxCull::None);
    assert(GfxCullFromGx(1) == GfxCull::Back);
    assert(GfxCullFromGx(2) == GfxCull::Front);
    assert(GfxCullFromGx(3) == GfxCull::None);

    // Hardware mode with CW front: Back→cull-back(2), Front→cull-front(1).
    // A swapped mapping (Back→1) is the defect that left sky at EFB clear.
    assert(GfxCullHwMode(GfxCull::None) == 0);
    assert(GfxCullHwMode(GfxCull::Back) == 2);
    assert(GfxCullHwMode(GfxCull::Front) == 1);

    std::puts("scr_cull_winding_test OK");
    return 0;
}
