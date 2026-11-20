#pragma once
#include <cstdint>
/* 胡桃: 色つきの軽いメッシュを、毎フレームその場で描く (向きは自由。上下も回せる) */
namespace hutao {
#include "hutao_mesh_data.h"
constexpr float kCy = 9.75f, kRadius = 12.4f; /* モデルの高さの半分 / 中心から一番遠い所まで (単位) */
constexpr int kZMax = 400;
static uint16_t g_z[kZMax * kZMax];
static int16_t g_sxy[kNV * 2];
static uint16_t g_sd[kNV];
/* out: S x S の RGBA (絵のない所は alpha 0)。upx: 単位あたりの画素数。r/u/f: 画面の右・上・前の向き (世界)。モデルは z を反転して (右手系) 世界の軸にそのまま置く */
inline void Render(uint32_t* out, int S, float upx, const float* r, const float* u, const float* f) {
    const int n = S * S; for (int i = 0; i < n; i++) { out[i] = 0; g_z[i] = 0xFFFF; }
    const float h = S * 0.5f;
    for (int i = 0; i < kNV; i++) {
        const float x = kV[i * 3] * 0.001f, y = kV[i * 3 + 1] * 0.001f - kCy, z = -kV[i * 3 + 2] * 0.001f;
        const float X = x * r[0] + y * r[1] + z * r[2], Y = x * u[0] + y * u[1] + z * u[2], Z = x * f[0] + y * f[1] + z * f[2];
        g_sxy[i * 2] = static_cast<int16_t>((h + X * upx) * 4.0f); g_sxy[i * 2 + 1] = static_cast<int16_t>((h - Y * upx) * 4.0f);
        float zz = (Z + 16.0f) * 2048.0f; if (zz < 0.0f) zz = 0.0f; if (zz > 65534.0f) zz = 65534.0f; g_sd[i] = static_cast<uint16_t>(zz);
    }
    for (int fi = 0; fi < kNF; fi++) {
        const float nx = kN[fi * 3] * (1.0f / 127.0f), ny = kN[fi * 3 + 1] * (1.0f / 127.0f), nz = kN[fi * 3 + 2] * (1.0f / 127.0f);
        const float nf = nx * f[0] + ny * f[1] + nz * f[2]; if (nf >= 0.0f) continue; /* 向こうを向いた面 */
        const float nr = nx * r[0] + ny * r[1] + nz * r[2], nu = nx * u[0] + ny * u[1] + nz * u[2];
        float sh = 0.5f + 0.62f * (nr * -0.35f + nu * 0.55f + nf * -0.76f); if (sh < 0.42f) sh = 0.42f; if (sh > 1.12f) sh = 1.12f;
        int cr = static_cast<int>(kC[fi * 3] * sh), cg = static_cast<int>(kC[fi * 3 + 1] * sh), cb = static_cast<int>(kC[fi * 3 + 2] * sh);
        if (cr > 255) cr = 255; if (cg > 255) cg = 255; if (cb > 255) cb = 255;
        const uint32_t col = 0xFF000000u | (static_cast<uint32_t>(cb) << 16) | (static_cast<uint32_t>(cg) << 8) | static_cast<uint32_t>(cr);
        int a = kF[fi * 3], b = kF[fi * 3 + 1], c = kF[fi * 3 + 2];
        float x0 = g_sxy[a * 2] * 0.25f, y0 = g_sxy[a * 2 + 1] * 0.25f, x1 = g_sxy[b * 2] * 0.25f, y1 = g_sxy[b * 2 + 1] * 0.25f, x2 = g_sxy[c * 2] * 0.25f, y2 = g_sxy[c * 2 + 1] * 0.25f;
        float area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0); if (area == 0.0f) continue;
        if (area < 0.0f) { const float tx = x1, ty = y1; x1 = x2; y1 = y2; x2 = tx; y2 = ty; const int tb = b; b = c; c = tb; area = -area; }
        const float d0 = g_sd[a], d1 = g_sd[b], d2 = g_sd[c];
        int bx0 = static_cast<int>(x0 < x1 ? (x0 < x2 ? x0 : x2) : (x1 < x2 ? x1 : x2)), bx1 = static_cast<int>((x0 > x1 ? (x0 > x2 ? x0 : x2) : (x1 > x2 ? x1 : x2)) + 1.0f);
        int by0 = static_cast<int>(y0 < y1 ? (y0 < y2 ? y0 : y2) : (y1 < y2 ? y1 : y2)), by1 = static_cast<int>((y0 > y1 ? (y0 > y2 ? y0 : y2) : (y1 > y2 ? y1 : y2)) + 1.0f);
        if (bx0 < 0) bx0 = 0; if (by0 < 0) by0 = 0; if (bx1 > S - 1) bx1 = S - 1; if (by1 > S - 1) by1 = S - 1; if (bx0 > bx1 || by0 > by1) continue;
        const float inv = 1.0f / area;
        for (int y = by0; y <= by1; y++) {
            const float py = y + 0.5f;
            for (int x = bx0; x <= bx1; x++) {
                const float px = x + 0.5f;
                const float e2 = (x1 - x0) * (py - y0) - (y1 - y0) * (px - x0); /* 辺 01 */
                const float e0 = (x2 - x1) * (py - y1) - (y2 - y1) * (px - x1); /* 辺 12 */
                const float e1 = (x0 - x2) * (py - y2) - (y0 - y2) * (px - x2); /* 辺 20 */
                if (e0 < 0.0f || e1 < 0.0f || e2 < 0.0f) continue;
                const uint16_t dz = static_cast<uint16_t>((e0 * d0 + e1 * d1 + e2 * d2) * inv);
                const int idx = y * S + x; if (dz <= g_z[idx]) { g_z[idx] = dz; out[idx] = col; }
            }
        }
    }
}
}
