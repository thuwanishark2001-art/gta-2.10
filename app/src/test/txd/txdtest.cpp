// Checks app/src/main/cpp/samp/game/txddecode.h against make_txd.py's output.
//   app/src/test/txd/run.sh
#include <cstdio>
#include <fstream>
#include <iterator>
#include <sstream>
#include "../../main/cpp/samp/game/txddecode.h"

int main(int argc, char** argv)
{
    std::ifstream f(argv[1], std::ios::binary);
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(f)), {});
    std::ostringstream got;
    int skipped = 0;
    int n = kmtxd::ForEachTexture(data.data(), data.size(), [&](const std::string& name, int w, int h, const std::vector<uint8_t>& rgba) {
        got << name << ' ' << w << ' ' << h << ' ';
        for (uint8_t b : rgba) { char hex[3]; snprintf(hex, sizeof hex, "%02x", b); got << hex; }
        got << '\n';
    }, [&](const std::string&) { skipped++; });
    std::ifstream e(argv[2]);
    std::string expected((std::istreambuf_iterator<char>(e)), {});
    // Truncated input must fail cleanly.
    for (size_t cut = 0; cut < data.size(); cut += 7) kmtxd::ForEachTexture(data.data(), cut, [](auto&&...) {}, [](auto&&) {});
    if (got.str() != expected || skipped != 1 || n != 5) {
        printf("FAIL: decoded %d, skipped %d\n--- got\n%s--- expected\n%s", n, skipped, got.str().c_str(), expected.c_str());
        return 1;
    }
    printf("txddecode OK: %d textures, %d skipped\n", n, skipped);
    return 0;
}
