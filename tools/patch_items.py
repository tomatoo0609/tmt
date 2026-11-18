#!/usr/bin/env python3
"""Menu rows for the v771 aim curve / aim tune / news skip, and the news-skip hooks (12.0.0)."""
import sys
p = sys.argv[1]
s = open(p, encoding='utf-8').read()
def rep(old, new, cnt=1):
    global s
    n = s.count(old); assert n == cnt, f'{n}: {old[:90]!r}'
    s = s.replace(old, new)

# ids
rep('    constexpr int kTplN = 30;', '    constexpr int kDynAimCurve = 520, kDynAimTune = 521, kDynAimNow = 522, kDynNewsSkip = 523; /* tmt: v771 の項目 */\n    volatile bool g_NewsSkip = true; volatile u32 g_NewsHits = 0; /* 広場のニュースを飛ばす (既定 ON) */\n    constexpr int kTplN = 30;')
# categories: cheats get the curve rows, tools get news skip
rep('{ kDynGaAim, kDynGcAim, kDynGcTgt, kDynGcA, kDynGcCircOn, kDynGcCirc, kDynGcRange, kDynGcLead, -1, -1, -1, -1, -1, -1 } /* ガチチート */',
    '{ kDynGaAim, kDynGcAim, kDynGcTgt, kDynGcA, kDynGcCircOn, kDynGcCirc, kDynGcRange, kDynGcLead, kDynAimCurve, kDynAimTune, kDynAimNow, -1, -1, -1 } /* ガチチート (tmt: エイム曲線) */')
rep('{ kDynZr, kItFreeCam, kDynMacroOn, kDynMacroBtn, kDynMacroIv, kDynAddr, -1, -1,',
    '{ kDynZr, kItFreeCam, kDynMacroOn, kDynMacroBtn, kDynMacroIv, kDynAddr, kDynNewsSkip, -1,')
# input handlers
rep('        if (it == kDynGcLead) {', '''        if (it == kDynAimCurve) { int c = g_AimCurveSel % 9; if (act) c = (c + 1) % 9; g_AimCurveSel = c; int v = g_AimCurveY[c]; if (lf && v > -50) v--; if (rt && v < 50) v++; g_AimCurveY[c] = v; return; } /* tmt: L で点を選ぶ、左右で値 */
        if (it == kDynAimTune) { int v = g_AimTune; if (lf && v > -50) v--; if (rt && v < 50) v++; if (act) v = 0; g_AimTune = v; return; }
        if (it == kDynAimNow) return;
        if (it == kDynNewsSkip) { if (act || lf || rt) g_NewsSkip = !g_NewsSkip; return; }
        if (it == kDynGcLead) {''')
# rows
rep('                else if (i == kDynGcLead) {', '''                else if (i == kDynAimCurve) { const int c = g_AimCurveSel % 9; static char cn[40]; std::snprintf(cn, sizeof(cn), T("狙い高さ %dm", "Aim Curve %dm"), 1 + 3 * c); nm = cn; st = -1; std::snprintf(vb, sizeof(vb), "< %d >", static_cast<int>(g_AimCurveY[c])); }
                else if (i == kDynAimTune) { nm = T("全体の調整 (+下 -上)", "Aim Tune (+dn -up)"); st = -1; std::snprintf(vb, sizeof(vb), "< %d >", static_cast<int>(g_AimTune)); }
                else if (i == kDynAimNow) { nm = T("今", "now"); st = -1; const float d = g_AimNowD; std::snprintf(vb, sizeof(vb), "%.1fm -> %.1f", static_cast<double>(d), static_cast<double>(AimUnitsFor(d) + static_cast<float>(g_AimTune))); }
                else if (i == kDynNewsSkip) { nm = T("広場のニュースを飛ばす", "Skip plaza news"); st = g_NewsSkip ? 1 : 0; }
                else if (i == kDynGcLead) {''')
