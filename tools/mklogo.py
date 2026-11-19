#!/usr/bin/env python3
"""Render the tmt wordmark into menu_text run format (y, x, len, alpha 1..3) and patch menu_text.hpp."""
import sys, re
from PIL import Image, ImageDraw, ImageFont
F = '/usr/share/fonts/truetype/google-fonts/ofl/montserrat/Montserrat[wght].ttf'
def render(text, h):
    f = ImageFont.truetype(F, int(h * 1.35))
    try: f.set_variation_by_axes([850])
    except Exception: pass
    im = Image.new('L', (h * 8, h * 3), 0); d = ImageDraw.Draw(im)
    d.text((4, 0), text, font=f, fill=255)
    bb = im.getbbox(); im = im.crop(bb)
    if im.height != h: im = im.resize((max(1, round(im.width * h / im.height)), h), Image.LANCZOS)
    W, H = im.size; px = im.load(); runs = []
    for y in range(H):
        x = 0
        while x < W:
            a = px[x, y]; q = 0 if a < 40 else 1 if a < 110 else 2 if a < 190 else 3
            if q == 0: x += 1; continue
            s = x
            while x < W:
                b = px[x, y]; qb = 0 if b < 40 else 1 if b < 110 else 2 if b < 190 else 3
                if qb != q: break
                x += 1
            runs.append((y, s, x - s, q))
    return W, H, runs
p = sys.argv[1]; src = open(p, encoding='utf-8').read()
for name, h in (('kTITLE', 22), ('kTITLEBIG', 40), ('kTITLEXL', 40)):
    W, H, runs = render('tmt', h)
    arr = ','.join(f'{a},{b},{c},{d}' for a, b, c, d in runs)
    src = re.sub(rf'inline constexpr uint16_t {name}_runs\[\] = \{{[^}}]*\}};', f'inline constexpr uint16_t {name}_runs[] = {{{arr}}};', src)
    src = re.sub(rf'inline constexpr Str {name} = \{{[0-9]+,[0-9]+,[0-9]+,{name}_runs\}};', f'inline constexpr Str {name} = {{{W},{H},{len(runs)},{name}_runs}};', src)
    print(name, W, H, len(runs))
open(p, 'w', encoding='utf-8').write(src)
