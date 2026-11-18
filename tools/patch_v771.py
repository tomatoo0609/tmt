#!/usr/bin/env python3
"""Re-implement the v707..v771 core changes on top of the ported v706 source."""
import sys
p = sys.argv[1]
s = open(p, encoding='utf-8').read()

def rep(old, new, cnt=1):
    global s
    n = s.count(old)
    assert n == cnt, f'expected {cnt} got {n}: {old[:80]!r}'
    s = s.replace(old, new)

# ---------------------------------------------------------------- 1. enemy instant detection (hook side)
rep('''HOOK_DEFINE_TRAMPOLINE(BodyCalcHook) {''', '''namespace {
    /* tmt (v771 の即検出を作り直し): BodyCalcHook はゲームのスレッドで毎フレーム全キャラの部品を 1 体ずつ処理する。
       部品 +0x108 がそのキャラの体 (vtable kEnVt)。ここで集めれば、メモリ全体を探さなくても、リスポーン直後から敵が出る。 */
    constexpr uintptr_t kEnHkVt = 0x5D3F078; /* = kEnVt (12.0.0) */
    constexpr int kEnHkMax = 32;
    volatile uintptr_t g_EnHkOb[kEnHkMax] = {}; volatile u64 g_EnHkT[kEnHkMax] = {};
    volatile uintptr_t g_EnHkSelf = 0; volatile u64 g_EnHkSelfT = 0, g_EnHkLastT = 0; volatile u32 g_EnHkNotes = 0, g_EnHkUses = 0;
    volatile int g_EnHkLk = 0;
    volatile uintptr_t g_EnHkLo = 0, g_EnHkHi = 0; volatile u32 g_EnHkGen = 0; /* 読めると分かっている領域 (svcQueryMemory を毎回呼ばない) */
    inline void EnHkLock() { while (__atomic_exchange_n(&g_EnHkLk, 1, __ATOMIC_ACQUIRE)) asm volatile("yield"); }
    inline void EnHkUnlock() { __atomic_store_n(&g_EnHkLk, 0, __ATOMIC_RELEASE); }
    extern volatile u32 g_LoadGen;
    bool EnHkReadable(uintptr_t a, size_t n) {
        if (a < 0x10000 || (a & 7)) return false;
        if (g_EnHkGen != g_LoadGen) { g_EnHkGen = g_LoadGen; g_EnHkLo = 0; g_EnHkHi = 0; } /* ロードしたら覚え直す */
        const uintptr_t lo = g_EnHkLo, hi = g_EnHkHi;
        if (hi && a >= lo && a + n <= hi) return true;
        MemoryInfo mi; u32 pg;
        if (R_FAILED(svcQueryMemory(&mi, &pg, a))) return false;
        if (mi.type == MemType_Unmapped || !(mi.perm & Perm_R) || (mi.attr & 0xC)) return false;
        if (a + n > mi.addr + mi.size) return false;
        g_EnHkLo = mi.addr; g_EnHkHi = mi.addr + mi.size; return true;
    }
    void EnHkNote(uintptr_t a1) {
        if (a1 < 0x10000 || (a1 & 7) || !EnHkReadable(a1 + 0x108, 8)) return;
        const uintptr_t ob = *reinterpret_cast<const volatile uintptr_t*>(a1 + 0x108);
        if (ob < 0x10000 || (ob & 7) || !EnHkReadable(ob, 0x10B0)) return;
        if (*reinterpret_cast<const volatile uintptr_t*>(ob) != MainBase() + kEnHkVt) return;
        const u64 now = NowTick();
        static u64 selfT = 0; static uintptr_t selfC = 0; /* 自分の部品は 1 フレームに 1 回だけ求める */
        if (now - selfT > 38400ull) { selfT = now; selfC = (g_OffPlayerMgrPtr == 0x6057568) ? BfSelfComp() : 0; }
        EnHkLock();
        int k = -1, old = 0; u64 ot = ~0ull;
        for (int i = 0; i < kEnHkMax; i++) { if (g_EnHkOb[i] == ob) { k = i; break; } const u64 t = g_EnHkOb[i] ? g_EnHkT[i] : 0; if (t < ot) { ot = t; old = i; } }
        if (k < 0) { k = old; g_EnHkOb[k] = ob; }
        g_EnHkT[k] = now; g_EnHkLastT = now; g_EnHkNotes = g_EnHkNotes + 1;
        if (selfC && selfC == a1) { g_EnHkSelf = ob; g_EnHkSelfT = now; }
        EnHkUnlock();
    }
}
HOOK_DEFINE_TRAMPOLINE(BodyCalcHook) {''')
rep('''        g_BfCalls++; ScnTick(); /* 版 653 */''', '''        g_BfCalls++; ScnTick(); /* 版 653 */
        EnHkNote(a1); /* tmt: 敵の即検出 */''')

