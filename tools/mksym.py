#!/usr/bin/env python3
"""Render the game's private-use glyphs (BlitzMain.otf, decrypted from romfs Font/scft/BlitzMain.bfotf) into menu_text run bitmaps."""
import sys
from PIL import Image, ImageDraw, ImageFont
from fontTools.ttLib import TTFont
otf, out = sys.argv[1], sys.argv[2]; H = 26
cm = TTFont(otf).getBestCmap(); codes = sorted(c for c in cm if 0xE000 <= c <= 0xF8FF)
f = ImageFont.truetype(otf, 30)
lines = ['#pragma once', '/* 自動生成 (tools/mksym.py): ゲームのフォント BlitzMain の私用領域の記号。runs は (y, x, 長さ, 濃さ1..3) */', '#include <cstdint>', '#include "menu_text.hpp"', 'namespace sym_glyphs {']
strs = []
for c in codes:
    im = Image.new('L', (64, 64), 0); d = ImageDraw.Draw(im); d.text((32, 32), chr(c), font=f, fill=255, anchor='mm')
    bb = im.getbbox() or (28, 28, 30, 30)
    cy = 32; top = cy - H // 2
    im = im.crop((bb[0], top, bb[2], top + H)); W = im.width; px = im.load(); runs = []
    for y in range(H):
        x = 0
        while x < W:
            q = lambda a: 0 if a < 40 else 1 if a < 110 else 2 if a < 190 else 3
            if q(px[x, y]) == 0: x += 1; continue
            s0 = x; qq = q(px[x, y])
            while x < W and q(px[x, y]) == qq: x += 1
            runs.append((y, s0, x - s0, qq))
    if not runs: runs = [(0, 0, 0, 0)]
    lines.append(f'inline constexpr uint16_t k{c:04X}_runs[] = {{{",".join(f"{a},{b},{cc},{dd}" for a, b, cc, dd in runs)}}};')
    strs.append(f'{{ {max(W, 8)}, {H}, {len(runs)}, k{c:04X}_runs }}')
lines.append(f'inline constexpr int kN = {len(codes)};')
lines.append('inline constexpr uint16_t kCode[kN] = { ' + ', '.join(f'0x{c:04X}' for c in codes) + ' };')
lines.append('inline constexpr menu_text::Str kGlyph[kN] = { ' + ', '.join(strs) + ' };')
lines.append('}')
open(out, 'w', encoding='utf-8').write('\n'.join(lines) + '\n'); print(len(codes), 'glyphs')
