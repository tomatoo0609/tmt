#pragma once
/* 武器の調整の中身 (ゲームに依存しない部分): Params.pack の中の値を、つまみの倍率に合わせて書きかえる */
#include <cstdint>
#include <cstring>
#include "wpdata.hpp"
namespace wpcore {
constexpr int kDef = 50, kMax = 100; /* つまみは 0〜100。0 が 0%、50 が 100% (そのまま)、100 が 500%。0〜50 は 2% 刻み、50〜100 は 8% 刻み */
struct State { uint8_t idx[wpdata::kWN][5]; uint32_t cur[wpdata::kEN]; };
inline float Mul(int ix) { if (ix < 0) ix = 0; if (ix > kMax) ix = kMax; return ix <= kDef ? static_cast<float>(ix) / static_cast<float>(kDef) : 1.0f + static_cast<float>(ix - kDef) * (4.0f / static_cast<float>(kMax - kDef)); }
/* つまみ k の種類: 0 射程 (小数 ×倍率)  1 連射 (回数 ÷倍率)  2 インク (小数 ×倍率)  3 チャージ (回数 ×倍率)  4 当たり判定 (小数 ×倍率) */
inline uint32_t NewVal(int k, uint32_t orig, int ix) {
    if (ix == kDef) return orig;
    float m = Mul(ix);
    if (k == 0 || k == 2 || k == 4) { float f; std::memcpy(&f, &orig, 4); f *= m; uint32_t r; std::memcpy(&r, &f, 4); return r; }
    if (k == 1 && m < 0.02f) m = 0.02f; /* 連射は 0 で割らない (ほぼ止まる) */
    const float o = static_cast<float>(static_cast<int32_t>(orig));
    int32_t nv = k == 1 ? static_cast<int32_t>(o / m + 0.5f) : static_cast<int32_t>(o * m + 0.5f);
    if (nv < 1) nv = 1;
    return static_cast<uint32_t>(nv);
}
inline bool IsPack(const uint8_t* p) {
    static const uint8_t mg[8] = { 'S', 'A', 'R', 'C', 0x14, 0x00, 0xFF, 0xFE };
    if (std::memcmp(p, mg, 8) != 0) return false;
    uint32_t a, b; std::memcpy(&a, p + 8, 4); std::memcpy(&b, p + 12, 4);
    return a == wpdata::kPackSize && b == wpdata::kPackDof;
}
/* 1 つの武器を書く。戻り値: 書きかえた数。bad に、元の値でも前に書いた値でもなかった (= 別の物になっている) 数を足す */
inline int Apply(uint8_t* base, State& st, int w, int* bad) {
    int wrote = 0; const wpdata::W& W = wpdata::kW[w];
    for (int k = 0; k < 5; k++) for (int j = 0; j < W.cnt[k]; j++) {
        const int e = W.first[k] + j; const wpdata::E& E = wpdata::kE[e];
        const uint32_t nv = NewVal(k, E.orig, st.idx[w][k]);
        volatile uint32_t* p = reinterpret_cast<volatile uint32_t*>(base + E.off); const uint32_t c = *p;
        if (c == nv) { st.cur[e] = nv; continue; }
        if (c == E.orig || c == st.cur[e]) { *p = nv; st.cur[e] = nv; wrote++; } else if (bad) (*bad)++;
    }
    return wrote;
}
}