# ---------------------------------------------------------------- 2. enemy list from the hook (consumer side)
rep('''    constexpr uintptr_t kRtVt = 0x5D43018;''', '''    constexpr uintptr_t kRtVt = 0x5D43018;
    static_assert(kEnVt == kEnHkVt, "kEnHkVt must match kEnVt");
    /* 新しい一覧 te[] を入れる。前の一覧と同じ物は、「動いたか」などの記録を引きつぐ (走査でもフックでも同じ処理) */
    static void EnAdopt(const uintptr_t* te, int ne) {
        if (ne > kEnMax) ne = kEnMax;
        static uintptr_t po[kEnMax]; static float pf[kEnMax][3]; static u8 pm[kEnMax], pk[kEnMax]; const int pn = g_EnN < kEnMax ? g_EnN : kEnMax;
        static float pr[kEnMax][3]; static u64 pt2[kEnMax];
        for (int k = 0; k < pn; k++) { po[k] = g_EnObj[k]; std::memcpy(pf[k], g_EnFirst[k], 12); pm[k] = g_EnMoved[k]; pk[k] = g_EnFirstOk[k]; std::memcpy(pr[k], g_EnRef[k], 12); pt2[k] = g_EnMoveT[k]; }
        for (int k = 0; k < ne; k++) { g_EnMoved[k] = 0; g_EnFirstOk[k] = 0; g_EnMoveT[k] = 0; for (int q = 0; q < pn; q++) if (po[q] == te[k]) { g_EnMoved[k] = pm[q]; g_EnFirstOk[k] = pk[q]; std::memcpy(g_EnRef[k], pr[q], 12); g_EnMoveT[k] = pt2[q]; std::memcpy(g_EnFirst[k], pf[q], 12); break; } }
        for (int k = 0; k < ne; k++) { g_EnPool[k] = 0; if (!Readable(te[k], kEnOff + 12)) continue; const float* a = reinterpret_cast<const float*>(te[k] + kEnOff);
            for (int q = 0; q < ne; q++) { if (q == k || !Readable(te[q], kEnOff + 12)) continue; const float* b2 = reinterpret_cast<const float*>(te[q] + kEnOff); const float dx = a[0] - b2[0], dy = a[1] - b2[1], dz = a[2] - b2[2]; if (dx * dx + dy * dy + dz * dz < 0.0001f) { g_EnPool[k] = 1; break; } } }
        for (int k = 0; k < ne; k++) g_EnObj[k] = te[k]; g_EnN = ne;
    }
    /* フックが集めた体から一覧を作る。1 秒以内に処理された体だけ。使えないとき (フックが止まっている等) は false = 従来の走査 */
    static bool EnHkBuild() {
        if (!g_BfHooked) return false;
        const u64 now = NowTick(); const u64 last = g_EnHkLastT;
        if (!last || now - last > 9600000ull) return false;
        uintptr_t te[kEnMax]; int ne = 0; uintptr_t self = 0;
        EnHkLock();
        for (int i = 0; i < kEnHkMax && ne < kEnMax; i++) { const uintptr_t ob = g_EnHkOb[i]; if (ob && now - g_EnHkT[i] < 19200000ull) te[ne++] = ob; }
        if (g_EnHkSelfT && now - g_EnHkSelfT < 19200000ull) self = g_EnHkSelf;
        EnHkUnlock();
        if (ne == 0) return false;
        for (int i = 1; i < ne; i++) { const uintptr_t v = te[i]; int j = i - 1; while (j >= 0 && te[j] > v) { te[j + 1] = te[j]; j--; } te[j + 1] = v; } /* 走査と同じ並び (アドレス順) */
        if (self) g_EnSelf = self;
        g_EnHkUses = g_EnHkUses + 1;
        bool same = ne == g_EnN; for (int k = 0; k < ne && same; k++) same = g_EnObj[k] == te[k];
        if (!same) EnAdopt(te, ne);
        return true;
    }''')