# redraw hash: the live distance row changes without input
rep('        mix(g_NetBlk ? 1u : 0u);', '        mix(static_cast<u32>(g_AimCurveSel)); mix(static_cast<u32>(g_AimTune)); for (int q = 0; q < 9; q++) mix(static_cast<u32>(g_AimCurveY[q])); mix(g_NewsSkip ? 1u : 0u); if (g_MenuOpen && g_Cat == 6) mix(static_cast<u32>(g_AimNowD * 10.0f));\n        mix(g_NetBlk ? 1u : 0u);')
# settings
rep('for (int i = 0; i < 9; i++) o[n++] = { kCv[i], g_AimCurveY[i] }; o[n++] = { "aimTune", g_AimTune };',
    'for (int i = 0; i < 9; i++) o[n++] = { kCv[i], g_AimCurveY[i] }; o[n++] = { "aimTune", g_AimTune }; o[n++] = { "newsSkip", g_NewsSkip ? 1 : 0 };')
rep('                if (get("aimTune", &v) && v >= -50 && v <= 50) g_AimTune = v;',
    '                if (get("aimTune", &v) && v >= -50 && v <= 50) g_AimTune = v; if (get("newsSkip", &v)) g_NewsSkip = v != 0;')
# hooks: the game copies *(*(x0+0x28)+0x48 / +0x49) ("news pending") into x0+0x30 and plays the news when set. Clear it first.
rep('/* tmt: セーブデータの入れ替え・削除の機能 (Inkdraw 版 700〜704) は入れない */\n', '''/* tmt: セーブデータの入れ替え・削除の機能 (Inkdraw 版 700〜704) は入れない */
/* tmt (v771 のニューススキップ): 広場に入ったときのニュースの 2 か所 (12.0.0: main+0x3B5717C / +0x3B5845C)。
   どちらも *(*(x0+0x28) + 0x48 / 0x49) の「ニュースあり」を x0+0x30 に写して、立っていれば流す。呼ぶ前に 0 にする */
namespace { void NewsClear(uintptr_t a0, u32 off) {
    if (!g_NewsSkip || a0 < 0x10000 || (a0 & 7) || !Readable(a0 + 0x28, 8)) return;
    const uintptr_t o = *reinterpret_cast<const uintptr_t*>(a0 + 0x28);
    if (o < 0x10000 || (o & 3) || !Readable(o + off, 1)) return;
    if (*reinterpret_cast<volatile u8*>(o + off)) { *reinterpret_cast<volatile u8*>(o + off) = 0; g_NewsHits = g_NewsHits + 1; }
} }
HOOK_DEFINE_TRAMPOLINE(NewsHookA) { static u64 Callback(uintptr_t a0) { NewsClear(a0, 0x48); return Orig(a0); } };
HOOK_DEFINE_TRAMPOLINE(NewsHookB) { static u64 Callback(uintptr_t a0) { NewsClear(a0, 0x49); return Orig(a0); } };
''')
rep('    if (INKDRAW_HIDDRAW && verOk && g_OffPaintRequest == 0x37246F0) {', '''    if (verOk) { const uintptr_t mb = exl::util::GetModuleInfo(exl::util::ModuleIndex::Main).m_Total.m_Start;
        if (*reinterpret_cast<const u32*>(mb + 0x3B5717C) == 0xD100C3FFu && *reinterpret_cast<const u32*>(mb + 0x3B5718C) == 0xF9401408u) NewsHookA::InstallAtOffset(0x3B5717C);
        if (*reinterpret_cast<const u32*>(mb + 0x3B5845C) == 0xD100C3FFu && *reinterpret_cast<const u32*>(mb + 0x3B5846C) == 0xF9401408u) NewsHookB::InstallAtOffset(0x3B5845C); }
    if (INKDRAW_HIDDRAW && verOk && g_OffPaintRequest == 0x37246F0) {''')
open(p, 'w', encoding='utf-8').write(s)
print('ok')
