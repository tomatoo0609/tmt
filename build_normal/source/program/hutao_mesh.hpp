#pragma once
#include <cstdint>
/* 胡桃: 色つきの軽いメッシュを、毎フレームその場で描く (向きは自由。上下も回せる) */
namespace hutao {
#include "hutao_mesh_data.h"
#include "hutao_tex_data.h"
constexpr float kCy = 9.75f; /* モデルの高さの半分 (単位) */
/* 版 194: SD の tmt/hutao/tex0..2.bin を読めたら、元の細かさ (1024/512/1024) の模様に差し替える (起動時のメモリを増やさないため、起動後に確保する) */
inline const uint8_t* volatile g_Hi[3] = { nullptr, nullptr, nullptr }; inline volatile int g_HiLg[3] = { 10, 9, 10 };
constexpr int kBudget = 120000; /* 描く絵の画素数の上限 */
static uint16_t g_z[kBudget];
static int16_t g_sxy[kNV * 2]; /* 中心からの位置 (1/4 画素) */
static uint16_t g_sd[kNV];
static uint8_t g_vs[kNV]; /* 頂点ごとの明るさ (128 = 1.0) */
/* 1) 画面でどれだけの四角になるかを測る。upx: x 方向の単位あたりの画素数、upy: y 方向 (縦にあらくしたいときは upx より小さくする)。r/u/f: 画面の右・上・前の向き (世界)。
      モデルは z を反転して (右手系) 世界の軸にそのまま置く。minx/miny: 中心から四角の左上までの画素数 */
inline void Project(float upx, float upy, const float* r, const float* u, const float* f, int& minx, int& miny, int& w, int& h) {
    float x0 = 1e9f, x1 = -1e9f, y0 = 1e9f, y1 = -1e9f;
    for (int i = 0; i < kNV; i++) {
        const float x = kV[i * 3] * 0.001f, y = kV[i * 3 + 1] * 0.001f - kCy, z = -kV[i * 3 + 2] * 0.001f;
        const float X = (x * r[0] + y * r[1] + z * r[2]) * upx, Y = -(x * u[0] + y * u[1] + z * u[2]) * upy, Z = x * f[0] + y * f[1] + z * f[2];
        g_sxy[i * 2] = static_cast<int16_t>(X * 4.0f); g_sxy[i * 2 + 1] = static_cast<int16_t>(Y * 4.0f);
        float zz = (Z + 16.0f) * 2048.0f; if (zz < 0.0f) zz = 0.0f; if (zz > 65534.0f) zz = 65534.0f; g_sd[i] = static_cast<uint16_t>(zz);
        if (X < x0) x0 = X; if (X > x1) x1 = X; if (Y < y0) y0 = Y; if (Y > y1) y1 = Y;
    }
    minx = static_cast<int>(x0) - 2; miny = static_cast<int>(y0) - 2; w = static_cast<int>(x1) - minx + 3; h = static_cast<int>(y1) - miny + 3;
}
/* BC1 (4 色固定) の 1 画素。lg = 一辺の 2 の何乗か */
inline uint32_t TexAt(const uint8_t* t, int lg, int x, int y) {
    const int m = (1 << lg) - 1; x &= m; y &= m;
    const uint8_t* b = t + (static_cast<size_t>(y >> 2) * (static_cast<size_t>(1) << (lg - 2)) + static_cast<size_t>(x >> 2)) * 8;
    const int c0 = b[0] | (b[1] << 8), c1 = b[2] | (b[3] << 8), k = (b[4 + (y & 3)] >> (2 * (x & 3))) & 3;
    int r0 = (c0 >> 11) & 31, g0 = (c0 >> 5) & 63, b0 = c0 & 31, r1 = (c1 >> 11) & 31, g1 = (c1 >> 5) & 63, b1 = c1 & 31;
    r0 = r0 << 3 | r0 >> 2; g0 = g0 << 2 | g0 >> 4; b0 = b0 << 3 | b0 >> 2; r1 = r1 << 3 | r1 >> 2; g1 = g1 << 2 | g1 >> 4; b1 = b1 << 3 | b1 >> 2;
    int r, g, bl;
    if (k == 0) { r = r0; g = g0; bl = b0; } else if (k == 1) { r = r1; g = g1; bl = b1; } else if (k == 2) { r = (2 * r0 + r1) / 3; g = (2 * g0 + g1) / 3; bl = (2 * b0 + b1) / 3; } else { r = (r0 + 2 * r1) / 3; g = (g0 + 2 * g1) / 3; bl = (b0 + 2 * b1) / 3; }
    return static_cast<uint32_t>(r) | (static_cast<uint32_t>(g) << 8) | (static_cast<uint32_t>(bl) << 16);
}
/* 2) 測った四角 (w x h <= kBudget) に描く。絵のない所は alpha 0。模様は 1 画素ずつ引き、明るさは頂点から混ぜる (面のつぎ目が見えない) */
inline void Draw(uint32_t* out, int w, int h, int minx, int miny, const float* r, const float* u, const float* f) {
    const int n = w * h; for (int i = 0; i < n; i++) { out[i] = 0; g_z[i] = 0xFFFF; }
    const float ox = static_cast<float>(minx), oy = static_cast<float>(miny);
    const float lx = -0.35f * r[0] + 0.55f * u[0] - 0.76f * f[0], ly = -0.35f * r[1] + 0.55f * u[1] - 0.76f * f[1], lz = -0.35f * r[2] + 0.55f * u[2] - 0.76f * f[2];
    for (int i = 0; i < kNV; i++) { /* 頂点の明るさ */
        float d = (kNv[i * 3] * lx + kNv[i * 3 + 1] * ly + kNv[i * 3 + 2] * lz) * (1.0f / 127.0f);
        float sh = 0.56f + 0.6f * d; if (sh < 0.45f) sh = 0.45f; if (sh > 1.12f) sh = 1.12f; g_vs[i] = static_cast<uint8_t>(sh * 128.0f);
    }
    static const uint8_t* const kTp[3] = { kTex0, kTex1, kTex2 }; static const int kLg[3] = { 9, 8, 9 };
    for (int fi = 0; fi < kNF; fi++) {
        const float nx = kN[fi * 3] * (1.0f / 127.0f), ny = kN[fi * 3 + 1] * (1.0f / 127.0f), nz = kN[fi * 3 + 2] * (1.0f / 127.0f);
        const float nf = nx * f[0] + ny * f[1] + nz * f[2]; if (nf >= 0.0f) continue; /* 向こうを向いた面 */
        const int tid = kT[fi];
        uint32_t flat = 0;
        if (tid == 255) {
            const float nr = nx * r[0] + ny * r[1] + nz * r[2], nu = nx * u[0] + ny * u[1] + nz * u[2];
            float sh = 0.5f + 0.62f * (nr * -0.35f + nu * 0.55f + nf * -0.76f); if (sh < 0.42f) sh = 0.42f; if (sh > 1.12f) sh = 1.12f;
            int cr = static_cast<int>(kC[fi * 3] * sh), cg = static_cast<int>(kC[fi * 3 + 1] * sh), cb = static_cast<int>(kC[fi * 3 + 2] * sh);
            if (cr > 255) cr = 255; if (cg > 255) cg = 255; if (cb > 255) cb = 255;
            flat = 0xFF000000u | (static_cast<uint32_t>(cb) << 16) | (static_cast<uint32_t>(cg) << 8) | static_cast<uint32_t>(cr);
        }
        int a = kF[fi * 3], b = kF[fi * 3 + 1], c = kF[fi * 3 + 2];
        float x0 = g_sxy[a * 2] * 0.25f - ox, y0 = g_sxy[a * 2 + 1] * 0.25f - oy, x1 = g_sxy[b * 2] * 0.25f - ox, y1 = g_sxy[b * 2 + 1] * 0.25f - oy, x2 = g_sxy[c * 2] * 0.25f - ox, y2 = g_sxy[c * 2 + 1] * 0.25f - oy;
        float area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0); if (area == 0.0f) continue;
        if (area < 0.0f) { const float tx = x1, ty = y1; x1 = x2; y1 = y2; x2 = tx; y2 = ty; const int tb = b; b = c; c = tb; area = -area; }
        const float d0 = g_sd[a], d1 = g_sd[b], d2 = g_sd[c];
        const float u0 = kU[a * 2] * (1.0f / 4096.0f), v0 = kU[a * 2 + 1] * (1.0f / 4096.0f), u1 = kU[b * 2] * (1.0f / 4096.0f), v1 = kU[b * 2 + 1] * (1.0f / 4096.0f), u2 = kU[c * 2] * (1.0f / 4096.0f), v2 = kU[c * 2 + 1] * (1.0f / 4096.0f);
        const float s0 = g_vs[a], s1 = g_vs[b], s2 = g_vs[c];
        int bx0 = static_cast<int>(x0 < x1 ? (x0 < x2 ? x0 : x2) : (x1 < x2 ? x1 : x2)), bx1 = static_cast<int>((x0 > x1 ? (x0 > x2 ? x0 : x2) : (x1 > x2 ? x1 : x2)) + 1.0f);
        int by0 = static_cast<int>(y0 < y1 ? (y0 < y2 ? y0 : y2) : (y1 < y2 ? y1 : y2)), by1 = static_cast<int>((y0 > y1 ? (y0 > y2 ? y0 : y2) : (y1 > y2 ? y1 : y2)) + 1.0f);
        if (bx0 < 0) bx0 = 0; if (by0 < 0) by0 = 0; if (bx1 > w - 1) bx1 = w - 1; if (by1 > h - 1) by1 = h - 1; if (bx0 > bx1 || by0 > by1) continue;
        const float inv = 1.0f / area; const uint8_t* hi = tid == 255 ? nullptr : g_Hi[tid]; const uint8_t* tp = tid == 255 ? nullptr : (hi ? hi : kTp[tid]); const int lg = tid == 255 ? 10 : (hi ? g_HiLg[tid] : kLg[tid]); const float tsz = static_cast<float>(1 << lg);
        for (int y = by0; y <= by1; y++) {
            const float py = y + 0.5f;
            for (int x = bx0; x <= bx1; x++) {
                const float px = x + 0.5f;
                const float e2 = (x1 - x0) * (py - y0) - (y1 - y0) * (px - x0);
                const float e0 = (x2 - x1) * (py - y1) - (y2 - y1) * (px - x1);
                const float e1 = (x0 - x2) * (py - y2) - (y0 - y2) * (px - x2);
                if (e0 < 0.0f || e1 < 0.0f || e2 < 0.0f) continue;
                const float w0 = e0 * inv, w1 = e1 * inv, w2 = e2 * inv;
                const uint16_t dz = static_cast<uint16_t>(w0 * d0 + w1 * d1 + w2 * d2);
                const int idx = y * w + x; if (dz > g_z[idx]) continue;
                g_z[idx] = dz;
                if (!tp) { out[idx] = flat; continue; }
                const float uu = (w0 * u0 + w1 * u1 + w2 * u2) * tsz, vv = (w0 * v0 + w1 * v1 + w2 * v2) * tsz;
                const uint32_t tc = TexAt(tp, lg, static_cast<int>(uu < 0.0f ? uu - 1.0f : uu), static_cast<int>(vv < 0.0f ? vv - 1.0f : vv));
                const int sh = static_cast<int>(w0 * s0 + w1 * s1 + w2 * s2);
                int cr = static_cast<int>(tc & 255) * sh >> 7, cg = static_cast<int>((tc >> 8) & 255) * sh >> 7, cb = static_cast<int>((tc >> 16) & 255) * sh >> 7;
                if (cr > 255) cr = 255; if (cg > 255) cg = 255; if (cb > 255) cb = 255;
                out[idx] = 0xFF000000u | (static_cast<uint32_t>(cb) << 16) | (static_cast<uint32_t>(cg) << 8) | static_cast<uint32_t>(cr);
            }
        }
    }
}
}
