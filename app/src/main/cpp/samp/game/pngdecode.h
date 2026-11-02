#pragma once
// Decodes a PNG to 8-bit RGBA with zlib. Covers what texture packs use:
// greyscale, RGB, palette (with tRNS), grey+alpha and RGBA, at 8 bits (and
// 1/2/4-bit palette/grey, 16-bit by dropping the low byte), non-interlaced
// and Adam7. Header-only and engine-free, so app/src/test/txd checks it on a PC.
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <zlib.h>

namespace kmpng {

inline uint32_t Be32(const uint8_t* p) { return (uint32_t) p[0] << 24 | (uint32_t) p[1] << 16 | (uint32_t) p[2] << 8 | p[3]; }

inline int Paeth(int a, int b, int c)
{
    int p = a + b - c, pa = p > a ? p - a : a - p, pb = p > b ? p - b : b - p, pc = p > c ? p - c : c - p;
    if (pa <= pb && pa <= pc) return a;
    return pb <= pc ? b : c;
}

// Undoes the per-row filters of one (sub)image in place. False on a bad filter.
inline bool Unfilter(uint8_t* data, size_t rows, size_t rowBytes, size_t bpp)
{
    std::vector<uint8_t> zero(rowBytes, 0);
    uint8_t* prev = zero.data();
    for (size_t y = 0; y < rows; y++) {
        uint8_t* line = data + y * (rowBytes + 1);
        uint8_t f = line[0];
        uint8_t* cur = line + 1;
        for (size_t x = 0; x < rowBytes; x++) {
            int a = x >= bpp ? cur[x - bpp] : 0, b = prev[x], c = x >= bpp ? prev[x - bpp] : 0;
            switch (f) {
                case 0: break;
                case 1: cur[x] = (uint8_t) (cur[x] + a); break;
                case 2: cur[x] = (uint8_t) (cur[x] + b); break;
                case 3: cur[x] = (uint8_t) (cur[x] + ((a + b) >> 1)); break;
                case 4: cur[x] = (uint8_t) (cur[x] + Paeth(a, b, c)); break;
                default: return false;
            }
        }
        prev = cur;
    }
    return true;
}

inline bool Decode(const uint8_t* data, size_t n, int& w, int& h, std::vector<uint8_t>& rgba)
{
    static const uint8_t sig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    if (n < 8 || memcmp(data, sig, 8) != 0) return false;
    size_t pos = 8;
    uint32_t width = 0, height = 0;
    int depth = 0, colour = 0, interlace = 0;
    std::vector<uint8_t> idat, palette, trns;
    bool haveHeader = false;
    while (pos + 12 <= n) {
        uint32_t len = Be32(data + pos);
        const uint8_t* type = data + pos + 4;
        const uint8_t* body = data + pos + 8;
        if (len > n - pos - 12) return false;
        if (!memcmp(type, "IHDR", 4) && len >= 13) {
            width = Be32(body); height = Be32(body + 4);
            depth = body[8]; colour = body[9]; interlace = body[12];
            haveHeader = body[10] == 0 && body[11] == 0;
        } else if (!memcmp(type, "PLTE", 4)) {
            palette.assign(body, body + len);
        } else if (!memcmp(type, "tRNS", 4)) {
            trns.assign(body, body + len);
        } else if (!memcmp(type, "IDAT", 4)) {
            idat.insert(idat.end(), body, body + len);
        } else if (!memcmp(type, "IEND", 4)) {
            break;
        }
        pos += 12 + (size_t) len;
    }
    if (!haveHeader || width == 0 || height == 0 || width > 4096 || height > 4096 || interlace > 1) return false;
    int channels;
    switch (colour) {
        case 0: channels = 1; break;
        case 2: channels = 3; break;
        case 3: channels = 1; break;
        case 4: channels = 2; break;
        case 6: channels = 4; break;
        default: return false;
    }
    bool okDepth = depth == 8 || depth == 16 || ((colour == 0 || colour == 3) && (depth == 1 || depth == 2 || depth == 4));
    if (!okDepth || (colour == 3 && depth == 16) || (colour == 3 && palette.size() < 3)) return false;
    size_t bitsPerPixel = (size_t) channels * depth;
    size_t bpp = bitsPerPixel >= 8 ? bitsPerPixel / 8 : 1;

    // Adam7 passes (or the one full image).
    static const int ax[7] = {0, 4, 0, 2, 0, 1, 0}, ay[7] = {0, 0, 4, 0, 2, 0, 1},
                     dx[7] = {8, 8, 4, 4, 2, 2, 1}, dy[7] = {8, 8, 8, 4, 4, 2, 2};
    int passes = interlace ? 7 : 1;
    size_t total = 0;
    size_t pw[7], ph[7];
    for (int p = 0; p < passes; p++) {
        pw[p] = interlace ? (width + dx[p] - 1 - ax[p]) / dx[p] : width;
        ph[p] = interlace ? (height + dy[p] - 1 - ay[p]) / dy[p] : height;
        if (pw[p] && ph[p]) total += ph[p] * ((pw[p] * bitsPerPixel + 7) / 8 + 1);
    }
    std::vector<uint8_t> raw(total);
    uLongf rawLen = (uLongf) total;
    if (uncompress(raw.data(), &rawLen, idat.data(), (uLong) idat.size()) != Z_OK || rawLen != total) return false;

    w = (int) width;
    h = (int) height;
    rgba.assign((size_t) width * height * 4, 255);
    size_t off = 0;
    for (int p = 0; p < passes; p++) {
        if (!pw[p] || !ph[p]) continue;
        size_t rowBytes = (pw[p] * bitsPerPixel + 7) / 8;
        uint8_t* img = raw.data() + off;
        if (!Unfilter(img, ph[p], rowBytes, bpp)) return false;
        for (size_t y = 0; y < ph[p]; y++) {
            const uint8_t* row = img + y * (rowBytes + 1) + 1;
            for (size_t x = 0; x < pw[p]; x++) {
                size_t X = interlace ? ax[p] + x * dx[p] : x, Y = interlace ? ay[p] + y * dy[p] : y;
                uint8_t* o = &rgba[(Y * width + X) * 4];
                auto sample = [&](int c) -> int {   // channel c as 0-255, or the raw index for palettes
                    if (depth == 8) return row[x * channels + c];
                    if (depth == 16) return row[(x * channels + c) * 2];
                    int perByte = 8 / depth;
                    int v = (row[x / perByte] >> ((perByte - 1 - (int) (x % perByte)) * depth)) & ((1 << depth) - 1);
                    return colour == 3 ? v : v * 255 / ((1 << depth) - 1);
                };
                switch (colour) {
                    case 0: {
                        int g = sample(0);
                        o[0] = o[1] = o[2] = (uint8_t) g;
                        if (trns.size() >= 2 && depth == 8 && g == trns[1]) o[3] = 0;
                        break;
                    }
                    case 2:
                        o[0] = (uint8_t) sample(0); o[1] = (uint8_t) sample(1); o[2] = (uint8_t) sample(2);
                        if (trns.size() >= 6 && depth == 8 && o[0] == trns[1] && o[1] == trns[3] && o[2] == trns[5]) o[3] = 0;
                        break;
                    case 3: {
                        size_t i = (size_t) sample(0);
                        if (i * 3 + 2 < palette.size()) { o[0] = palette[i * 3]; o[1] = palette[i * 3 + 1]; o[2] = palette[i * 3 + 2]; }
                        if (i < trns.size()) o[3] = trns[i];
                        break;
                    }
                    case 4: o[0] = o[1] = o[2] = (uint8_t) sample(0); o[3] = (uint8_t) sample(1); break;
                    case 6: o[0] = (uint8_t) sample(0); o[1] = (uint8_t) sample(1); o[2] = (uint8_t) sample(2); o[3] = (uint8_t) sample(3); break;
                }
            }
        }
        off += ph[p] * (rowBytes + 1);
    }
    return true;
}

} // namespace kmpng