# scan: use the hook list when it is alive; only fall back to the full memory scan otherwise
rep('''        const bool needBal = lookBal || g_BalSumUntil != 0, needEn = (lookEn || needBal || g_EnInkOn || NowTick() < g_EnWantUntil) && !lobby, needRt = false;''',
    '''        const bool needBal = lookBal || g_BalSumUntil != 0, needEn0 = (lookEn || needBal || g_EnInkOn || NowTick() < g_EnWantUntil) && !lobby, needRt = false;
        const bool enHk = needEn0 && EnHkBuild(); /* tmt: フックで集めた体が使えるなら、敵はメモリを探さない */
        const bool needEn = needEn0 && !enHk;''')

# the scan's own adopt block -> shared helper
a = s.index('            if (scEn) { /* 前の一覧と同じ物は、「動いたか」の記録を引きつぐ */')
b = s.index('for (int k = 0; k < ne; k++) g_EnObj[k] = te[k]; g_EnN = ne; }', a) + len('for (int k = 0; k < ne; k++) g_EnObj[k] = te[k]; g_EnN = ne; }')
s = s[:a] + '            if (scEn) EnAdopt(te, ne); /* 前の一覧と同じ物は、「動いたか」の記録を引きつぐ */' + s[b:]

# ---------------------------------------------------------------- 3. SelfPos: game-identified own body first + once-per-frame cache
rep('''    bool SelfPos(float* o) {
        if (FootValid()) { std::memcpy(o, reinterpret_cast<const void*>(g_FootPos), 12); o[1] += kLiveHeight; return true; } /* 今までの座標と同じ高さ (床 + kLiveHeight) にそろえる */
        const uintptr_t lp = __atomic_load_n(&g_LivePos, __ATOMIC_ACQUIRE); if (!LiveValid()) return false;
        std::memcpy(o, reinterpret_cast<const void*>(lp), 12); return true;
    }''', '''    bool SelfPos(float* o) {
        /* tmt (v771 の迷子対策): 1) ゲーム自身が「自分」と判定した体 (BodyCalcHook) 2) 足元 3) 生の座標。
           結果は 5ms だけ覚えて、1 フレームに何十回も svcQueryMemory を呼ばない (カクつき対策) */
        static volatile int lk = 0; static u64 ct = 0; static float cv[3]; static bool cok = false;
        const u64 now = NowTick();
        while (__atomic_exchange_n(&lk, 1, __ATOMIC_ACQUIRE)) asm volatile("yield");
        if (ct && now - ct < 96000ull) { const bool r = cok; if (r) std::memcpy(o, cv, 12); __atomic_store_n(&lk, 0, __ATOMIC_RELEASE); return r; }
        __atomic_store_n(&lk, 0, __ATOMIC_RELEASE);
        float v[3]; bool ok = false;
        { const uintptr_t sb = g_EnHkSelf; const u64 st = g_EnHkSelfT;
          if (sb && st && now - st < 6400000ull && EnHkReadable(sb, 0x20) && *reinterpret_cast<const uintptr_t*>(sb) == MainBase() + kEnHkVt) {
              const float* f = reinterpret_cast<const float*>(sb + 0x10);
              if (f[0] == f[0] && f[1] == f[1] && f[2] == f[2] && f[0] > -5000.0f && f[0] < 5000.0f && f[1] > -5000.0f && f[1] < 5000.0f && f[2] > -5000.0f && f[2] < 5000.0f && !(f[0] == 0.0f && f[1] == 0.0f && f[2] == 0.0f)) {
                  v[0] = f[0]; v[1] = f[1] + kLiveHeight; v[2] = f[2]; ok = true; } } }
        if (!ok && FootValid()) { std::memcpy(v, reinterpret_cast<const void*>(g_FootPos), 12); v[1] += kLiveHeight; ok = true; } /* 今までの座標と同じ高さ (床 + kLiveHeight) にそろえる */
        if (!ok) { const uintptr_t lp = __atomic_load_n(&g_LivePos, __ATOMIC_ACQUIRE); if (LiveValid()) { std::memcpy(v, reinterpret_cast<const void*>(lp), 12); ok = true; } }
        while (__atomic_exchange_n(&lk, 1, __ATOMIC_ACQUIRE)) asm volatile("yield");
        ct = now; cok = ok; if (ok) std::memcpy(cv, v, 12);
        __atomic_store_n(&lk, 0, __ATOMIC_RELEASE);
        if (ok) std::memcpy(o, v, 12);
        return ok;
    }''')

