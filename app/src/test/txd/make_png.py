#!/usr/bin/env python3
"""Writes PNGs in each colour type to a folder, plus <name>.rgba with the
RGBA the decoder must produce (Pillow's convert('RGBA'))."""
import os, random, sys
from PIL import Image
out = sys.argv[1]
random.seed(3)
def save(name, im, **kw):
    im.save(os.path.join(out, name + '.png'), **kw)
    ref = Image.open(os.path.join(out, name + '.png')).convert('RGBA')
    open(os.path.join(out, name + '.rgba'), 'wb').write(ref.size[0].to_bytes(4, 'little') + ref.size[1].to_bytes(4, 'little') + ref.tobytes())
def noise(mode, size, n):
    im = Image.new(mode, size)
    im.putdata([tuple(random.randrange(256) for _ in range(n)) if n > 1 else random.randrange(256) for _ in range(size[0] * size[1])])
    return im
save('rgba', noise('RGBA', (17, 9), 4))
save('rgb', noise('RGB', (16, 16), 3))
save('grey', noise('L', (5, 7), 1))
save('greyalpha', noise('LA', (6, 3), 2))
p = noise('P', (9, 9), 1); p.putpalette([random.randrange(256) for _ in range(768)])
save('palette', p, transparency=bytes(random.randrange(256) for _ in range(256)))
save('mono', noise('1', (13, 5), 1).convert('1'))
p4 = Image.new('P', (11, 6)); p4.putpalette([random.randrange(256) for _ in range(48)]); p4.putdata([random.randrange(16) for _ in range(66)])
save('pal4', p4, bits=4)
save('big', noise('RGBA', (256, 128), 4), compress_level=9)
