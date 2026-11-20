
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>
typedef uint32_t u32; typedef uint64_t u64; typedef uint8_t u8; typedef uintptr_t uintptr_t_;
#define ALWAYS_INLINE inline
constexpr int kOvW = 1440, kOvH = 960; constexpr size_t kPixBytes = (size_t)kOvW*kOvH*4; constexpr size_t kPixSlot = (kPixBytes+0xFFF)&~(size_t)0xFFF;
bool g_PixCached = true; size_t g_PixHi = 0; int g_SwCur = 0; uintptr_t g_CamPos = 1, g_FootPos = 1;
volatile bool g_PetOn=true, g_PetFlip=false; volatile int g_PetKind=7, g_PetCol=7, g_PetNum=4, g_PetSpd=5, g_FovLv=0;
u8* g_PixStorage; u64 g_BufAddr[2];
float g_cam[6]; float g_foot[3];
bool CamValid(){return true;} bool FootValid(){return true;} bool Readable(uintptr_t,size_t){return true;}
u64 NowTick(){ static u64 t=0; t+=320000; return t; }
void DcClean(const void*,size_t){} void DcInval(const void*,size_t){}
struct SwApiT { void (*rowStrideF)(void*,ptrdiff_t); } ;
static ptrdiff_t g_stride; static std::vector<u32> g_frame; static int FW, FH;
struct Sw { void rowStride(void*, ptrdiff_t s){ g_stride=s; } void copyB2T(void*, u64 src, void*, const void*, const void* reg, int){ const int* r=(const int*)reg; int x=r[0],y=r[1],w=r[3],h=r[4]; const u32* sp=(const u32*)src; for(int j=0;j<h;j++) for(int i=0;i<w;i++) g_frame[(y+j)*FW+x+i]=sp[j*w+i]; } } g_Sw;
using CbCopyT2B = void (*)(void*, const void*, const void*, const void*, u64, int);
void T2B(void*, const void*, const void*, const void* reg, u64 dst, int){ const int* r=(const int*)reg; int x=r[0],y=r[1],w=r[3]; memcpy((void*)dst,&g_frame[y*FW+x],w*4); }
CbCopyT2B g_T2B = T2B;
    ALWAYS_INLINE u32 LerpPx(u32 a, u32 b, u32 t) {
        const u32 M = 0x00FF00FFu;
        const u32 rb = (((a & M) * (256 - t) + (b & M) * t) >> 8) & M;
        const u32 ag = (((a >> 8) & M) * (256 - t) + ((b >> 8) & M) * t) & 0xFF00FF00u;
        return rb | ag;
    }
