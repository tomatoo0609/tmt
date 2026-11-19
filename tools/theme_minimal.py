#!/usr/bin/env python3
"""tmt theme v2: minimal / flat / translucent graphite, one soft accent. Applied on top of theme_neon.py output."""
import sys, re
p = sys.argv[1]
s = open(p, encoding='utf-8').read()
a = s.index('    void SwSwitch(Cv& v, int rx, int cy, bool on, Col ac, Col off, Col knob) {')
b = s.index('extern "C" void exl_main')
ui = s[a:b]
def rep(o, n, cnt=1):
    global ui
    c = ui.count(o); assert c == cnt, (c, o[:90]); ui = ui.replace(o, n)

ACC = 'Col { 0.56f, 0.84f, 0.78f }'   # soft mint, the only accent
# palette
rep('const Col panel { 0.030f, 0.035f, 0.060f }, border { 0.00f, 0.55f, 0.70f }, whiteG { 0.88f, 0.95f, 1.00f }, accent { 0.00f, 0.90f, 1.00f }, good { 0.55f, 1.00f, 0.80f }; /* tmt: ダーク + ネオン */',
    'const Col panel { 0.055f, 0.057f, 0.064f }, border { 0.17f, 0.175f, 0.19f }, whiteG { 0.95f, 0.95f, 0.96f }, accent ' + ACC[4:] + ', good ' + ACC[4:] + '; /* tmt: 平らな黒鉛色 (半透明) + 控えめなミント 1 色 */')
rep('const Col dimG { 0.46f, 0.56f, 0.70f }, selBg { 0.00f, 0.20f, 0.28f }, ink { 0.40f, 0.97f, 1.00f }; /* 選択: 暗いシアンの帯 + 光る文字 */',
    'const Col dimG { 0.50f, 0.52f, 0.56f }, selBg { 0.115f, 0.12f, 0.135f }, ink { 1.0f, 1.0f, 1.0f }; /* 選択: 少し明るい帯 + 白い文字 */')
rep('const Col panel { 0.030f, 0.035f, 0.060f }, frame { 0.00f, 0.90f, 1.00f }, line { 1.00f, 0.17f, 0.84f }, white { 0.88f, 0.95f, 1.00f }, dim { 0.46f, 0.56f, 0.70f }; /* tmt */',
    'const Col panel { 0.055f, 0.057f, 0.064f }, frame { 0.17f, 0.175f, 0.19f }, line ' + ACC[4:] + ', white { 0.95f, 0.95f, 0.96f }, dim { 0.50f, 0.52f, 0.56f }; /* tmt */')
# selection marker: thin 2px accent tick
rep('if (!side) { SwRect(v, x0 + 6, yy + 5, 3, rowH - 10, Col { 0.00f, 0.90f, 1.00f }); SwRect(v, x0 + 9, yy + 5, 1, rowH - 10, Col { 0.00f, 0.45f, 0.55f }); } /* tmt: 左に光る線 */',
    'if (!side) SwRect(v, x0 + 6, yy + 10, 2, rowH - 20, ' + ACC + '); /* tmt: 左に細い線 1 本 */')
# title: small, white, no glow; version grey
rep('''            SwStrBig(v, menu_text::kTITLE, tx + 2, ty2 + 1, tsc, Col { 1.00f, 0.17f, 0.84f }, 0.55f); /* tmt: マゼンタの影 */
            SwStrBig(v, menu_text::kTITLE, tx, ty2, tsc, Col { 0.00f, 0.92f, 1.00f }, 1.0f);''',
    '''            SwStrBig(v, menu_text::kTITLE, tx, ty2, tsc, whiteG, 1.0f); /* tmt: 小さく白く */''')
rep('const float tsc = 1.25f; const int tx = x0 + 18, ty2 = y0 + 11;', 'const float tsc = 0.82f; const int tx = x0 + 18, ty2 = y0 + 15;')
rep('120, Col { 1.00f, 0.45f, 0.90f }); } /* ロゴの右に小さくバージョン (マゼンタ) */', '120, dimG); } /* ロゴの右に小さくバージョン */')
# frames: hairline graphite instead of neon; nearly square corners
rep('SwRRect(v, bx, y0, MW, MHc, 10.0f, Col { 1.00f, 0.17f, 0.84f }); /* tmt: 2 つ目はマゼンタのふち */', 'SwRRect(v, bx, y0, MW, MHc, 3.0f, border);')
ui = ui.replace('Col { 0.00f, 0.90f, 1.00f }', 'Col { 0.17f, 0.175f, 0.19f }')   # remaining neon frames -> hairline
ui = ui.replace('Col { 1.00f, 0.17f, 0.84f }', ACC)
ui = ui.replace(', 10.0f, ', ', 3.0f, ').replace(', 8.0f, ', ', 2.0f, ').replace('rowH - 2, 5.0f,', 'rowH - 2, 2.0f,')
ui = re.sub(r'(PRect pr \{[^}]*?), 10 \}', r'\1, 3 }', ui)
ui = re.sub(r'(PRect pr \{[^}]*?), 8 \}', r'\1, 2 }', ui)
# header separator: plain hairline
rep('''            SwRect(v, x + i, y, 1, 1, Col { 0.00f + 1.00f * t, 0.90f - 0.73f * t, 1.00f - 0.16f * t }); }''',
    '''            (void)t; SwRect(v, x + i, y, 1, 1, Col { 0.17f, 0.175f, 0.19f }); } /* tmt v2: 細い灰色の線 */''')
