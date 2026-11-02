#!/usr/bin/env python3
"""Writes test.txd: small PC textures in each format txddecode.h reads, and
expected.txt with each texture's name, size and RGBA bytes (hex)."""
import struct, sys

def chunk(t, body): return struct.pack('<III', t, len(body), 0x1803FFFF) + body

def native(platform, name, fmt, d3d, w, h, depth, comp, data, palette=b''):
    s = struct.pack('<II', platform, 0x1102) + name.encode().ljust(32, b'\0') + b''.ljust(32, b'\0')
    s += struct.pack('<IIHHBBBB', fmt, d3d, w, h, depth, 1, 4, comp) + palette
    s += struct.pack('<I', len(data)) + data
    return chunk(0x15, chunk(0x1, s) + chunk(0x3, b''))

tex, expected = [], []
# D3D9 A8R8G8B8 2x1: stored B,G,R,A
tex.append(native(9, 'argb', 0x500, 21, 2, 1, 32, 0, bytes([10, 20, 30, 40, 1, 2, 3, 255])))
expected.append(('argb', 2, 1, bytes([30, 20, 10, 40, 3, 2, 1, 255])))
# D3D9 DXT1 4x4, colour0 = pure red (0xF800) everywhere (indices 0)
tex.append(native(9, 'dxt1red', 0x200, 0x31545844, 4, 4, 16, 8, struct.pack('<HHI', 0xF800, 0x001F, 0)))
expected.append(('dxt1red', 4, 4, bytes([255, 0, 0, 255]) * 16))
# D3D9 DXT5 4x4: alpha a0=128 (code 0 everywhere), colour0 = green (0x07E0)
tex.append(native(9, 'dxt5', 0x300, 0x35545844, 4, 4, 16, 9, bytes([128, 0]) + bytes(6) + struct.pack('<HHI', 0x07E0, 0, 0)))
expected.append(('dxt5', 4, 4, bytes([0, 255, 0, 128]) * 16))
# D3D8 PAL8 2x1, palette RGBA: entry 0 blue, entry 1 white
pal = bytes([0, 0, 255, 255, 255, 255, 255, 255]) + bytes(254 * 4)
tex.append(native(8, 'pal8', 0x2500, 1, 2, 1, 8, 0, bytes([1, 0]), pal))
expected.append(('pal8', 2, 1, bytes([255, 255, 255, 255, 0, 0, 255, 255])))
# D3D8 DXT3 4x4: alpha nibbles all 15, colour white (0xFFFF)
tex.append(native(8, 'dxt3', 0x300, 1, 4, 4, 16, 3, bytes([0xFF] * 8) + struct.pack('<HHI', 0xFFFF, 0, 0)))
expected.append(('dxt3', 4, 4, bytes([255, 255, 255, 255]) * 16))
# A texture from another platform (PS2) must be skipped, not crash.
tex.append(native(6, 'ps2', 0x500, 0, 2, 1, 32, 0, bytes(8)))

body = chunk(0x1, struct.pack('<HH', len(tex), 0)) + b''.join(tex) + chunk(0x3, b'')
open(sys.argv[1], 'wb').write(chunk(0x16, body))
with open(sys.argv[2], 'w') as f:
    for name, w, h, rgba in expected: f.write(f'{name} {w} {h} {rgba.hex()}\n')
