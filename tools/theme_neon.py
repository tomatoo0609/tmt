#!/usr/bin/env python3
"""tmt theme: dark + neon (cyan / magenta). Rewrites only the UI drawing region of main.cpp."""
import sys, re
p = sys.argv[1]
s = open(p, encoding='utf-8').read()
a = s.index('    void SwDrawFunc(Cv& v, int x, int y, int& outW, int& outH) {')
b = s.index('extern "C" void exl_main')
ui = s[a:b]

CY = 'Col { 0.00f, 0.90f, 1.00f }'      # neon cyan
MG = 'Col { 1.00f, 0.17f, 0.84f }'      # neon magenta
def C(r, g, b_): return f'Col {{ {r:.2f}f, {g:.2f}f, {b_:.2f}f }}'

pairs = [
    # base palette (menu + splash + dialog)
    ('const Col panel { 0.235f, 0.240f, 0.250f }, border { 0.44f, 0.45f, 0.47f }, whiteG { 0.98f, 0.98f, 0.99f }, accent { 1.0f, 1.0f, 1.0f }, good { 0.90f, 0.92f, 0.94f };',
     'const Col panel { 0.030f, 0.035f, 0.060f }, border { 0.00f, 0.55f, 0.70f }, whiteG { 0.88f, 0.95f, 1.00f }, accent { 0.00f, 0.90f, 1.00f }, good { 0.55f, 1.00f, 0.80f }; /* tmt: ダーク + ネオン */'),
    ('const Col dimG { 0.74f, 0.75f, 0.78f }, selBg { 0.93f, 0.94f, 0.95f }, ink { 0.05f, 0.055f, 0.065f };',
     'const Col dimG { 0.46f, 0.56f, 0.70f }, selBg { 0.00f, 0.20f, 0.28f }, ink { 0.40f, 0.97f, 1.00f }; /* 選択: 暗いシアンの帯 + 光る文字 */'),
    ('const Col panel { 0.235f, 0.240f, 0.250f }, frame { 0.56f, 0.57f, 0.60f }, line { 0.44f, 0.45f, 0.47f }, white { 0.98f, 0.98f, 0.99f }, dim { 0.74f, 0.75f, 0.78f };',
     'const Col panel { 0.030f, 0.035f, 0.060f }, frame { 0.00f, 0.90f, 1.00f }, line { 1.00f, 0.17f, 0.84f }, white { 0.88f, 0.95f, 1.00f }, dim { 0.46f, 0.56f, 0.70f }; /* tmt */'),
    # colours that assumed a white selection pill -> bright on the dark neon bar
    ('sel ? Col { 0.96f, 0.96f, 0.97f } : Col { 0.45f, 0.47f, 0.51f }', 'sel ? Col { 0.40f, 0.97f, 1.00f } : Col { 0.20f, 0.30f, 0.42f }'),
    ('sel ? Col { 0.66f, 0.67f, 0.70f } : Col { 0.20f, 0.21f, 0.24f }', 'sel ? Col { 0.00f, 0.45f, 0.58f } : Col { 0.07f, 0.10f, 0.16f }'),
    ('sel ? Col { 0.75f, 0.05f, 0.05f } : Col { 1.0f, 0.25f, 0.25f }', 'sel ? Col { 1.0f, 0.45f, 0.55f } : Col { 1.0f, 0.25f, 0.35f }'),
    ('sel ? Col { 0.70f, 0.71f, 0.75f } : Col { 0.16f, 0.15f, 0.22f }', 'sel ? Col { 0.00f, 0.45f, 0.58f } : Col { 0.10f, 0.08f, 0.18f }'),
    ('sel ? Col { 0.40f, 0.42f, 0.46f }', 'sel ? Col { 0.20f, 0.75f, 0.85f }'),
    ('sel ? Col { 0.40f, 0.40f, 0.46f } : Col { 0.40f, 0.38f, 0.50f }', 'sel ? Col { 0.30f, 0.80f, 0.90f } : Col { 0.40f, 0.38f, 0.55f }'),
    ('sel ? Col { 0.36f, 0.20f, 0.64f } : Col { 0.78f, 0.66f, 1.0f }', 'sel ? Col { 1.00f, 0.45f, 0.90f } : Col { 0.85f, 0.60f, 1.0f }'),
    ('sel ? Col { 0.25f, 0.27f, 0.30f } : Col { 0.78f, 0.80f, 0.84f }', 'sel ? Col { 0.30f, 0.85f, 0.95f } : Col { 0.70f, 0.78f, 0.88f }'),
    ('sel ? Col { 0.08f, 0.09f, 0.11f } : Col { 1.0f, 1.0f, 1.0f }', 'sel ? Col { 0.40f, 0.97f, 1.00f } : Col { 0.88f, 0.95f, 1.00f }'),
    # frames / greys
    ('Col { 0.46f, 0.48f, 0.52f }', CY),
    ('Col { 0.16f, 0.17f, 0.19f }', C(0.05, 0.11, 0.17)),
    ('Col { 0.20f, 0.21f, 0.24f }', C(0.07, 0.10, 0.16)),
    ('Col { 0.34f, 0.36f, 0.40f }', C(0.14, 0.26, 0.36)),
    ('Col { 0.38f, 0.40f, 0.45f }', C(0.00, 0.55, 0.70)),
]
for o, n in pairs:
    c = ui.count(o)
    assert c >= 1, o
    ui = ui.replace(o, n)