# selected-row variants of neon colours -> neutral
ui = ui.replace('Col { 0.40f, 0.97f, 1.00f }', 'Col { 1.0f, 1.0f, 1.0f }').replace('Col { 0.00f, 0.45f, 0.58f }', 'Col { 0.30f, 0.31f, 0.34f }')
ui = ui.replace('Col { 0.05f, 0.11f, 0.17f }', 'Col { 0.10f, 0.105f, 0.115f }').replace('Col { 0.07f, 0.10f, 0.16f }', 'Col { 0.13f, 0.135f, 0.15f }')
ui = ui.replace('Col { 0.14f, 0.26f, 0.36f }', 'Col { 0.20f, 0.21f, 0.23f }').replace('Col { 0.17f, 0.175f, 0.19f }', 'Col { 0.17f, 0.175f, 0.19f }')
ui = ui.replace('Col { 0.20f, 0.30f, 0.42f }', 'Col { 0.36f, 0.37f, 0.40f }').replace('Col { 0.20f, 0.75f, 0.85f }', 'Col { 0.62f, 0.64f, 0.68f }')
ui = ui.replace('Col { 0.30f, 0.85f, 0.95f }', 'Col { 0.80f, 0.81f, 0.84f }').replace('Col { 0.88f, 0.95f, 1.00f }', 'Col { 0.95f, 0.95f, 0.96f }')
# switch: minimal line + square (ON: filled accent right / OFF: grey outline left)
rep('''        SwRRect(v, rx - 38, cy - 9, 38, 18, 9.0f, MixC(off, ac, ep)); const int kx = rx - 38 + 3 + static_cast<int>(18.0f * ep + 0.5f); SwRRect(v, kx, cy - 7, 14, 14, 7.0f, knob);''',
    '''        (void)off; (void)ac; (void)knob; /* tmt: 細い線の上を動く小さな四角 */
        const Col onC ''' + ACC[4:] + ''', offC { 0.36f, 0.37f, 0.40f };
        SwRect(v, rx - 30, cy, 30, 1, Col { 0.24f, 0.25f, 0.27f });
        const int kx = rx - 30 + static_cast<int>(20.0f * ep + 0.5f); const Col kc = MixC(offC, onC, ep);
        if (ep > 0.5f) SwRect(v, kx, cy - 5, 10, 10, kc); else { SwRect(v, kx, cy - 5, 10, 1, kc); SwRect(v, kx, cy + 4, 10, 1, kc); SwRect(v, kx, cy - 5, 1, 10, kc); SwRect(v, kx + 9, cy - 5, 1, 10, kc); SwRect(v, kx + 1, cy - 4, 8, 8, Col { 0.055f, 0.057f, 0.064f }); }''')
# category list: no icons, two-digit index + name
o = ui[ui.index('            if (i >= 100 && i < kDyn) {'):]
o = o[:o.index('                continue;\n            }\n') + len('                continue;\n            }\n')]
ui = ui.replace(o, '''            if (i >= 100 && i < kDyn) { /* tmt: アイコンなし。小さな番号 + 名前 */
                char ix[8]; std::snprintf(ix, sizeof(ix), "%02d", j + 1);
                SwU8(v, ix, x0 + 18, y + ty, 30, sel ? accent : Col { 0.38f, 0.39f, 0.42f });
                const Col cc = (i - 100 == kCatUlt) ? Col { 1.0f, 0.42f, 0.42f } : (sel ? ink : Col { 0.82f, 0.83f, 0.86f });
                SwU8(v, kCatName[i - 100], x0 + 50, y + ty, 226, cc);
                continue;
            }
''')
s = s[:a] + ui + s[b:]
# title width used for the menu width
s = s.replace('static_cast<int>(static_cast<float>(menu_text::kTITLE.w) * 1.25f)', 'static_cast<int>(static_cast<float>(menu_text::kTITLE.w) * 0.82f)')
# translucent by default (new key so the migrated Inkdraw value 100 = opaque is not used)
s = s.replace('volatile int g_UiOpa = 80;', 'volatile int g_UiOpa = 62;').replace('g_PanQ = 4; g_UiOpa = 80;', 'g_PanQ = 4; g_UiOpa = 62;')
s = s.replace('if (act) v = 80; g_UiOpa = v; return; }', 'if (act) v = 62; g_UiOpa = v; return; }')
s = s.replace('o[n++] = { "uiOpa2", g_UiOpa };', 'o[n++] = { "tmtOpa", g_UiOpa };')
s = s.replace('get("uiOpa2", &v)', 'get("tmtOpa", &v)')
open(p, 'w', encoding='utf-8').write(s)
print('ok', s.count('"tmtOpa"'))