# ---------------------------------------------------------------- 4. aim curve (9 points, PCHIP, same as v771) + aimTune
rep('''    static float LookAtan2(float y, float x) { /* 度 (おおよそ) */''', '''    /* tmt (v771 の狙い高さ曲線): 距離 1,4,7,...,25m の 9 点。値 1 = 0.1m 下 (マイナスで上)。間は PCHIP (形を保つ 3 次補間) */
    volatile int g_AimCurveY[9] = { -10, -8, -1, -1, 2, 8, 13, 15, 20 }; volatile int g_AimTune = 0; volatile float g_AimNowD = 0.0f; volatile int g_AimCurveSel = 0;
    float AimUnitsFor(float d) {
        float y[9]; for (int i = 0; i < 9; i++) y[i] = static_cast<float>(g_AimCurveY[i]);
        if (!(d > 1.0f)) return y[0];
        if (d >= 25.0f) return y[8];
        float dl[8]; for (int k = 0; k < 8; k++) dl[k] = (y[k + 1] - y[k]) / 3.0f;
        float m[9]; m[0] = dl[0]; m[8] = dl[7];
        for (int k = 1; k < 8; k++) m[k] = (dl[k - 1] * dl[k] > 0.0f) ? 2.0f / (1.0f / dl[k - 1] + 1.0f / dl[k]) : 0.0f;
        int k = static_cast<int>((d - 1.0f) / 3.0f); if (k > 7) k = 7; if (k < 0) k = 0;
        const float t = (d - (1.0f + 3.0f * static_cast<float>(k))) / 3.0f, t2 = t * t, t3 = t2 * t;
        return (2.0f * t3 - 3.0f * t2 + 1.0f) * y[k] + (t3 - 2.0f * t2 + t) * 3.0f * m[k] + (-2.0f * t3 + 3.0f * t2) * y[k + 1] + (t3 - t2) * 3.0f * m[k + 1];
    }
    static float LookAtan2(float y, float x) { /* 度 (おおよそ) */''')
rep('''const float dx = (Tt[0] - rgx * kAimLeft) - Lb[0], dyy = (Tt[1] - kLiveHeight + 0.9f) - Lb[1], dz = (Tt[2] - rgz * kAimLeft) - Lb[2], hd = __builtin_sqrtf(dx * dx + dz * dz);''',
    '''const float dx = (Tt[0] - rgx * kAimLeft) - Lb[0], dz = (Tt[2] - rgz * kAimLeft) - Lb[2], hd = __builtin_sqrtf(dx * dx + dz * dz);
                    g_AimNowD = hd; const float dyy = (Tt[1] - kLiveHeight + 0.9f - 0.1f * (AimUnitsFor(hd) + static_cast<float>(g_AimTune))) - Lb[1]; /* tmt: 距離ごとの高さ曲線 (v771) */''')
