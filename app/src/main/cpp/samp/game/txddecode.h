#pragma once
// Decodes PC (D3D8/D3D9) RenderWare .txd textures to RGBA, level 0 only.
// Header-only and engine-free, so app/src/test/txd checks it on a PC.
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace kmtxd {

// ---- PC .txd decoding -------------------------------------------------------

struct Reader {
    const uint8_t* p; size_t n; size_t pos = 0;
    bool ok = true;
    bool Has(size_t k) const { return ok && pos + k <= n; }
    uint32_t U32() { if (!Has(4)) { ok = false; return 0; } uint32_t v; memcpy(&v, p + pos, 4); pos += 4; return v; }
    uint16_t U16() { if (!Has(2)) { ok = false; return 0; } uint16_t v; memcpy(&v, p + pos, 2); pos += 2; return v; }
    uint8_t U8() { if (!Has(1)) { ok = false; return 0; } return p[pos++]; }
    const uint8_t* Bytes(size_t k) { if (!Has(k)) { ok = false; return nullptr; } const uint8_t* b = p + pos; pos += k; return b; }
};

void Rgb565(uint16_t c, uint8_t* o)
{
    o[0] = (uint8_t) (((c >> 11) & 31) * 255 / 31);
    o[1] = (uint8_t) (((c >> 5) & 63) * 255 / 63);
    o[2] = (uint8_t) ((c & 31) * 255 / 31);
    o[3] = 255;
}

// One 4x4 DXT colour block into rgba (row stride w*4); dxt1 allows 1-bit alpha.
void DxtColourBlock(const uint8_t* b, uint8_t* rgba, int x, int y, int w, int h, bool dxt1)
{
    uint16_t c0 = (uint16_t) (b[0] | b[1] << 8), c1 = (uint16_t) (b[2] | b[3] << 8);
    uint8_t pal[4][4];
    Rgb565(c0, pal[0]);
    Rgb565(c1, pal[1]);
    if (!dxt1 || c0 > c1) {
        for (int i = 0; i < 3; i++) {
            pal[2][i] = (uint8_t) ((2 * pal[0][i] + pal[1][i]) / 3);
            pal[3][i] = (uint8_t) ((pal[0][i] + 2 * pal[1][i]) / 3);
        }
        pal[2][3] = pal[3][3] = 255;
    } else {
        for (int i = 0; i < 3; i++) { pal[2][i] = (uint8_t) ((pal[0][i] + pal[1][i]) / 2); pal[3][i] = 0; }
        pal[2][3] = 255;
        pal[3][3] = 0;
    }
    uint32_t idx = (uint32_t) b[4] | (uint32_t) b[5] << 8 | (uint32_t) b[6] << 16 | (uint32_t) b[7] << 24;
    for (int py = 0; py < 4; py++)
        for (int px = 0; px < 4; px++) {
            int X = x + px, Y = y + py;
            const uint8_t* c = pal[(idx >> (2 * (py * 4 + px))) & 3];
            if (X < w && Y < h) memcpy(rgba + (Y * w + X) * 4, c, 4);
        }
}

bool DecodeDxt(const uint8_t* src, size_t size, int w, int h, int kind, std::vector<uint8_t>& rgba)
{
    int bw = (w + 3) / 4, bh = (h + 3) / 4;
    size_t blockBytes = kind == 1 ? 8 : 16;
    if (size < (size_t) bw * bh * blockBytes) return false;
    rgba.assign((size_t) w * h * 4, 0);
    for (int by = 0; by < bh; by++)
        for (int bx = 0; bx < bw; bx++) {
            const uint8_t* b = src + ((size_t) by * bw + bx) * blockBytes;
            const uint8_t* colour = kind == 1 ? b : b + 8;
            DxtColourBlock(colour, rgba.data(), bx * 4, by * 4, w, h, kind == 1);
            if (kind == 1) continue;
            for (int py = 0; py < 4; py++)
                for (int px = 0; px < 4; px++) {
                    int X = bx * 4 + px, Y = by * 4 + py;
                    if (X >= w || Y >= h) continue;
                    int i = py * 4 + px;
                    uint8_t a;
                    if (kind == 3) {
                        a = (uint8_t) (((b[i / 2] >> ((i & 1) * 4)) & 15) * 17);
                    } else {
                        uint8_t a0 = b[0], a1 = b[1];
                        uint64_t bits = 0;
                        for (int k = 0; k < 6; k++) bits |= (uint64_t) b[2 + k] << (8 * k);
                        int code = (int) ((bits >> (3 * i)) & 7);
                        if (code == 0) a = a0;
                        else if (code == 1) a = a1;
                        else if (a0 > a1) a = (uint8_t) (((8 - code) * a0 + (code - 1) * a1) / 7);
                        else if (code == 6) a = 0;
                        else if (code == 7) a = 255;
                        else a = (uint8_t) (((6 - code) * a0 + (code - 1) * a1) / 5);
                    }
                    rgba[((size_t) Y * w + X) * 4 + 3] = a;
                }
        }
    return true;
}

// Raster format bits (RenderWare) and Direct3D formats a PC .txd uses.
constexpr uint32_t kFmt1555 = 0x100, kFmt565 = 0x200, kFmt4444 = 0x300, kFmtLum8 = 0x400,
                   kFmt8888 = 0x500, kFmt888 = 0x600, kFmt555 = 0xA00,
                   kFmtPal8 = 0x2000, kFmtPal4 = 0x4000;
constexpr uint32_t kFourDxt1 = 0x31545844, kFourDxt3 = 0x33545844, kFourDxt5 = 0x35545844;

// Decodes level 0 of one Texture Native struct to RGBA. False if unsupported.
bool DecodeTextureNative(Reader& r, std::string& name, int& w, int& h, std::vector<uint8_t>& rgba)
{
    uint32_t platform = r.U32();
    r.U32(); // filtering and addressing
    const uint8_t* nm = r.Bytes(32);
    r.Bytes(32); // mask
    uint32_t rasterFormat = r.U32();
    uint32_t d3d = r.U32();  // D3D8: has alpha; D3D9: D3DFORMAT or FOURCC
    w = r.U16();
    h = r.U16();
    uint8_t depth = r.U8();
    r.U8(); // levels
    r.U8(); // raster type
    uint8_t compression = r.U8(); // D3D8: 0 or DXTn; D3D9: flags (bit 3 compressed)
    if (!r.ok || !nm) return false;
    name.assign((const char*) nm, strnlen((const char*) nm, 32));
    if (platform != 8 && platform != 9) {
        return false;
    }
    if (w <= 0 || h <= 0 || w > 4096 || h > 4096) return false;

    const uint8_t* palette = nullptr;
    int palCount = 0;
    if (rasterFormat & kFmtPal8) palCount = 256;
    else if (rasterFormat & kFmtPal4) palCount = platform == 8 ? 32 : 16;
    if (palCount) palette = r.Bytes((size_t) palCount * 4);

    uint32_t size = r.U32();
    const uint8_t* data = r.Bytes(size);
    if (!r.ok || !data) return false;

    int dxt = 0;
    if (platform == 9) {
        if (d3d == kFourDxt1) dxt = 1; else if (d3d == kFourDxt3) dxt = 3; else if (d3d == kFourDxt5) dxt = 5;
    } else if (compression) {
        dxt = compression;
    }
    if (dxt) return DecodeDxt(data, size, w, h, dxt, rgba);

    size_t pixels = (size_t) w * h;
    rgba.assign(pixels * 4, 255);
    if (palette) {
        bool pal4 = (rasterFormat & kFmtPal4) != 0;
        if (size < (pal4 ? (pixels + 1) / 2 : pixels)) return false;
        for (size_t i = 0; i < pixels; i++) {
            int idx = pal4 ? ((data[i / 2] >> ((i & 1) * 4)) & 15) : data[i];
            if (idx >= palCount) idx = 0;
            memcpy(&rgba[i * 4], palette + idx * 4, 4);   // palette is R,G,B,A
        }
        return true;
    }
    uint32_t fmt = rasterFormat & 0xF00;
    int bpp = depth / 8;
    if (bpp <= 0 || size < pixels * bpp) return false;
    for (size_t i = 0; i < pixels; i++) {
        const uint8_t* s = data + i * bpp;
        uint8_t* o = &rgba[i * 4];
        if (bpp == 4) {               // B,G,R,A (A8R8G8B8 / X8R8G8B8)
            o[0] = s[2]; o[1] = s[1]; o[2] = s[0];
            o[3] = (fmt == kFmt888) ? 255 : s[3];
        } else if (bpp == 3) {
            o[0] = s[2]; o[1] = s[1]; o[2] = s[0]; o[3] = 255;
        } else if (bpp == 2) {
            uint16_t c = (uint16_t) (s[0] | s[1] << 8);
            if (fmt == kFmt565) Rgb565(c, o);
            else if (fmt == kFmt4444) {
                o[0] = (uint8_t) (((c >> 8) & 15) * 17); o[1] = (uint8_t) (((c >> 4) & 15) * 17);
                o[2] = (uint8_t) ((c & 15) * 17); o[3] = (uint8_t) (((c >> 12) & 15) * 17);
            } else { // 1555 / 555
                o[0] = (uint8_t) (((c >> 10) & 31) * 255 / 31); o[1] = (uint8_t) (((c >> 5) & 31) * 255 / 31);
                o[2] = (uint8_t) ((c & 31) * 255 / 31); o[3] = (fmt == kFmt1555 && !(c & 0x8000)) ? 0 : 255;
            }
        } else if (bpp == 1) {        // LUM8
            o[0] = o[1] = o[2] = s[0]; o[3] = 255;
        }
    }
    (void) kFmtLum8; (void) kFmt8888; (void) kFmt555;
    return true;
}


// Walks a .txd: TexDictionary { Struct, TextureNative { Struct, Extension }* }.
// Calls onTexture(name, w, h, rgba) for each decoded texture and onSkip(name)
// for unsupported ones. Returns how many decoded, or -1 if it isn't a .txd.
template <typename OnTexture, typename OnSkip>
int ForEachTexture(const uint8_t* data, size_t n, OnTexture onTexture, OnSkip onSkip)
{
    Reader top{data, n};
    if (top.U32() != 0x16) return -1;
    uint32_t dictSize = top.U32();
    top.U32();
    size_t end = std::min(n, top.pos + (size_t) dictSize);
    int made = 0;
    while (top.ok && top.pos + 12 <= end) {
        uint32_t type = top.U32(), size = top.U32();
        top.U32();
        size_t next = top.pos + size;
        if (next > end) break;
        if (type == 0x15) {
            Reader tex{data, next, top.pos};
            if (tex.U32() == 0x1) {
                uint32_t structSize = tex.U32();
                tex.U32();
                Reader body{data, std::min(tex.pos + (size_t) structSize, next), tex.pos};
                std::string name;
                int w = 0, h = 0;
                std::vector<uint8_t> rgba;
                if (DecodeTextureNative(body, name, w, h, rgba)) {
                    onTexture(name, w, h, rgba);
                    made++;
                } else {
                    onSkip(name);
                }
            }
        }
        top.pos = next;
    }
    return made;
}

} // namespace kmtxd
