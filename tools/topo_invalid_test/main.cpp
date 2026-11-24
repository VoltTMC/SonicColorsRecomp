// A4c / A3x: Topo::Invalid must remain distinct from every live GX topology.
#include <cassert>
#include <cstdint>

enum class Topo : uint8_t { Tri, Line, Point, Invalid };

int main()
{
    static_assert(Topo::Invalid != Topo::Tri);
    static_assert(Topo::Invalid != Topo::Line);
    static_assert(Topo::Invalid != Topo::Point);

    Topo bound = Topo::Invalid;
    auto apply = [&](Topo t) {
        if (t != bound)
            bound = t;
    };
    // Utility path (DrawQuad analogue) leaves strip semantics — must invalidate.
    bound = Topo::Tri;
    bound = Topo::Invalid;
    apply(Topo::Tri);
    assert(bound == Topo::Tri);
    apply(Topo::Line);
    assert(bound == Topo::Line);
    apply(Topo::Point);
    assert(bound == Topo::Point);
    bound = Topo::Invalid;
    apply(Topo::Tri);
    assert(bound == Topo::Tri);
    return 0;
}
