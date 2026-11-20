// Unit checks for Install_LooksLikeDiscImage + Release_UnsupportedMessage (no disc I/O).
#include "../../runtime/install/install.h"
#include "../../runtime/install/releases.h"

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
    CHECK(Install_LooksLikeDiscImage("game/Sonic.iso"));
    CHECK(Install_LooksLikeDiscImage("x.RVZ"));
    CHECK(Install_LooksLikeDiscImage("a/b/c.Wbfs"));
    CHECK(Install_LooksLikeDiscImage("colours.gcz"));
    CHECK(Install_LooksLikeDiscImage("disc.wia"));
    CHECK(Install_LooksLikeDiscImage("dump.ciso"));
    CHECK(Install_LooksLikeDiscImage("x.nfs"));
    CHECK(!Install_LooksLikeDiscImage("readme.txt"));
    CHECK(!Install_LooksLikeDiscImage("game/extracted/DATA"));
    CHECK(!Install_LooksLikeDiscImage("sonic.iso.bak"));

    CHECK(Release_Find("SNCP8P") != nullptr);
    CHECK(Release_Find("SNCE8P") == nullptr);

    const std::string usa = Release_UnsupportedMessage("SNCE8P");
    CHECK(usa.find("USA") != std::string::npos);
    CHECK(usa.find("SNCP8P") != std::string::npos);

    const std::string jp = Release_UnsupportedMessage("SNCJ8P");
    CHECK(jp.find("Japan") != std::string::npos);

    const std::string other = Release_UnsupportedMessage("RMGE01");
    CHECK(other.find("RMGE01") != std::string::npos);

    const std::string empty = Release_UnsupportedMessage("");
    CHECK(empty.find("SNCP8P") != std::string::npos);

    if (g_fail)
    {
        fprintf(stderr, "%d check(s) failed\n", g_fail);
        return 1;
    }
    puts("OK: install_helpers_test");
    return 0;
}
