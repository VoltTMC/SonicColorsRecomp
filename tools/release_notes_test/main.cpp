// Unit checks for SoftenReleaseBody (release notes softener).
#include "../../runtime/ui/release_notes.h"

#include <cstdio>
#include <string>

static int g_fail = 0;
#define CHECK(cond)                                                                                                    \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(cond))                                                                                                   \
        {                                                                                                              \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                                             \
            g_fail++;                                                                                                  \
        }                                                                                                              \
    } while (0)

int main()
{
    CHECK(SoftenReleaseBody("## Title\n").find('#') == std::string::npos);
    CHECK(SoftenReleaseBody("- bullet\n").find("bullet") != std::string::npos);
    CHECK(SoftenReleaseBody("well-known\n").find("well-known") != std::string::npos);
    CHECK(SoftenReleaseBody("[link](https://example.com)\n").find("https") == std::string::npos);
    CHECK(SoftenReleaseBody("[link](https://example.com)\n").find("link") != std::string::npos);
    CHECK(SoftenReleaseBody("<!-- hide -->shown\n").find("hide") == std::string::npos);
    CHECK(SoftenReleaseBody("<!-- hide -->shown\n").find("shown") != std::string::npos);

    const std::string longBody(2000, 'a');
    const std::string capped = SoftenReleaseBody(longBody, 50);
    CHECK(capped.size() <= 60);
    CHECK(capped.find("…") != std::string::npos);

    if (g_fail)
    {
        fprintf(stderr, "%d checks failed\n", g_fail);
        return 1;
    }
    puts("release_notes_test OK");
    return 0;
}
