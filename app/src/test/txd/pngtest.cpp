// Checks pngdecode.h against make_png.py's PNGs and reference RGBA.
#include <cstdio>
#include <dirent.h>
#include <fstream>
#include <iterator>
#include <string>
#include "../../main/cpp/samp/game/pngdecode.h"

static std::vector<uint8_t> Read(const std::string& p)
{
    std::ifstream f(p, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), {});
}

int main(int, char** argv)
{
    std::string dir = argv[1];
    int checked = 0, failed = 0;
    DIR* d = opendir(dir.c_str());
    while (dirent* e = readdir(d)) {
        std::string name = e->d_name;
        if (name.size() < 5 || name.substr(name.size() - 4) != ".png") continue;
        std::string base = name.substr(0, name.size() - 4);
        auto png = Read(dir + "/" + name), ref = Read(dir + "/" + base + ".rgba");
        int w = 0, h = 0;
        std::vector<uint8_t> rgba;
        bool ok = kmpng::Decode(png.data(), png.size(), w, h, rgba);
        uint32_t rw, rh;
        memcpy(&rw, ref.data(), 4); memcpy(&rh, ref.data() + 4, 4);
        std::vector<uint8_t> want(ref.begin() + 8, ref.end());
        if (!ok || (uint32_t) w != rw || (uint32_t) h != rh || rgba != want) { printf("FAIL %s\n", base.c_str()); failed++; }
        // Damaged files must fail cleanly.
        for (size_t cut = 0; cut < png.size(); cut += 5) kmpng::Decode(png.data(), cut, w, h, rgba);
        auto bad = png;
        for (size_t i = 40; i < bad.size(); i += 13) bad[i] ^= 0x5a;
        kmpng::Decode(bad.data(), bad.size(), w, h, rgba);
        checked++;
    }
    closedir(d);
    if (failed || checked < 8) { printf("pngdecode FAILED: %d of %d\n", failed, checked); return 1; }
    printf("pngdecode OK: %d images\n", checked);
    return 0;
}