rep('''if (hd > 3.4f) cr *= 0.25f; if (cr > 25.0f) cr = 25.0f; ep = LookAtan2(dyy, hd) + cr - pit;''',
    '''if (hd > 3.4f) cr = 0.0f; if (cr > 25.0f) cr = 25.0f; ep = LookAtan2(dyy, hd) + cr - pit; /* tmt: 遠い側は曲線にまかせる (v771) */''')

# ---------------------------------------------------------------- 5. CamDir3 flip guard (視点ブルブル対策)
rep('''    bool CamDir3(float* o) {
        { float e[3];''', '''    bool CamDir3Raw(float* o);
    bool CamDir3(float* o) {
        /* tmt (v771 のブルブル対策): 読んだ向きが 1 回だけ急に反対を向いたら (読み間違い)、前の向きを使う。3 回続いたら本物とみなす */
        static float last[3]; static bool have = false; static int flips = 0;
        float c[3]; if (!CamDir3Raw(c)) return false;
        if (have) { const float dt = c[0] * last[0] + c[1] * last[1] + c[2] * last[2];
            if (dt < 0.0f && ++flips < 3) { o[0] = last[0]; o[1] = last[1]; o[2] = last[2]; return true; } }
        flips = 0; have = true; last[0] = c[0]; last[1] = c[1]; last[2] = c[2]; o[0] = c[0]; o[1] = c[1]; o[2] = c[2]; return true;
    }
    bool CamDir3Raw(float* o) {
        { float e[3];''')

# ---------------------------------------------------------------- 6. settings: room for more keys + bounds check (KV kv[24] overflowed -> snprintf(null) crash)
rep('auto cur = [&](KV* o) { int n = 0;', 'auto cur = [&](KV* o) { int n = 0; /* tmt: 入れ物は kKvMax。足りなくなったら kKvMax を増やす */')
rep('o[n++] = { "uiX", g_UiX }; o[n++] = { "uiY", g_UiY }; return n; };',
    'o[n++] = { "uiX", g_UiX }; o[n++] = { "uiY", g_UiY };\n'
    '            static const char* const kCv[9] = { "curve0", "curve1", "curve2", "curve3", "curve4", "curve5", "curve6", "curve7", "curve8" };\n'
    '            for (int i = 0; i < 9; i++) o[n++] = { kCv[i], g_AimCurveY[i] }; o[n++] = { "aimTune", g_AimTune };\n'
    '            return n; };')
s = s.replace('KV kv[24]; const int m = cur(kv);', 'KV kv[kKvMax]; const int m = cur(kv);')
rep('        struct KV { const char* k; int v; };', '        struct KV { const char* k; int v; }; constexpr int kKvMax = 64;')
rep('''                if (get("aimbot", &v) && v) { g_LookSlot = 6;''', '''                { char nm[8] = { 'c', 'u', 'r', 'v', 'e', '0', 0, 0 }; for (int i = 0; i < 9; i++) { nm[5] = static_cast<char>('0' + i); if (get(nm, &v) && v >= -100 && v <= 100) g_AimCurveY[i] = v; } }
                if (get("aimTune", &v) && v >= -50 && v <= 50) g_AimTune = v;
                if (get("aimbot", &v) && v) { g_LookSlot = 6;''')
rep('''        char t[1024]; int L = std::snprintf(t, sizeof(t), "{\\n");
        for (int k = 0; k < m; k++) L += std::snprintf(t + L, sizeof(t) - L, "  \\"%s\\": %d%s\\n", kv[k].k, kv[k].v, k + 1 < m ? "," : "");''',
    '''        char t[2048]; int L = std::snprintf(t, sizeof(t), "{\\n");
        for (int k = 0; k < m && L < static_cast<int>(sizeof(t)) - 64; k++) L += std::snprintf(t + L, sizeof(t) - L, "  \\"%s\\": %d%s\\n", kv[k].k ? kv[k].k : "?", kv[k].v, k + 1 < m ? "," : "");''')

open(p, 'w', encoding='utf-8').write(s)
print('ok')