namespace {
    struct PetP { float p[3]; float ph, sz, fall, r, ang, h, w, rise, len; int kind, ci; };
    constexpr size_t kPetSpBytes = 1310720, kPetRbMax = 1114112, kPetPal = 16 * 64 * 4; /* 粒の絵 / 背景の読み出し / 円の色の帯 (いちばん後ろから順に) */
    constexpr size_t kPetLo = kPixBytes - kPetPal - kPetSpBytes - kPetRbMax; /* ここより前 (メニューの絵・メニューの背景読み出し) は、粒が使っている間は使わない */
    struct PetRbI { bool ok; int w, rows, step; size_t off; } g_PetRb {};
    struct PetCol { int r, g, b; };
    inline int PetClamp(int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); }
    /* 炎の色: 熱さ 0〜1 → 暗い赤 → 橙 → 琥珀 → 黄 → 白っぽい */
    PetCol PetFire(float h) {
        static const float kS[5] = { 0.0f, 0.35f, 0.65f, 0.9f, 1.0f }; static const PetCol kC[5] = { {190, 40, 16}, {255, 100, 16}, {255, 170, 48}, {255, 225, 120}, {255, 248, 208} };
        if (h <= 0.0f) return kC[0]; if (h >= 1.0f) return kC[4];
        int i = 0; while (i < 3 && h > kS[i + 1]) i++; const float t = (h - kS[i]) / (kS[i + 1] - kS[i]);
        return PetCol { static_cast<int>(kC[i].r + (kC[i + 1].r - kC[i].r) * t), static_cast<int>(kC[i].g + (kC[i + 1].g - kC[i].g) * t), static_cast<int>(kC[i].b + (kC[i + 1].b - kC[i].b) * t) };
    }
    /* 粒の絵 (半透明) を作って写す。背景は前のフレームの画面 (横は全部・縦は数行おき) を読んで混ぜる。
       1 粒 = 1 回の写し (四角の絵の外側は背景と同じ色になる)。形は透明度の分布 (accA) と熱さ (accH) を描いて、最後に色にして混ぜる */
    void PetDrawSp(void* cb, void* tex, u64 base, int W, int H) {
        static PetP P[160]; static int sel = -1; static u64 lastT = 0; static u32 rs = 20251120u; static u8 accA[192 * 192], accH[192 * 192];
        auto rnd = [&]() -> float { rs = rs * 1664525u + 1013904223u; return static_cast<float>((rs >> 8) & 0xFFFFu) / 65535.0f; };
        struct Reg { int x, y, z, w, h, d; };
        if (!g_T2B || !g_PixCached || W <= 0 || H <= 0 || W > kOvW || H > kOvH) return;
        const uintptr_t cp = g_CamPos; if (!cp || !CamValid() || !Readable(cp - 0xC, 24)) return;
        const size_t used = (g_PixHi + 255u) & ~static_cast<size_t>(255u);
        /* 背景の読み出しの大きさ: 縦の間引きを、1.1MB に収まるまで大きくする */
        int step = 4; while (step < 64 && static_cast<size_t>((H + step - 1) / step) * static_cast<size_t>(W) * 4u > kPetRbMax) step++;
        const int rows = (H + step - 1) / step; const size_t rbBytes = static_cast<size_t>(rows) * W * 4u;
        const size_t spOff = kPixBytes - kPetPal - kPetSpBytes, rbOff = (spOff - rbBytes) & ~static_cast<size_t>(255u);
        if (rbOff < used) return; /* メニューの絵が大きくて場所がない */
        float f[6]; std::memcpy(f, reinterpret_cast<const void*>(cp - 0xC), 24);
        float fx = f[3] - f[0], fy = f[4] - f[1], fz = f[5] - f[2]; const float m2 = fx * fx + fy * fy + fz * fz;
        if (!(m2 == m2) || m2 < 1.0f || m2 > 1600.0f) return;
        const float im = 1.0f / __builtin_sqrtf(m2); fx *= im; fy *= im; fz *= im;
        const float ch = __builtin_sqrtf(fx * fx + fz * fz); float rx = 1.0f, rz = 0.0f; if (ch > 0.05f) { rx = -fz / ch; rz = fx / ch; }
        if (g_PetFlip) { rx = -rx; rz = -rz; }
        const float ux = -rz * fy, uy = rz * fx - rx * fz, uz = rx * fy; /* 上 = 右 x 前 */
        const int kind = g_PetKind & 7, col = g_PetCol & 7, nSel = kind * 10 + col; const bool fire = col == 7;
        const int n = 16 * g_PetNum + 8; /* 24〜136 */
        static const PetCol kTh[7][2] = { { {225, 232, 255}, {255, 255, 255} }, { {255, 140, 180}, {255, 232, 240} }, { {100, 195, 255}, {225, 248, 255} }, { {255, 185, 50}, {255, 245, 170} }, { {110, 215, 110}, {225, 255, 200} }, { {165, 115, 255}, {235, 220, 255} }, { {255, 140, 180}, {255, 255, 255} } };
        static const PetCol kRbw[6] = { {255, 120, 170}, {255, 225, 90}, {120, 230, 120}, {100, 220, 255}, {120, 150, 255}, {235, 120, 255} };
        float C[3] = { f[0] + fx * 3.5f, f[1] - 1.3f, f[2] + fz * 3.5f }; /* 渦の中心: 自分の足元 (分からないときは、カメラの前 3.5m) */
        { const uintptr_t fp = g_FootPos; if (fp && FootValid() && Readable(fp, 12)) { float q3[3]; std::memcpy(q3, reinterpret_cast<const void*>(fp), 12); if (q3[0] == q3[0] && q3[1] == q3[1] && q3[2] == q3[2]) { const float ddx = q3[0] - f[0], ddy = q3[1] - f[1], ddz = q3[2] - f[2]; if (ddx * ddx + ddy * ddy + ddz * ddz < 900.0f) { C[0] = q3[0]; C[1] = q3[1]; C[2] = q3[2]; } } } }
        const u64 now = NowTick(); float dt = lastT ? static_cast<float>(now - lastT) / 19200000.0f : 0.0f; lastT = now; if (dt > 0.1f) dt = 0.1f; if (dt < 0.0f) dt = 0.0f;
        const float hd = ch > 0.05f ? __builtin_atan2f(fz, fx) : 0.0f;
        auto sgn = [&]() -> float { return rnd() < 0.5f ? -1.0f : 1.0f; };
        auto spawn = [&](PetP& q, bool first) {
            int kd = kind;
            if (kind == 7) { const float u = rnd(); if (fire) kd = u < 0.46f ? 4 : (u < 0.53f ? 3 : (u < 0.65f ? 5 : (u < 0.82f ? 6 : (u < 0.91f ? 2 : 0)))); else kd = u < 0.4f ? 0 : (u < 0.6f ? 1 : (u < 0.8f ? 2 : (u < 0.9f ? 3 : 4))); }
            q.kind = kd; q.ph = rnd() * 6.2831853f; q.len = kd == 5 ? 0.7f + 0.6f * rnd() : 0.35f + 0.35f * rnd(); q.ci = static_cast<int>(rnd() * 5.99f); q.ang = rnd() * 6.2831853f; q.fall = 0.0f;
            if (fire) {
                switch (kd) {
                case 4: q.r = 0.6f + 4.5f * rnd(); q.h = first ? 7.0f * rnd() : -0.2f + 0.8f * rnd(); q.rise = 0.8f + 1.6f * rnd(); q.w = sgn() * (0.4f + 0.8f * rnd()); q.sz = 0.07f + 0.09f * rnd(); break;
                case 3: q.r = 1.0f + 4.5f * rnd(); q.h = 0.3f + 2.5f * rnd(); q.rise = 0.0f; q.w = sgn() * (0.15f + 0.3f * rnd()); q.sz = 0.2f + 0.14f * rnd(); break;
                case 5: q.r = 0.8f + 3.5f * rnd(); q.h = first ? 4.0f * rnd() : -0.2f + 0.5f * rnd(); q.rise = 1.0f + 1.8f * rnd(); q.w = sgn() * (1.2f + 1.6f * rnd()); q.sz = 0.32f + 0.25f * rnd(); break;
                case 6: q.r = 0.6f + 4.0f * rnd(); q.h = first ? 6.0f * rnd() : -0.2f + 0.6f * rnd(); q.rise = 0.5f + 1.0f * rnd(); q.w = sgn() * (0.5f + 1.0f * rnd()); q.sz = 0.08f + 0.08f * rnd(); break;
                case 2: q.r = 0.5f + 5.0f * rnd(); q.h = 0.2f + 4.0f * rnd(); q.rise = 0.3f; q.w = sgn() * 0.2f; q.sz = 0.12f + 0.12f * rnd(); break;
                default: q.r = 0.6f + 4.0f * rnd(); q.h = first ? 6.0f * rnd() : -0.2f + 0.6f * rnd(); q.rise = 0.5f + 1.0f * rnd(); q.w = sgn() * (0.5f + 1.0f * rnd()); q.sz = 0.12f + 0.10f * rnd(); break;
                }
            } else {
                const float ang = (rnd() < 0.75f) ? hd + (rnd() - 0.5f) * 1.7f : rnd() * 6.2831853f; const float r = 2.0f + 13.0f * rnd();
                q.p[0] = f[0] + r * __builtin_cosf(ang); q.p[2] = f[2] + r * __builtin_sinf(ang); q.p[1] = f[1] + (first ? -2.5f + 8.0f * rnd() : 1.0f + 5.0f * rnd());
                switch (kd) {
                case 0: q.sz = 0.10f + 0.10f * rnd(); q.fall = 0.5f + 0.5f * rnd(); break;
                case 1: q.sz = 0.20f + 0.14f * rnd(); q.fall = 0.3f + 0.3f * rnd(); break;
                case 2: q.sz = 0.08f + 0.08f * rnd(); q.fall = 0.02f + 0.08f * rnd(); break;
                case 3: q.sz = 0.22f + 0.14f * rnd(); q.fall = 0.05f + 0.1f * rnd(); break;
                case 4: q.sz = 0.06f + 0.06f * rnd(); q.fall = -0.15f - 0.2f * rnd(); break;
                case 5: q.sz = 0.06f + 0.05f * rnd(); q.fall = 1.5f + 1.0f * rnd(); break;
                default: q.sz = 0.07f + 0.06f * rnd(); q.fall = 0.6f + 0.5f * rnd(); break;
                }
            }
        };
        if (sel != nSel) { sel = nSel; for (int i = 0; i < 160; i++) spawn(P[i], true); }
        /* 前のフレームの画面 (背景) を読む。まだ無いときは、読み出しの命令だけ出して、今回は描かない */
        const u32* rb = reinterpret_cast<const u32*>(g_PixStorage + rbOff); const bool haveRb = g_PetRb.ok && g_PetRb.off == rbOff && g_PetRb.w == W && g_PetRb.rows == rows && g_PetRb.step == step;
        if (haveRb) DcInval(rb, rbBytes);
        { g_Sw.rowStride(cb, static_cast<ptrdiff_t>(W) * 4); for (int r = 0; r < rows; r++) { int yy = r * step; if (yy >= H) yy = H - 1; Reg rg { 0, yy, 0, W, 1, 1 }; g_T2B(cb, tex, nullptr, &rg, g_BufAddr[0] + rbOff + static_cast<u64>(r) * W * 4u, 0); } g_PetRb = PetRbI { true, W, rows, step, rbOff }; }
        const float sp = 0.3f + 0.2f * static_cast<float>(g_PetSpd);
        const float vh = static_cast<float>(g_FovLv > 0 ? 55 + 5 * g_FovLv : 55) * 0.5f * 0.017453293f;
        const float kpx = static_cast<float>(H) * 0.5f * __builtin_cosf(vh) / __builtin_sinf(vh);
        const float cx = static_cast<float>(W) * 0.5f, cy = static_cast<float>(H) * 0.5f;
        auto proj = [&](float X, float Y, float Z, float& px, float& py) -> float { const float dx = X - f[0], dy = Y - f[1], dz = Z - f[2]; const float z = dx * fx + dy * fy + dz * fz; if (z < 0.7f) return -1.0f; const float xr = dx * rx + dz * rz, yu = dx * ux + dy * uy + dz * uz; px = cx + xr / z * kpx; py = cy - yu / z * kpx; return z; };
        u32* spb = reinterpret_cast<u32*>(g_PixStorage + static_cast<size_t>(g_SwCur) * kPixSlot + spOff); size_t spUsed = 0; /* 粒の絵を置く場所 (今の 1 枚の後ろ) */
        struct Cp { int x, y, w, h; size_t off; }; static Cp cps[170]; int ncp = 0;
        /* 1 粒の絵: 場所 (x0,y0)・大きさ (w,h) の accA/accH から作る。mode 0 = ふつうに重ねる、1 = 光る (足し算気味)。color(h) で色 */
        auto flush = [&](int x0, int y0, int w, int h, int mode, float alpha, const PetCol& c0, const PetCol& c1, bool fireRamp) {
            if (ncp >= 168) return; const size_t need = static_cast<size_t>(w) * h * 4u; if (spUsed + need > kPetSpBytes) return;
            if (x0 < 0 || y0 < 0 || x0 + w > W || y0 + h > H) return; u32* o = spb + spUsed / 4;
            const int ia = static_cast<int>(alpha * 256.0f);
            int ov[8], nov = 0; for (int k = ncp - 1; k >= 0 && nov < 8; k--) { const Cp& e = cps[k]; if (e.x < x0 + w && e.x + e.w > x0 && e.y < y0 + h && e.y + e.h > y0) ov[nov++] = k; } /* 先に作った粒と重なるところは、その粒の絵を背景にする (四角が上書きして欠けないように) */
            for (int y = 0; y < h; y++) {
                const int yy0 = y0 + y; int r0 = yy0 / step; int r1 = r0 + 1; const u32 tw = static_cast<u32>((yy0 - r0 * step) * 256 / step); if (r0 >= rows) r0 = rows - 1; if (r1 >= rows) r1 = rows - 1; const u32* ra = rb + static_cast<size_t>(r0) * W + x0; const u32* rb1 = rb + static_cast<size_t>(r1) * W + x0;
                for (int x = 0; x < w; x++) {
                    u32 bg = tw ? LerpPx(ra[x], rb1[x], tw) : ra[x]; for (int q2 = 0; q2 < nov; q2++) { const Cp& e = cps[ov[q2]]; const int ex2 = x0 + x - e.x, ey2 = yy0 - e.y; if (ex2 >= 0 && ex2 < e.w && ey2 >= 0 && ey2 < e.h) { bg = spb[(e.off - spOff) / 4 + static_cast<size_t>(ey2) * e.w + ex2]; break; } } const int a = accA[y * 192 + x] * ia / 255; if (a <= 0) { o[y * w + x] = bg | 0xFF000000u; continue; }
                    const float hh = accH[y * 192 + x] * (1.0f / 255.0f); PetCol c; if (fireRamp) c = PetFire(hh); else { c.r = static_cast<int>(c0.r + (c1.r - c0.r) * hh); c.g = static_cast<int>(c0.g + (c1.g - c0.g) * hh); c.b = static_cast<int>(c0.b + (c1.b - c0.b) * hh); }
                    const int br = bg & 255, bgc = (bg >> 8) & 255, bb = (bg >> 16) & 255; int r, g, b;
                    if (mode == 0) { r = (br * (256 - a) + c.r * a) >> 8; g = (bgc * (256 - a) + c.g * a) >> 8; b = (bb * (256 - a) + c.b * a) >> 8; }
                    else { const int k = 256 - (a * 90 >> 8); r = PetClamp(((br * k) >> 8) + (c.r * a >> 8)); g = PetClamp(((bgc * k) >> 8) + (c.g * a >> 8)); b = PetClamp(((bb * k) >> 8) + (c.b * a >> 8)); }
                    o[y * w + x] = 0xFF000000u | (static_cast<u32>(b) << 16) | (static_cast<u32>(g) << 8) | static_cast<u32>(r);
                }
            }
            cps[ncp++] = Cp { x0, y0, w, h, spOff + spUsed }; spUsed += need;
        };
        auto clear = [&](int w, int h) { for (int y = 0; y < h; y++) { std::memset(accA + y * 192, 0, static_cast<size_t>(w)); std::memset(accH + y * 192, 0, static_cast<size_t>(w)); } };
        /* だ円 (中心 cx,cy・半径 rx,ry・角度 th): 縁はやわらかく、中心ほど熱い */
        auto ellA = [&](int w, int h, float ex, float ey, float erx, float ery, float th, float heat) {
            const float c = __builtin_cosf(th), s2 = __builtin_sinf(th); const int xa = static_cast<int>(ex - (erx + ery)) - 1, xb = static_cast<int>(ex + (erx + ery)) + 2, ya = static_cast<int>(ey - (erx + ery)) - 1, yb = static_cast<int>(ey + (erx + ery)) + 2;
            for (int y = ya < 0 ? 0 : ya; y < yb && y < h; y++) for (int x = xa < 0 ? 0 : xa; x < xb && x < w; x++) {
                const float dx = static_cast<float>(x) + 0.5f - ex, dy = static_cast<float>(y) + 0.5f - ey; const float u = (dx * c + dy * s2) / erx, v = (-dx * s2 + dy * c) / ery, d2 = u * u + v * v; if (d2 >= 1.0f) continue;
                const float d = __builtin_sqrtf(d2), e = 1.0f - d; float cov = e * 4.0f; if (cov > 1.0f) cov = 1.0f; cov = cov * cov * (3.0f - 2.0f * cov);
                const int A = static_cast<int>(cov * 255.0f), Hh = static_cast<int>((e * heat + (1.0f - heat) * 0.0f) * 255.0f); if (A > accA[y * 192 + x]) accA[y * 192 + x] = static_cast<u8>(A); if (Hh > accH[y * 192 + x]) accH[y * 192 + x] = static_cast<u8>(Hh > 255 ? 255 : Hh); }
        };
        /* 光る円 (中心ほど熱くて濃い) */
        auto discG = [&](int w, int h, float ex, float ey, float R, float heat, float aMul) {
            const int xa = static_cast<int>(ex - R) - 1, xb = static_cast<int>(ex + R) + 2, ya = static_cast<int>(ey - R) - 1, yb = static_cast<int>(ey + R) + 2;
            for (int y = ya < 0 ? 0 : ya; y < yb && y < h; y++) for (int x = xa < 0 ? 0 : xa; x < xb && x < w; x++) {
                const float dx = static_cast<float>(x) + 0.5f - ex, dy = static_cast<float>(y) + 0.5f - ey; const float d = __builtin_sqrtf(dx * dx + dy * dy) / R; if (d >= 1.0f) continue;
                const float e = 1.0f - d, cov = e * e * (3.0f - 2.0f * e) * aMul; const int A = static_cast<int>(cov * 255.0f), Hh = static_cast<int>(e * e * heat * 255.0f);
                if (A > accA[y * 192 + x]) accA[y * 192 + x] = static_cast<u8>(A); if (Hh > accH[y * 192 + x]) accH[y * 192 + x] = static_cast<u8>(Hh > 255 ? 255 : Hh); }
        };
        for (int i = 0; i < n; i++) {
            PetP& q = P[i];
            q.ph += dt * sp * 1.6f;
            float X, Y, Z, TX = 0, TY = 0, TZ = 0;
            if (fire) {
                q.ang += q.w * sp * dt; q.h += q.rise * sp * dt; q.r += __builtin_sinf(q.ph * 0.7f) * 0.3f * dt;
                if (q.kind == 3) q.h += __builtin_sinf(q.ph * 1.3f) * 0.6f * dt;
                if (q.h > (q.kind == 5 ? 4.5f : 7.5f) || q.r < 0.3f) { spawn(q, false); continue; }
                X = C[0] + q.r * __builtin_cosf(q.ang); Y = C[1] + q.h; Z = C[2] + q.r * __builtin_sinf(q.ang);
                const float a2 = q.ang - (q.w < 0.0f ? -q.len : q.len); TX = C[0] + q.r * __builtin_cosf(a2); TY = Y - 0.9f; TZ = C[2] + q.r * __builtin_sinf(a2);
            } else {
                q.p[0] += __builtin_cosf(q.ph) * 0.5f * sp * dt; q.p[2] += __builtin_sinf(q.ph * 0.9f) * 0.5f * sp * dt; q.p[1] -= q.fall * sp * dt;
                const float ddx = q.p[0] - f[0], ddy = q.p[1] - f[1], ddz = q.p[2] - f[2];
                if (ddy < -4.0f || ddx * ddx + ddz * ddz > 324.0f || ddy > 9.0f) { spawn(q, false); continue; }
                X = q.p[0]; Y = q.p[1]; Z = q.p[2]; TX = X + 0.25f * __builtin_cosf(q.ph); TY = Y + 0.9f; TZ = Z + 0.25f * __builtin_sinf(q.ph);
            }
            if (!haveRb) continue;
            float pxf, pyf; const float z = proj(X, Y, Z, pxf, pyf); if (z < 0.0f) continue;
            float s = q.sz * kpx / z; if (s < 2.0f) continue; if (s > 70.0f) s = 70.0f; if (pxf < -80.0f || pxf > static_cast<float>(W) + 80.0f || pyf < -80.0f || pyf > static_cast<float>(H) + 80.0f) continue;
            const PetCol& th0 = col == 6 ? kRbw[q.ci] : (fire ? kTh[0][0] : kTh[col][0]); const PetCol& th1 = fire ? kTh[0][1] : kTh[col][1]; const PetCol ash0 = { 60, 36, 28 }, ash1 = { 110, 56, 32 }, bf0 = { 224, 128, 24 }, bf1 = { 255, 214, 96 };
            switch (q.kind) {
            case 4: case 2: { /* 火の粉 / きらめき: 光る円。きらめきは十字の光も足す */
                int R = static_cast<int>(s * (q.kind == 4 ? 0.9f : 0.7f)) + 2; if (R > 30) R = 30; const int w = 2 * R + 2, h = w; clear(w, h); discG(w, h, static_cast<float>(w) * 0.5f, static_cast<float>(h) * 0.5f, static_cast<float>(R), q.kind == 4 ? 1.5f : 1.2f, 1.0f);
                if (q.kind == 2) { const float tw = 0.5f + 0.5f * __builtin_sinf(q.ph * 3.0f); const int a = static_cast<int>(static_cast<float>(R) * (0.4f + 0.6f * tw)); const int c0 = w / 2; for (int k = -a; k <= a; k++) { const int xx = c0 + k, yy = c0 + k; const int v = 255 - 255 * (k < 0 ? -k : k) / (a + 1); if (xx >= 0 && xx < w) { if (accA[c0 * 192 + xx] < v) accA[c0 * 192 + xx] = static_cast<u8>(v); if (accH[c0 * 192 + xx] < v) accH[c0 * 192 + xx] = static_cast<u8>(v); } if (yy >= 0 && yy < h) { if (accA[yy * 192 + c0] < v) accA[yy * 192 + c0] = static_cast<u8>(v); if (accH[yy * 192 + c0] < v) accH[yy * 192 + c0] = static_cast<u8>(v); } } }
                flush(static_cast<int>(pxf) - w / 2, static_cast<int>(pyf) - h / 2, w, h, 1, 0.95f, th0, th1, fire); break; }
            case 0: case 6: { /* 花びら / 灰の葉: だ円 (傾きがゆれる) */
                const float rxp = s * 0.55f, ryp = s * 0.32f; int R = static_cast<int>(rxp + ryp) + 2; if (R > 60) R = 60; const int w = 2 * R + 2, h = w; clear(w, h);
                ellA(w, h, static_cast<float>(w) * 0.5f, static_cast<float>(h) * 0.5f, rxp < 1.5f ? 1.5f : rxp, ryp < 1.2f ? 1.2f : ryp, q.ph * 1.7f + __builtin_sinf(q.ph * 1.3f), 0.9f);
                if (q.kind == 6) flush(static_cast<int>(pxf) - w / 2, static_cast<int>(pyf) - h / 2, w, h, 0, 0.85f, ash0, ash1, false);
                else flush(static_cast<int>(pxf) - w / 2, static_cast<int>(pyf) - h / 2, w, h, fire ? 1 : 0, 0.85f, fire ? bf0 : th0, fire ? bf1 : th1, false); break; }
            case 1: { /* 羽根: ななめの細い葉 */
                const float L = s * 0.9f; int R = static_cast<int>(L) + 2; if (R > 60) R = 60; const int w = 2 * R + 2, h = w; clear(w, h);
                ellA(w, h, static_cast<float>(w) * 0.5f, static_cast<float>(h) * 0.5f, L < 3.0f ? 3.0f : L, L * 0.16f < 1.1f ? 1.1f : L * 0.16f, 0.9f + 0.7f * __builtin_sinf(q.ph * 0.7f), 0.8f);
                flush(static_cast<int>(pxf) - w / 2, static_cast<int>(pyf) - h / 2, w, h, 0, 0.9f, th0, th1, false); break; }
            case 3: { /* 蝶: 羽ばたく 2 枚の羽。縁が濃く中が明るい。光る */
                const float fl = __builtin_sinf(q.ph * 5.0f); const float af = (fl < 0.0f ? -fl : fl) * 0.75f + 0.25f; const float wr = s * 0.5f * af, hr = s * 0.42f; int R = static_cast<int>(s * 1.05f) + 4; if (R > 90) R = 90; const int w = 2 * R + 2, h = w; clear(w, h);
                const float m = static_cast<float>(w) * 0.5f; const float ox = (wr < 1.2f ? 1.2f : wr) * 0.95f;
                ellA(w, h, m - ox, m - hr * 0.3f, (wr < 1.2f ? 1.2f : wr) * 1.05f, hr * 0.62f, -0.85f, 0.9f); ellA(w, h, m + ox, m - hr * 0.3f, (wr < 1.2f ? 1.2f : wr) * 1.05f, hr * 0.62f, 0.85f, 0.9f);
                ellA(w, h, m - ox * 0.55f, m + hr * 0.5f, (wr < 1.2f ? 1.2f : wr) * 0.62f, hr * 0.42f, 0.35f, 0.7f); ellA(w, h, m + ox * 0.55f, m + hr * 0.5f, (wr < 1.2f ? 1.2f : wr) * 0.62f, hr * 0.42f, -0.35f, 0.7f);
                flush(static_cast<int>(pxf) - w / 2, static_cast<int>(pyf) - h / 2, w, h, 0, 0.88f, bf0, bf1, false); break; }
            default: { /* 炎の帯: 頭から尾へ細くなる曲がった帯 (光る円をならべる)。頭が熱くて黄〜白、尾は赤 */
                float txf, tyf; const float zt = proj(TX, TY, TZ, txf, tyf); if (zt < 0.0f) break;
                const float dx = txf - pxf, dy = tyf - pyf, dist = __builtin_sqrtf(dx * dx + dy * dy); if (dist < 4.0f || dist > 170.0f) break;
                const float w0 = s * 0.8f < 3.0f ? 3.0f : (s * 0.8f > 26.0f ? 26.0f : s * 0.8f); const float nx = -dy / dist, ny = dx / dist, cv = 0.22f * dist * __builtin_sinf(q.ph * 0.5f); const float acv = cv < 0.0f ? -cv : cv, mg = w0 * 1.7f + acv + 3.0f;
                const float minx = (pxf < txf ? pxf : txf) - mg, miny = (pyf < tyf ? pyf : tyf) - mg, maxx = (pxf > txf ? pxf : txf) + mg, maxy = (pyf > tyf ? pyf : tyf) + mg;
                int bw = static_cast<int>(maxx - minx) + 1, bh = static_cast<int>(maxy - miny) + 1; if (bw > 168 || bh > 168) break; const int bx = static_cast<int>(minx), by = static_cast<int>(miny); clear(bw, bh);
                int N = static_cast<int>(dist / 3.0f); if (N < 4) N = 4; if (N > 36) N = 36;
                for (int k = 0; k <= N; k++) { const float t = static_cast<float>(k) / static_cast<float>(N), bend = cv * __builtin_sinf(3.1415927f * t); const float ex = pxf + dx * t + nx * bend - static_cast<float>(bx), ey = pyf + dy * t + ny * bend - static_cast<float>(by); const float rr = w0 * (1.0f - 0.8f * t); if ((k & 1) == 0) discG(bw, bh, ex, ey, (rr < 1.5f ? 1.5f : rr) * 1.7f, 0.3f, 0.34f); discG(bw, bh, ex, ey, rr < 1.5f ? 1.5f : rr, 1.1f - 0.8f * t, 1.0f); }
                flush(bx, by, bw, bh, 1, 0.95f, th0, th1, true); break; }
            }
        }
        /* 作った絵をまとめて書き出して、写す */
        if (ncp > 0) { DcClean(spb, spUsed); for (int k = 0; k < ncp; k++) { const Cp& c = cps[k]; g_Sw.rowStride(cb, static_cast<ptrdiff_t>(c.w) * 4); Reg r { c.x, c.y, 0, c.w, c.h, 1 }; g_Sw.copyB2T(cb, base + c.off, tex, nullptr, &r, 0); } }
    }

}
int main(int argc,char**argv){
  FW=1280; FH=720; g_PixStorage=(u8*)calloc(kPixSlot*2+4096,1); g_BufAddr[0]=(u64)g_PixStorage; g_BufAddr[1]=(u64)g_PixStorage+kPixSlot;
  std::vector<u32> bgf(FW*FH);
  for(int y=0;y<FH;y++) for(int x=0;x<FW;x++){ int r,g,b; if(y<400){ float t=y/400.f; r=(int)(20+30*t); g=(int)(25+30*t); b=(int)(70+80*t);} else { float t=(y-400)/320.f; r=(int)(80-30*t); g=(int)(80-30*t); b=(int)(95-30*t);} if(((x/40)+(y/40))&1){r+=6;g+=6;b+=6;} bgf[y*FW+x]=0xFF000000u|(b<<16)|(g<<8)|r; }
  g_frame=bgf;
  g_cam[0]=0;g_cam[1]=3;g_cam[2]=0; g_cam[3]=0;g_cam[4]=2.5;g_cam[5]=-1; g_CamPos=(uintptr_t)(g_cam+3); g_FootPos=(uintptr_t)g_foot; g_foot[0]=0;g_foot[1]=0;g_foot[2]=-4;
  int kind=argc>1?atoi(argv[1]):7, col=argc>2?atoi(argv[2]):7; g_PetKind=kind; g_PetCol=col; g_PetNum=argc>3?atoi(argv[3]):6;
  for(int fr=0;fr<150;fr++){ g_frame=bgf; g_SwCur=fr&1; PetDrawSp(nullptr,nullptr,g_BufAddr[g_SwCur],FW,FH); }
  FILE*o=fopen(argc>4?argv[4]:"out.ppm","wb"); fprintf(o,"P6\n%d %d\n255\n",FW,FH); for(int i=0;i<FW*FH;i++){ u32 p=g_frame[i]; fputc(p&255,o); fputc((p>>8)&255,o); fputc((p>>16)&255,o);} fclose(o);
}
