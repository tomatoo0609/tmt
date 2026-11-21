#pragma once
#include <cstdint>
/* 胡桃: 色つきの軽いメッシュを、毎フレームその場で描く (向きは自由。上下も回せる) */
namespace hutao {
#include "hutao_mesh_data.h"
#include "hutao_tex_data.h"
constexpr float kCy = 9.75f; /* モデルの高さの半分 (単位) */
/* 版 194: SD の tmt/hutao/tex0..2.bin を読めたら、元の細かさ (1024/512/1024) の模様に差し替える (起動時のメモリを増やさないため、起動後に確保する) */
inline const uint8_t* volatile g_Hi[3] = { nullptr, nullptr, nullptr }; inline volatile int g_HiLg[3] = { 10, 9, 10 }, g_HiN[3] = { 4, 3, 4 }; /* 段の数 (ミップ) */
constexpr int kBudget = 300000; /* 描く絵の画素数の上限 */
/* 版 195: 深さの入れ物 (2 バイト x kBudget)・動かした頂点 (float 3 つ)・その法線は、起動時のメモリ (BSS) を増やさないよう、最初に描くときに確保する */
inline uint16_t* g_Zp = nullptr; inline float* g_AV = nullptr; inline int8_t* g_AN = nullptr; inline volatile bool g_Anim = false;
static int16_t g_sxy[kNV * 2]; /* 中心からの位置 (1/4 画素) */
static uint16_t g_sd[kNV];
static uint8_t g_vs[kNV]; /* 頂点ごとの明るさ (128 = 1.0) */
/* 版 195: 歩き・走りの動き。足 (腿・ひざ) は前後 (X 軸まわり) に振る。腕は、元の姿 (両腕を横に伸ばした形) から体の横に下ろしてから (Z 軸まわり)、肩・ひじを前後に振る。
   体全体は前に傾け、上下にゆらす。ph = 歩きの位相、amp = 腿の振り (ラジアン)、knee = ひざの曲がり、arm = 腕の振り、elb = ひじの曲がり、lean = 前傾、bob = 上下 (単位) */
struct M34 { float m[12]; };
inline M34 Id() { return { { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0 } }; }
inline M34 Mul(const M34& B, const M34& A) { /* A を先に、B を後に */
    M34 o; for (int r = 0; r < 3; r++) { for (int c = 0; c < 3; c++) o.m[r * 4 + c] = B.m[r * 4] * A.m[c] + B.m[r * 4 + 1] * A.m[4 + c] + B.m[r * 4 + 2] * A.m[8 + c]; o.m[r * 4 + 3] = B.m[r * 4] * A.m[3] + B.m[r * 4 + 1] * A.m[7] + B.m[r * 4 + 2] * A.m[11] + B.m[r * 4 + 3]; } return o; }
inline M34 RotX(float py, float pz, float th) { const float c = __builtin_cosf(th), s = __builtin_sinf(th); return { { 1, 0, 0, 0, 0, c, -s, py - c * py + s * pz, 0, s, c, pz - s * py - c * pz } }; }
inline M34 RotZ(float px, float py, float ph) { const float c = __builtin_cosf(ph), s = __builtin_sinf(ph); return { { c, -s, 0, px - c * px + s * py, s, c, 0, py - s * px - c * py, 0, 0, 1, 0 } }; }
inline void Pose(float ph, float amp, float knee, float arm, float elb, float lean, float bob, float low, float armBase, float hipBase) {
    if (!g_AV || !g_AN) { g_Anim = false; return; }
    { static float lp[10] = { 1e9f }; const float np[10] = { ph, amp, knee, arm, elb, lean, bob, low, armBase, hipBase }; bool same = g_Anim; for (int i = 0; i < 10; i++) if (np[i] != lp[i]) same = false; if (same) return; for (int i = 0; i < 10; i++) lp[i] = np[i]; } /* 同じ姿ならやり直さない */
    const float s = __builtin_sinf(ph), c = __builtin_cosf(ph);
    float px[9], py[9], pz[9]; for (int j = 1; j < 9; j++) { px[j] = kPiv[j * 3] * 0.001f; py[j] = kPiv[j * 3 + 1] * 0.001f - kCy; pz[j] = -kPiv[j * 3 + 2] * 0.001f; }
    M34 root = RotX(-kCy, 0.0f, lean); root.m[7] += bob;
    M34 M[9]; M[0] = root;
    const float hipL = -(hipBase + amp * s), hipR = -(hipBase - amp * s); /* 前に出す = 負の回転。hipBase = 走るときは腿を前に上げた形を中心にする */
    const float kL = knee * (c > 0.0f ? c : 0.0f), kR = knee * (c < 0.0f ? -c : 0.0f); /* ひざは足を前に出すときに曲がる */
    const float armL = arm * s - armBase, armR = -arm * s - armBase; /* 腕は反対の足と同じ向きに振る。armBase = 走るときは肩から前に上げた形を中心にする */
    const float el = -elb, lowA = low;                          /* ひじは前に曲がる。lowA = 腕を横から下ろす角度 (小さいほど体から離れる) */
    const M34 hL = Mul(root, RotX(py[1], pz[1], hipL)), hR = Mul(root, RotX(py[3], pz[3], hipR));
    M[1] = hL; M[2] = Mul(hL, RotX(py[2], pz[2], kL)); M[3] = hR; M[4] = Mul(hR, RotX(py[4], pz[4], kR));
    /* 腕: 下ろす (左腕は x が正の側 = Z 軸まわりに負、右腕は正) → 肩で前後に振る。ひじは、下ろしたあとの位置で曲げる */
    const M34 lowL = RotZ(px[5], py[5], -lowA), lowR = RotZ(px[7], py[7], lowA);
    const M34 sL = Mul(root, Mul(RotX(py[5], pz[5], armL), lowL)), sR = Mul(root, Mul(RotX(py[7], pz[7], armR), lowR));
    auto at = [&](const M34& m, int j, float* o) { o[0] = m.m[0] * px[j] + m.m[1] * py[j] + m.m[2] * pz[j] + m.m[3]; o[1] = m.m[4] * px[j] + m.m[5] * py[j] + m.m[6] * pz[j] + m.m[7]; o[2] = m.m[8] * px[j] + m.m[9] * py[j] + m.m[10] * pz[j] + m.m[11]; };
    float eL[3], eR[3]; at(lowL, 6, eL); at(lowR, 8, eR);
    M[5] = sL; M[7] = sR;
    M[6] = Mul(root, Mul(RotX(py[5], pz[5], armL), Mul(RotX(eL[1], eL[2], el), lowL)));
    M[8] = Mul(root, Mul(RotX(py[7], pz[7], armR), Mul(RotX(eR[1], eR[2], el), lowR)));
    for (int i = 0; i < kNV; i++) {
        const float x = kV[i * 3] * 0.001f, y = kV[i * 3 + 1] * 0.001f - kCy, z = -kV[i * 3 + 2] * 0.001f;
        const float nxv = kNv[i * 3] * (1.0f / 127.0f), nyv = kNv[i * 3 + 1] * (1.0f / 127.0f), nzv = kNv[i * 3 + 2] * (1.0f / 127.0f);
        const float* A = M[kJ[i * 2]].m; const float* B = M[kJ[i * 2 + 1]].m; const float w = kW[i] * (1.0f / 255.0f), v = 1.0f - w;
        g_AV[i * 3] = w * (A[0] * x + A[1] * y + A[2] * z + A[3]) + v * (B[0] * x + B[1] * y + B[2] * z + B[3]);
        g_AV[i * 3 + 1] = w * (A[4] * x + A[5] * y + A[6] * z + A[7]) + v * (B[4] * x + B[5] * y + B[6] * z + B[7]);
        g_AV[i * 3 + 2] = w * (A[8] * x + A[9] * y + A[10] * z + A[11]) + v * (B[8] * x + B[9] * y + B[10] * z + B[11]);
        const float n0 = w * (A[0] * nxv + A[1] * nyv + A[2] * nzv) + v * (B[0] * nxv + B[1] * nyv + B[2] * nzv);
        const float n1 = w * (A[4] * nxv + A[5] * nyv + A[6] * nzv) + v * (B[4] * nxv + B[5] * nyv + B[6] * nzv);
        const float n2 = w * (A[8] * nxv + A[9] * nyv + A[10] * nzv) + v * (B[8] * nxv + B[9] * nyv + B[10] * nzv);
        int q0 = static_cast<int>(n0 * 127.0f), q1 = static_cast<int>(n1 * 127.0f), q2 = static_cast<int>(n2 * 127.0f);
        if (q0 > 127) q0 = 127; if (q0 < -127) q0 = -127; if (q1 > 127) q1 = 127; if (q1 < -127) q1 = -127; if (q2 > 127) q2 = 127; if (q2 < -127) q2 = -127;
        g_AN[i * 3] = static_cast<int8_t>(q0); g_AN[i * 3 + 1] = static_cast<int8_t>(q1); g_AN[i * 3 + 2] = static_cast<int8_t>(q2);
    }
    g_Anim = true;
}
/* 1) 画面でどれだけの四角になるかを測る。upx: x 方向の単位あたりの画素数、upy: y 方向 (縦にあらくしたいときは upx より小さくする)。r/u/f: 画面の右・上・前の向き (世界)。
      モデルは z を反転して (右手系) 世界の軸にそのまま置く。minx/miny: 中心から四角の左上までの画素数 */
inline void Project(float upx, float upy, const float* r, const float* u, const float* f, int& minx, int& miny, int& w, int& h) {
    float x0 = 1e9f, x1 = -1e9f, y0 = 1e9f, y1 = -1e9f;
    for (int i = 0; i < kNV; i++) {
        float x, y, z; if (g_Anim) { x = g_AV[i * 3]; y = g_AV[i * 3 + 1]; z = g_AV[i * 3 + 2]; } else { x = kV[i * 3] * 0.001f; y = kV[i * 3 + 1] * 0.001f - kCy; z = -kV[i * 3 + 2] * 0.001f; }
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
    const int n = w * h; uint16_t* const g_z = g_Zp; for (int i = 0; i < n; i++) { out[i] = 0; g_z[i] = 0xFFFF; }
    const float ox = static_cast<float>(minx), oy = static_cast<float>(miny);
    const float lx = -0.35f * r[0] + 0.55f * u[0] - 0.76f * f[0], ly = -0.35f * r[1] + 0.55f * u[1] - 0.76f * f[1], lz = -0.35f * r[2] + 0.55f * u[2] - 0.76f * f[2];
    for (int i = 0; i < kNV; i++) { /* 頂点の明るさ */
        const int8_t* nn = g_Anim ? g_AN + i * 3 : kNv + i * 3; float d = (nn[0] * lx + nn[1] * ly + nn[2] * lz) * (1.0f / 127.0f);
        float sh = 0.56f + 0.6f * d; if (sh < 0.45f) sh = 0.45f; if (sh > 1.12f) sh = 1.12f; g_vs[i] = static_cast<uint8_t>(sh * 128.0f);
    }
    static const uint8_t* const kTp[3] = { kTex0, kTex1, kTex2 }; static const int kLg[3] = { 9, 8, 9 };
    for (int fi = 0; fi < kNF; fi++) {
        float nx, ny, nz;
        if (g_Anim) { /* 動かした頂点から面の向きを求める (向きの印 kFs でモデルの外向きにそろえる) */
            const float* pa = g_AV + kF[fi * 3] * 3; const float* pb = g_AV + kF[fi * 3 + 1] * 3; const float* pc = g_AV + kF[fi * 3 + 2] * 3;
            const float e1x = pb[0] - pa[0], e1y = pb[1] - pa[1], e1z = pb[2] - pa[2], e2x = pc[0] - pa[0], e2y = pc[1] - pa[1], e2z = pc[2] - pa[2];
            nx = e1y * e2z - e1z * e2y; ny = e1z * e2x - e1x * e2z; nz = e1x * e2y - e1y * e2x;
            const float l2 = nx * nx + ny * ny + nz * nz; if (l2 < 1e-12f) continue; const float il = (kFs[fi] ? 1.0f : -1.0f) / __builtin_sqrtf(l2); nx *= il; ny *= il; nz *= il;
        } else { nx = kN[fi * 3] * (1.0f / 127.0f); ny = kN[fi * 3 + 1] * (1.0f / 127.0f); nz = kN[fi * 3 + 2] * (1.0f / 127.0f); }
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
        const float inv = 1.0f / area; const uint8_t* hi = tid == 255 ? nullptr : g_Hi[tid]; const uint8_t* tp = nullptr; int lg = 10;
        if (tid != 255) {
            if (hi) { /* 細かい模様 (SD): 画面での大きさに合う段 (ミップ) を選ぶ = ちらつきを減らし、画質を上げる */
                const int l0 = g_HiLg[tid]; const float t0 = static_cast<float>(1 << l0); float ua = ((u1 - u0) * (v2 - v0) - (u2 - u0) * (v1 - v0)) * t0 * t0; if (ua < 0.0f) ua = -ua;
                const float ratio = ua * inv; int lv = 0; if (ratio > 1.0f) lv = static_cast<int>(0.5f * __builtin_log2f(ratio) + 0.35f);
                if (lv > g_HiN[tid] - 1) lv = g_HiN[tid] - 1; if (lv < 0) lv = 0;
                size_t off = 0; for (int k = 0; k < lv; k++) off += (static_cast<size_t>(1) << (2 * (l0 - k))) / 2u;
                tp = hi + off; lg = l0 - lv;
            } else { tp = kTp[tid]; lg = kLg[tid]; }
        }
        const float tsz = static_cast<float>(1 << lg);
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