# sharper corners (techy look): panels 18/16 -> 10/8, pills 12 -> 5, tabs 14 -> 8
ui = ui.replace(', 18.0f, ', ', 10.0f, ').replace(', 16.0f, ', ', 8.0f, ').replace(', 14.0f, ', ', 8.0f, ')
ui = re.sub(r'(PRect pr \{[^}]*?), 18 \}', r'\1, 10 }', ui)
ui = re.sub(r'(PRect pr \{[^}]*?), 14 \}', r'\1, 8 }', ui)
ui = ui.replace('rowH - 2, 12.0f,', 'rowH - 2, 5.0f,')

# second (category) panel gets the magenta frame, first keeps cyan
o = f'SwRRect(v, bx, y0, MW, MHc, 10.0f, {CY});'
assert ui.count(o) == 1
ui = ui.replace(o, f'SwRRect(v, bx, y0, MW, MHc, 10.0f, {MG}); /* tmt: 2 つ目はマゼンタのふち */')

# selection: neon bar on the left of the highlighted row
o = '''                SwRRect(v, x0 + 6, yy + 1, MW - 12, rowH - 2, 5.0f, side ? Col { 0.05f, 0.11f, 0.17f } : selBg);'''
assert ui.count(o) == 1, 'selbar'
ui = ui.replace(o, o + '''
                if (!side) { SwRect(v, x0 + 6, yy + 5, 3, rowH - 10, Col { 0.00f, 0.90f, 1.00f }); SwRect(v, x0 + 9, yy + 5, 1, rowH - 10, Col { 0.00f, 0.45f, 0.55f }); } /* tmt: 左に光る線 */''')

# header separator: cyan -> magenta gradient line
o = 'SwRect(v, x0 + 12, y0 + head - 6, MW - 24, 1, border);'
assert ui.count(o) == 1
ui = ui.replace(o, 'SwNeonLine(v, x0 + 12, y0 + head - 6, MW - 24);')

# title: neon wordmark with a magenta shadow, version in magenta
o = '''            SwStrBig(v, menu_text::kTITLE, tx, ty2, tsc, whiteG, 1.0f);'''
assert ui.count(o) == 1
ui = ui.replace(o, '''            SwStrBig(v, menu_text::kTITLE, tx + 2, ty2 + 1, tsc, Col { 1.00f, 0.17f, 0.84f }, 0.55f); /* tmt: マゼンタの影 */
            SwStrBig(v, menu_text::kTITLE, tx, ty2, tsc, Col { 0.00f, 0.92f, 1.00f }, 1.0f);''')
o = '''ty2 + static_cast<int>(menu_text::kTITLE.h * tsc) - font_glyphs::kLineH, 120, dimG); } /* ロゴの右に小さくバージョン */'''
assert ui.count(o) == 1
ui = ui.replace(o, '''ty2 + static_cast<int>(menu_text::kTITLE.h * tsc) - font_glyphs::kLineH, 120, Col { 1.00f, 0.45f, 0.90f }); } /* ロゴの右に小さくバージョン (マゼンタ) */''')

# helper for the gradient line
o = '    void SwDrawMenu(int, int) {'
assert ui.count(o) == 1
ui = ui.replace(o, '''    static void SwNeonLine(Cv& v, int x, int y, int w) { /* tmt: シアン → マゼンタの細い線 */
        for (int i = 0; i < w; i++) { const float t = w > 1 ? static_cast<float>(i) / static_cast<float>(w - 1) : 0.0f;
            SwRect(v, x + i, y, 1, 1, Col { 0.00f + 1.00f * t, 0.90f - 0.73f * t, 1.00f - 0.16f * t }); }
    }
''' + o)

s = s[:a] + ui + s[b:]
open(p, 'w', encoding='utf-8').write(s)
print('theme applied')
