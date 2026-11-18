/* 版 663: Banc (配置データ, BYML) に RivalAppearPoint (タコゾネスの出現点) を足す。
   元の BYML を作り直したコピーに、(1) キー表と文字列表を拡張 (2) 全ハッシュのキー番号を付け直す (3) 出現点の節を追加 (4) Actors 配列を 1 つ大きくする。
   少しでも想定と違えば nullptr を返す (そのときは元のまま使われる = 何も起きない)。 */
#pragma once
#include <cstdint>
#include <cstring>
#include <cstddef>
#include "oct_data.hpp"
namespace riv {
typedef uint8_t u8; typedef uint32_t u32;
typedef void* (*AllocFn)(size_t);
typedef void (*FreeFn)(void*);
static inline u32 r32(const u8* p) { u32 v; std::memcpy(&v, p, 4); return v; }
static inline void w32(u8* p, u32 v) { std::memcpy(p, &v, 4); }
static inline u32 r24(const u8* p) { return static_cast<u32>(p[0]) | (static_cast<u32>(p[1]) << 8) | (static_cast<u32>(p[2]) << 16); }
static inline void w24(u8* p, u32 v) { p[0] = static_cast<u8>(v); p[1] = static_cast<u8>(v >> 8); p[2] = static_cast<u8>(v >> 16); }
constexpr u32 kMaxOff = 0x2000000;
constexpr u32 kMaxHash = 1u << 18;
struct Scan { u8* bm; u32* hashes; u32 nh; u32* arrs; u32 na; u32 ext; u32 nstr; u32 nkey; bool ok; };
static inline bool Mark(Scan& s, u32 off) { const u32 b = off >> 2; if (s.bm[b >> 3] & (1u << (b & 7))) return false; s.bm[b >> 3] |= static_cast<u8>(1u << (b & 7)); return true; }
static inline void Ext(Scan& s, u32 e) { if (e > s.ext) s.ext = e; }
/* ノードを歩く (読むだけ)。type/val は親の持っている型と値 */
static bool Visit(const u8* d, u8 type, u32 val, Scan& s, int depth) {
    if (depth > 40) return false;
    switch (type) {
    case 0xa0: return val < s.nstr;
    case 0xd0: case 0xd1: case 0xd2: case 0xd3: case 0xff: return true;
    case 0xd4: case 0xd5: case 0xd6: if (val == 0 || val >= kMaxOff) return false; Ext(s, val + 8); return true;
    case 0xa1: { if (val == 0 || val >= kMaxOff) return false; const u32 sz = r32(d + val); if (sz > kMaxOff) return false; Ext(s, val + 4 + sz); return true; }
    case 0xc0: {
        if (val == 0 || val >= kMaxOff || (val & 3)) return false;
        if (!Mark(s, val)) return true;
        const u32 n = r24(d + val + 1); if (n > 0x100000) return false;
        if (s.na >= kMaxHash) return false; s.arrs[s.na++] = val;
        const u32 po = val + 4 + ((n + 3) & ~3u); Ext(s, po + 4 * n);
        for (u32 i = 0; i < n; i++) if (!Visit(d, d[val + 4 + i], r32(d + po + 4 * i), s, depth + 1)) return false;
        return true; }
    case 0xc1: {
        if (val == 0 || val >= kMaxOff || (val & 3)) return false;
        if (!Mark(s, val)) return true;
        const u32 n = r24(d + val + 1); if (n > 0x100000) return false;
        Ext(s, val + 4 + 8 * n);
        if (s.nh >= kMaxHash) return false; s.hashes[s.nh++] = val;
        for (u32 i = 0; i < n; i++) { const u8* e = d + val + 4 + 8 * i; if (r24(e) >= s.nkey) return false; if (!Visit(d, e[3], r32(e + 4), s, depth + 1)) return false; }
        return true; }
    default: return false;
    }
}
struct Out { u8* d; u32 len, cap; bool ok; };
static inline u32 Al(Out& o) { while (o.len & 3) { if (o.len >= o.cap) { o.ok = false; return o.len; } o.d[o.len++] = 0; } return o.len; }
static inline u8* Take(Out& o, u32 n) { if (o.len + n > o.cap) { o.ok = false; return nullptr; } u8* p = o.d + o.len; o.len += n; return p; }
struct Tabs {
    const char* key[400]; u32 nkey;       /* 新しいキー表 (並べ替え済み) */
    u32 keyMap[2048];                      /* 古い番号 -> 新しい番号 */
    const char* str[2048]; u32 nstr;      /* 新しい文字列表 (並べ替え済み。ゲームは文字列表も並んでいることを確かめる) */
    u32 strMap[2048];                      /* 古い文字列番号 -> 新しい番号 */
};
static inline const char* TabStr(const u8* d, u32 tab, u32 i) { return reinterpret_cast<const char*>(d + tab + r32(d + tab + 4 + 4 * i)); }
static inline int FindKey(const Tabs& t, const char* k) { for (u32 i = 0; i < t.nkey; i++) if (std::strcmp(t.key[i], k) == 0) return static_cast<int>(i); return -1; }
static inline int FindStr(const Tabs& t, const char* k) { for (u32 i = 0; i < t.nstr; i++) if (std::strcmp(t.str[i], k) == 0) return static_cast<int>(i); return -1; }
static u32 PutTable(Out& o, const char* const* s, u32 n) {
    Al(o); const u32 at = o.len; u32 sz = 4 + 4 * (n + 1); for (u32 i = 0; i < n; i++) sz += static_cast<u32>(std::strlen(s[i])) + 1;
    u8* p = Take(o, sz); if (!p) return 0;
    p[0] = 0xc2; w24(p + 1, n); u32 pos = 4 + 4 * (n + 1);
    for (u32 i = 0; i < n; i++) { w32(p + 4 + 4 * i, pos); const u32 L = static_cast<u32>(std::strlen(s[i])) + 1; std::memcpy(p + pos, s[i], L); pos += L; }
    w32(p + 4 + 4 * n, pos); Al(o); return at;
}
/* 出現点 (octByaml) の節を、元の BYML のキー表・文字列表に合わせて複製する */
static bool Clone(const u8* f, const u8* fkt, const u8* fst, u8 type, u32 val, Tabs& t, Out& o, u8* rt, u32* rv) {
    switch (type) {
    case 0xa0: { const char* s = reinterpret_cast<const char*>(fst + r32(fst + 4 + 4 * val)); const int i = FindStr(t, s); if (i < 0) return false; *rt = type; *rv = static_cast<u32>(i); return true; }
    case 0xd0: case 0xd1: case 0xd2: case 0xd3: case 0xff: *rt = type; *rv = val; return true;
    case 0xc0: {
        const u32 n = r24(f + val + 1); const u32 po = val + 4 + ((n + 3) & ~3u);
        u8 ty[64]; u32 vv[64]; if (n > 64) return false;
        for (u32 i = 0; i < n; i++) if (!Clone(f, fkt, fst, f[val + 4 + i], r32(f + po + 4 * i), t, o, &ty[i], &vv[i])) return false;
        Al(o); const u32 at = o.len; u8* p = Take(o, 4 + ((n + 3) & ~3u) + 4 * n); if (!p) return false;
        p[0] = 0xc0; w24(p + 1, n); std::memset(p + 4, 0, (n + 3) & ~3u); for (u32 i = 0; i < n; i++) { p[4 + i] = ty[i]; w32(p + 4 + ((n + 3) & ~3u) + 4 * i, vv[i]); }
        *rt = 0xc0; *rv = at; return true; }
    case 0xc1: {
        const u32 n = r24(f + val + 1); struct E { u32 k; u8 ty; u32 v; } e[32]; if (n > 32) return false;
        for (u32 i = 0; i < n; i++) { const u8* q = f + val + 4 + 8 * i; const char* kn = reinterpret_cast<const char*>(fkt + r32(fkt + 4 + 4 * r24(q))); const int ki = FindKey(t, kn); if (ki < 0) return false;
            e[i].k = static_cast<u32>(ki); if (!Clone(f, fkt, fst, q[3], r32(q + 4), t, o, &e[i].ty, &e[i].v)) return false; }
        for (u32 a = 1; a < n; a++) for (u32 b = a; b > 0 && e[b - 1].k > e[b].k; b--) { E x = e[b]; e[b] = e[b - 1]; e[b - 1] = x; }
        Al(o); const u32 at = o.len; u8* p = Take(o, 4 + 8 * n); if (!p) return false;
        p[0] = 0xc1; w24(p + 1, n); for (u32 i = 0; i < n; i++) { w24(p + 4 + 8 * i, e[i].k); p[4 + 8 * i + 3] = e[i].ty; w32(p + 4 + 8 * i + 4, e[i].v); }
        *rt = 0xc1; *rv = at; return true; }
    default: return false;
    }
}
/* 戻り値: 新しい BYML (alloc した領域。使い終わっても解放しない) / nullptr。err: 失敗の理由 (1..9) */
static u8* AddRival(const u8* src, int count, AllocFn alloc, FreeFn freeF, u32* outLen, u32* actorsN, int* err) {
    *err = 0; *actorsN = 0; if (count < 1 || count > 16) { *err = 1; return nullptr; }
    if (src[0] != 'Y' || src[1] != 'B') { *err = 2; return nullptr; }
    const u32 kt = r32(src + 4), st = r32(src + 8), root = r32(src + 12);
    if (!kt || !st || !root || kt >= kMaxOff || st >= kMaxOff || root >= kMaxOff || src[kt] != 0xc2 || src[st] != 0xc2 || src[root] != 0xc1) { *err = 2; return nullptr; }
    const u32 nk = r24(src + kt + 1), ns = r24(src + st + 1); if (nk == 0 || nk > 1500 || ns > 1900) { *err = 2; return nullptr; }
    int actIdx = -1; for (u32 i = 0; i < nk; i++) if (std::strcmp(TabStr(src, kt, i), "Actors") == 0) { actIdx = static_cast<int>(i); break; }
    if (actIdx < 0) { *err = 3; return nullptr; }
    /* 歩いて、広がりと全ハッシュを調べる */
    Scan sc; sc.bm = static_cast<u8*>(alloc(kMaxOff / 32 + 16)); sc.hashes = static_cast<u32*>(alloc(sizeof(u32) * kMaxHash)); sc.arrs = static_cast<u32*>(alloc(sizeof(u32) * kMaxHash)); Tabs* t = static_cast<Tabs*>(alloc(sizeof(Tabs)));
    if (!sc.bm || !sc.hashes || !sc.arrs || !t) { *err = 4; return nullptr; }
    std::memset(sc.bm, 0, kMaxOff / 32 + 16); sc.nh = 0; sc.na = 0; sc.ext = 0; sc.nstr = ns; sc.nkey = nk; sc.ok = true;
    Ext(sc, kt + r32(src + kt + 4 + 4 * nk)); Ext(sc, st + r32(src + st + 4 + 4 * ns));
    u8* result = nullptr; u32 actCount = 0;
    do {
        if (!Visit(src, 0xc1, root, sc, 0)) { *err = 5; break; }
        /* Actors のエントリ */
        const u8* rh = src + root; const u32 rn = r24(rh + 1); int ent = -1;
        for (u32 i = 0; i < rn; i++) if (r24(rh + 4 + 8 * i) == static_cast<u32>(actIdx)) ent = static_cast<int>(i);
        if (ent < 0 || rh[4 + 8 * ent + 3] != 0xc0) { *err = 3; break; }
        const u32 aoff = r32(rh + 4 + 8 * ent + 4); actCount = r24(src + aoff + 1); *actorsN = actCount;
        /* 出現点の入った BYML の表 */
        const u8* f = kOctByml; const u8* fkt = f + r32(f + 4); const u8* fst = f + r32(f + 8); const u32 froot = r32(f + 12);
        const u32 fnk = r24(fkt + 1), fns = r24(fst + 1);
        /* 新しいキー表: 古いキー + 足りないキー (並べ替えて入れる) */
        t->nkey = 0; for (u32 i = 0; i < nk; i++) t->key[t->nkey++] = TabStr(src, kt, i);
        t->nstr = 0; for (u32 i = 0; i < ns; i++) t->str[t->nstr++] = TabStr(src, st, i);
        static char g_guid[16][40];
        for (u32 i = 0; i < fnk; i++) { const char* k = reinterpret_cast<const char*>(fkt + r32(fkt + 4 + 4 * i)); bool have = false; for (u32 q = 0; q < t->nkey; q++) if (std::strcmp(t->key[q], k) == 0) { have = true; break; }
            if (!have) { if (t->nkey >= 399) { *err = 6; break; } u32 pos = t->nkey; while (pos > 0 && std::strcmp(t->key[pos - 1], k) > 0) { t->key[pos] = t->key[pos - 1]; pos--; } t->key[pos] = k; t->nkey++; } }
        if (*err) break;
        for (u32 i = 0; i < fns; i++) { const char* s = reinterpret_cast<const char*>(fst + r32(fst + 4 + 4 * i)); if (FindStr(*t, s) < 0) { if (t->nstr >= 2000) { *err = 6; break; } u32 pos = t->nstr; while (pos > 0 && std::strcmp(t->str[pos - 1], s) > 0) { t->str[pos] = t->str[pos - 1]; pos--; } t->str[pos] = s; t->nstr++; } }
        if (*err) break;
        for (int b = 0; b < count; b++) { char* g = g_guid[b]; std::memcpy(g, "2b5d726a-d42b-3c32-cb56-028c4d0b3800", 37); g[34] = static_cast<char>('0' + (b / 10)); g[35] = static_cast<char>('0' + (b % 10)); /* 一体ごとに違う */
            if (t->nstr >= 2040) { *err = 6; break; } u32 pos = t->nstr; while (pos > 0 && std::strcmp(t->str[pos - 1], g) > 0) { t->str[pos] = t->str[pos - 1]; pos--; } t->str[pos] = g; t->nstr++; }
        if (*err) break;
        for (u32 i = 0; i < nk; i++) { const int ni = FindKey(*t, TabStr(src, kt, i)); if (ni < 0) { *err = 6; break; } t->keyMap[i] = static_cast<u32>(ni); }
        for (u32 i = 0; i < ns; i++) { const int ni = FindStr(*t, TabStr(src, st, i)); if (ni < 0) { *err = 6; break; } t->strMap[i] = static_cast<u32>(ni); }
        if (*err) break;
        /* 出力: 元のコピー + 表 + 節 + 新しい Actors 配列 */
        Out o; o.cap = sc.ext + r32(src + kt + 4 + 4 * nk) + r32(src + st + 4 + 4 * ns) + 24576 + 5 * (actCount + 16) + 4096 * static_cast<u32>(count) + 4 * (nk + ns); o.len = 0; o.ok = true;
        o.d = static_cast<u8*>(alloc(o.cap)); if (!o.d) { *err = 4; break; }
        std::memcpy(o.d, src, sc.ext); o.len = (sc.ext + 3) & ~3u; for (u32 i = sc.ext; i < o.len; i++) o.d[i] = 0;
        for (u32 i = 0; i < sc.nh; i++) { u8* h = o.d + sc.hashes[i]; const u32 n = r24(h + 1); for (u32 q = 0; q < n; q++) { u8* e = h + 4 + 8 * q; w24(e, t->keyMap[r24(e)]); if (e[3] == 0xa0) w32(e + 4, t->strMap[r32(e + 4)]); } }
        for (u32 i = 0; i < sc.na; i++) { u8* a = o.d + sc.arrs[i]; const u32 n = r24(a + 1); const u32 po = 4 + ((n + 3) & ~3u); for (u32 q = 0; q < n; q++) if (a[4 + q] == 0xa0) w32(a + po + 4 * q, t->strMap[r32(a + po + 4 * q)]); }
        const u32 nkt = PutTable(o, t->key, t->nkey); const u32 nst = PutTable(o, t->str, t->nstr);
        if (!o.ok) { *err = 7; break; }
        w32(o.d + 4, nkt); w32(o.d + 8, nst);
        u32 nodeOff[16];
        for (int b = 0; b < count; b++) {
            u8 ty; u32 v; if (!Clone(f, fkt, fst, 0xc1, froot, *t, o, &ty, &v) || ty != 0xc1) { *err = 8; break; }
            /* 一体ごとに違う: InstanceID / SRTHash / Translate */
            u8* h = o.d + v; const u32 n = r24(h + 1);
            const int kI = FindKey(*t, "InstanceID"), kS = FindKey(*t, "SRTHash"), kT = FindKey(*t, "Translate");
            for (u32 q = 0; q < n; q++) { u8* e = h + 4 + 8 * q; const u32 k = r24(e);
                if (static_cast<int>(k) == kI && e[3] == 0xa0) { const int gi = FindStr(*t, g_guid[b]); if (gi < 0) { *err = 8; break; } w32(e + 4, static_cast<u32>(gi)); }
                else if (static_cast<int>(k) == kS && (e[3] == 0xd1 || e[3] == 0xd3)) w32(e + 4, r32(e + 4) + static_cast<u32>(b) * 7919u);
                else if (static_cast<int>(k) == kT && e[3] == 0xc0) { u8* a = o.d + r32(e + 4); const u32 an = r24(a + 1); const u32 po = 4 + ((an + 3) & ~3u); if (an == 3 && a[4] == 0xd2) { float x; std::memcpy(&x, a + po, 4); x += (static_cast<float>(b) - 0.5f * static_cast<float>(count - 1)) * 4.0f; std::memcpy(a + po, &x, 4); } } }
            if (*err) break; nodeOff[b] = v;
        }
        if (*err) break;
        /* 新しい Actors 配列 */
        const u32 n0 = actCount; const u32 n1 = n0 + static_cast<u32>(count); Al(o); const u32 an = o.len; const u32 pad = (n1 + 3) & ~3u;
        u8* a = Take(o, 4 + pad + 4 * n1); if (!a) { *err = 7; break; }
        a[0] = 0xc0; w24(a + 1, n1); std::memset(a + 4, 0, pad);
        const u32 opo = aoff + 4 + ((n0 + 3) & ~3u);
        for (u32 i = 0; i < n0; i++) { a[4 + i] = src[aoff + 4 + i]; w32(a + 4 + pad + 4 * i, r32(src + opo + 4 * i)); }
        for (int b = 0; b < count; b++) { a[4 + n0 + b] = 0xc1; w32(a + 4 + pad + 4 * (n0 + b), nodeOff[b]); }
        { u8* rh2 = o.d + root; const u32 rn2 = r24(rh2 + 1); bool done = false; for (u32 i = 0; i < rn2; i++) { u8* e = rh2 + 4 + 8 * i; if (r24(e) == t->keyMap[actIdx] && e[3] == 0xc0) { w32(e + 4, an); done = true; } } if (!done) { *err = 9; break; } }
        *outLen = o.len; result = o.d;
    } while (false);
    freeF(sc.bm); freeF(sc.hashes); freeF(sc.arrs); freeF(t);
    return result;
}
}
