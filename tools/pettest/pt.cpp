
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
bool MenuVis(){return false;} bool FuncVis(){return false;} bool SplashOn(){return false;} bool HudWanted(){return false;} bool UpdWanted(){return false;}
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
        if (!g_T2B || !g_PixCached || W <= 0 || H <= 0 || W > 3840 || H > 2160) return; /* 据え置き 1920x1080 でも動かす (以前は 1440x960 より大きいと何も出なかった) */
        const uintptr_t cp = g_CamPos; if (!cp || !CamValid() || !Readable(cp - 0xC, 24)) return;
        const size_t used = (MenuVis() || FuncVis() || SplashOn() || HudWanted() || UpdWanted()) ? ((g_PixHi + 255u) & ~static_cast<size_t>(255u)) : 0; /* メニューなどが出ていないときは、古い絵の大きさを気にしない */
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
            const int qi = static_cast<int>(&q - P);
            if (kind == 7) { const float u = rnd(); if (fire) kd = (qi & 7) == 0 ? 8 : (u < 0.5f ? 4 : (u < 0.54f ? 3 : (u < 0.64f ? 5 : (u < 0.82f ? 6 : (u < 0.92f ? 2 : 0))))); else kd = u < 0.4f ? 0 : (u < 0.62f ? 1 : (u < 0.9f ? 2 : (u < 0.94f ? 3 : 4))); }
            else if (fire && kind == 5) kd = 8; /* 大きな炎 */
            q.kind = kd; q.ph = rnd() * 6.2831853f; q.len = kd == 5 ? 0.7f + 0.6f * rnd() : 0.35f + 0.35f * rnd(); q.ci = static_cast<int>(rnd() * 5.99f); q.ang = rnd() * 6.2831853f; q.fall = 0.0f;
            if (fire) {
                float hh = 0.0f; const float ang0 = rnd() * 6.2831853f; float rr = 0.6f + 5.0f * rnd(); q.rise = 0.0f; q.w = 0.0f;
                switch (kd) {
                case 8: rr = 3.0f + 4.5f * rnd(); hh = 0.0f; q.sz = 1.8f + 2.4f * rnd(); q.w = 0.45f + 0.4f * rnd(); break;
                case 4: hh = first ? 7.0f * rnd() : -0.2f + 0.8f * rnd(); q.rise = 0.8f + 1.6f * rnd(); q.sz = 0.07f + 0.09f * rnd(); break;
                case 3: rr = 1.0f + 4.5f * rnd(); hh = 0.3f + 2.5f * rnd(); q.sz = 0.2f + 0.14f * rnd(); break;
                case 5: hh = first ? 4.0f * rnd() : -0.2f + 0.5f * rnd(); q.rise = 1.0f + 1.8f * rnd(); q.sz = 0.32f + 0.25f * rnd(); break;
                case 6: hh = first ? 6.0f * rnd() : -0.2f + 0.6f * rnd(); q.rise = 0.5f + 1.0f * rnd(); q.sz = 0.08f + 0.08f * rnd(); break;
                case 2: hh = 0.2f + 4.0f * rnd(); q.rise = 0.3f; q.sz = 0.12f + 0.12f * rnd(); break;
                default: hh = first ? 6.0f * rnd() : -0.2f + 0.6f * rnd(); q.rise = 0.5f + 1.0f * rnd(); q.sz = 0.12f + 0.10f * rnd(); break;
                }
                q.p[0] = C[0] + rr * __builtin_cosf(ang0); q.p[2] = C[2] + rr * __builtin_sinf(ang0); q.p[1] = C[1] + hh; /* ほかと同じに、空間に置く (自分の動きにはついてこない) */
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
        struct Cp { int x, y, w, h; size_t off; }; static Cp cps[1100]; int ncp = 0; int pixB = 150000; /* 写す四角 (横の帯)・1 フレームで作る点の数の上限 */
        /* 1 粒の絵: 場所 (x0,y0)・大きさ (w,h) を、accA/accH (sc 倍に粗い) から作る。粒がある所だけを、横の帯 (数行ずつ・粒のある幅だけ) にして写す
           (四角全体を写すと、粒のまわりに背景の読み出しの四角が見えて、モザイクになる)。mode 0 = ふつうに重ねる、1 = 光る (足し算気味)。 */
        auto flush = [&](int x0, int y0, int w, int h, int sc, int mode, float alpha, const PetCol& c0, const PetCol& c1, bool fireRamp) {
            const int aw = (w + sc - 1) / sc, ah = (h + sc - 1) / sc; if (aw > 192 || ah > 192) return;
            const int vy0 = y0 < 0 ? 0 : y0, vy1 = y0 + h > H ? H : y0 + h, vx0 = x0 < 0 ? 0 : x0, vx1 = x0 + w > W ? W : x0 + w; if (vy0 >= vy1 || vx0 >= vx1) return;
            const int ia = static_cast<int>(alpha * 256.0f);
            int lr[256], lg[256], lb[256]; for (int k = 0; k < 256; k++) { const float hh = static_cast<float>(k) * (1.0f / 255.0f); if (fireRamp) { const PetCol c = PetFire(hh); lr[k] = c.r; lg[k] = c.g; lb[k] = c.b; } else { lr[k] = static_cast<int>(c0.r + (c1.r - c0.r) * hh); lg[k] = static_cast<int>(c0.g + (c1.g - c0.g) * hh); lb[k] = static_cast<int>(c0.b + (c1.b - c0.b) * hh); } }
            short e0[192], e1[192]; for (int ay = 0; ay < ah; ay++) { int a0 = -1, a1 = -1; const u8* ra = accA + ay * 192; for (int ax = 0; ax < aw; ax++) if (ra[ax] > 2) { if (a0 < 0) a0 = ax; a1 = ax; } e0[ay] = static_cast<short>(a0); e1[ay] = static_cast<short>(a1); }
            const int BH = (vy1 - vy0 + 15) / 16 < 8 ? 8 : (vy1 - vy0 + 15) / 16;
            for (int by = vy0; by < vy1; by += BH) {
                const int bh = by + BH > vy1 ? vy1 - by : BH; int ya = (by - y0) / sc - 1, yb = (by + bh - 1 - y0) / sc + 1; if (ya < 0) ya = 0; if (yb >= ah) yb = ah - 1;
                int mn = 100000, mx = -1; for (int ay = ya; ay <= yb; ay++) if (e1[ay] >= 0) { if (e0[ay] < mn) mn = e0[ay]; if (e1[ay] > mx) mx = e1[ay]; }
                if (mx < 0) continue;
                int bx0 = x0 + (mn - 1) * sc, bx1 = x0 + (mx + 2) * sc; if (bx0 < vx0) bx0 = vx0; if (bx1 > vx1) bx1 = vx1; if (bx1 <= bx0) continue;
                const int bw = bx1 - bx0; const size_t need = static_cast<size_t>(bw) * bh * 4u;
                if (ncp >= 1000 || spUsed + need > kPetSpBytes || pixB <= 0) return; pixB -= bw * bh;
                u32* o = spb + spUsed / 4;
                int ov[10], nov = 0; for (int k = ncp - 1; k >= 0 && nov < 10; k--) { const Cp& e = cps[k]; if (e.x < bx1 && e.x + e.w > bx0 && e.y < by + bh && e.y + e.h > by) ov[nov++] = k; } /* 先に作った粒と重なるところは、その絵を背景にする */
                for (int y = 0; y < bh; y++) {
                    const int yy = by + y; int r0 = yy / step, r1 = r0 + 1; const u32 tw = static_cast<u32>((yy - r0 * step) * 256 / step); if (r0 >= rows) r0 = rows - 1; if (r1 >= rows) r1 = rows - 1;
                    const u32* ra = rb + static_cast<size_t>(r0) * W + bx0; const u32* rb1 = rb + static_cast<size_t>(r1) * W + bx0;
                    const int ly = yy - y0; int iy = 0, fy = 0; if (sc > 1) { const int t = (2 * ly + 1) * 128 / sc - 128; iy = t >> 8; fy = t & 255; if (iy < 0) { iy = 0; fy = 0; } if (iy >= ah - 1) { iy = ah - 1; fy = 0; } } else iy = ly;
                    for (int x = 0; x < bw; x++) {
                        u32 bg = tw ? LerpPx(ra[x], rb1[x], tw) : ra[x];
                        for (int q2 = 0; q2 < nov; q2++) { const Cp& e = cps[ov[q2]]; const int ex2 = bx0 + x - e.x, ey2 = yy - e.y; if (ex2 >= 0 && ex2 < e.w && ey2 >= 0 && ey2 < e.h) { bg = spb[(e.off - spOff) / 4 + static_cast<size_t>(ey2) * e.w + ex2]; break; } }
                        const int lx = bx0 + x - x0; int A, Hh;
                        if (sc == 1) { A = accA[iy * 192 + lx]; Hh = accH[iy * 192 + lx]; }
                        else { const int t = (2 * lx + 1) * 128 / sc - 128; int ix = t >> 8, fx = t & 255; if (ix < 0) { ix = 0; fx = 0; } if (ix >= aw - 1) { ix = aw - 1; fx = 0; } const int i2 = iy + (fy ? 1 : 0), j2 = ix + (fx ? 1 : 0); const u8* qa = accA; const u8* qh = accH;
                            const int a00 = qa[iy * 192 + ix], a01 = qa[iy * 192 + j2], a10 = qa[i2 * 192 + ix], a11 = qa[i2 * 192 + j2]; A = ((a00 * (256 - fx) + a01 * fx) * (256 - fy) + (a10 * (256 - fx) + a11 * fx) * fy) >> 16;
                            const int h00 = qh[iy * 192 + ix], h01 = qh[iy * 192 + j2], h10 = qh[i2 * 192 + ix], h11 = qh[i2 * 192 + j2]; Hh = ((h00 * (256 - fx) + h01 * fx) * (256 - fy) + (h10 * (256 - fx) + h11 * fx) * fy) >> 16; }
                        const int a = A * ia / 255; if (a <= 0) { o[y * bw + x] = bg | 0xFF000000u; continue; }
                        const int br = bg & 255, bgc = (bg >> 8) & 255, bb = (bg >> 16) & 255; int r, g, b;
                        if (mode == 0) { r = (br * (256 - a) + lr[Hh] * a) >> 8; g = (bgc * (256 - a) + lg[Hh] * a) >> 8; b = (bb * (256 - a) + lb[Hh] * a) >> 8; }
                        else { const int k = 256 - (a * 90 >> 8); r = PetClamp(((br * k) >> 8) + (lr[Hh] * a >> 8)); g = PetClamp(((bgc * k) >> 8) + (lg[Hh] * a >> 8)); b = PetClamp(((bb * k) >> 8) + (lb[Hh] * a >> 8)); }
                        o[y * bw + x] = 0xFF000000u | (static_cast<u32>(b) << 16) | (static_cast<u32>(g) << 8) | static_cast<u32>(r);
                    }
                }
                cps[ncp++] = Cp { bx0, by, bw, bh, spOff + spUsed }; spUsed += need;
            }
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
        bool grB = false;
        for (int pass = 0; pass < 2; pass++) for (int i = 0; i < n; i++) {
            PetP& q = P[i]; if ((q.kind == 8) != (pass == 0)) continue; if (pass == 1 && !grB) { grB = true; pixB += 120000; }
            if (pass == 0) q.ph += dt * sp * 1.6f; else q.ph += dt * sp * 1.6f;
            float X, Y, Z, TX = 0, TY = 0, TZ = 0;
            if (fire) {
                if (q.kind != 8) { q.p[0] += __builtin_cosf(q.ph) * 0.35f * sp * dt; q.p[2] += __builtin_sinf(q.ph * 0.9f) * 0.35f * sp * dt; q.p[1] += q.rise * sp * dt; if (q.kind == 3) q.p[1] += __builtin_sinf(q.ph * 1.3f) * 0.6f * dt; }
                const float ddx = q.p[0] - C[0], ddz = q.p[2] - C[2], dh = q.p[1] - C[1];
                if (dh > (q.kind == 5 ? 4.5f : 7.5f) || ddx * ddx + ddz * ddz > (q.kind == 8 ? 110.0f : 64.0f) || (q.kind == 8 && ddx * ddx + ddz * ddz < 6.0f)) { spawn(q, false); continue; }
                X = q.p[0]; Y = q.p[1]; Z = q.p[2]; TX = X - 0.25f * __builtin_cosf(q.ph); TY = Y - 0.9f; TZ = Z - 0.25f * __builtin_sinf(q.ph);
            } else {
                q.p[0] += __builtin_cosf(q.ph) * 0.5f * sp * dt; q.p[2] += __builtin_sinf(q.ph * 0.9f) * 0.5f * sp * dt; q.p[1] -= q.fall * sp * dt;
                const float ddx = q.p[0] - f[0], ddy = q.p[1] - f[1], ddz = q.p[2] - f[2];
                if (ddy < -4.0f || ddx * ddx + ddz * ddz > 324.0f || ddy > 9.0f) { spawn(q, false); continue; }
                X = q.p[0]; Y = q.p[1]; Z = q.p[2]; TX = X + 0.25f * __builtin_cosf(q.ph); TY = Y + 0.9f; TZ = Z + 0.25f * __builtin_sinf(q.ph);
            }
            if (!haveRb) continue;
            float pxf, pyf; const float z = proj(X, Y, Z, pxf, pyf); if (z < 0.0f) continue;
            float s = q.sz * kpx / z; if (q.kind != 8) { if (s < 2.0f) continue; if (s > 70.0f) s = 70.0f; if (pxf < -80.0f || pxf > static_cast<float>(W) + 80.0f || pyf < -80.0f || pyf > static_cast<float>(H) + 80.0f) continue; }
            const PetCol& th0 = col == 6 ? kRbw[q.ci] : (fire ? kTh[0][0] : kTh[col][0]); const PetCol& th1 = fire ? kTh[0][1] : kTh[col][1]; const PetCol ash0 = { 60, 36, 28 }, ash1 = { 110, 56, 32 }, bf0 = { 224, 128, 24 }, bf1 = { 255, 214, 96 };
            switch (q.kind) {
            case 4: case 2: { /* 火の粉 / きらめき: 光る円。きらめきは十字の光も足す */
                int R = static_cast<int>(s * (q.kind == 4 ? 0.9f : 0.7f)) + 2; if (R > 30) R = 30; const int w = 2 * R + 2, h = w; clear(w, h); discG(w, h, static_cast<float>(w) * 0.5f, static_cast<float>(h) * 0.5f, static_cast<float>(R), q.kind == 4 ? 1.5f : 1.2f, 1.0f);
                if (q.kind == 2) { const float tw = 0.5f + 0.5f * __builtin_sinf(q.ph * 3.0f); const int a = static_cast<int>(static_cast<float>(R) * (0.4f + 0.6f * tw)); const int c0 = w / 2; for (int k = -a; k <= a; k++) { const int xx = c0 + k, yy = c0 + k; const int v = 255 - 255 * (k < 0 ? -k : k) / (a + 1); if (xx >= 0 && xx < w) { if (accA[c0 * 192 + xx] < v) accA[c0 * 192 + xx] = static_cast<u8>(v); if (accH[c0 * 192 + xx] < v) accH[c0 * 192 + xx] = static_cast<u8>(v); } if (yy >= 0 && yy < h) { if (accA[yy * 192 + c0] < v) accA[yy * 192 + c0] = static_cast<u8>(v); if (accH[yy * 192 + c0] < v) accH[yy * 192 + c0] = static_cast<u8>(v); } } }
                flush(static_cast<int>(pxf) - w / 2, static_cast<int>(pyf) - h / 2, w, h, 1, 1, 0.95f, th0, th1, fire); break; }
            case 0: case 6: { /* 花びら / 灰の葉: だ円 (傾きがゆれる) */
                const float rxp = s * 0.55f, ryp = s * 0.32f; int R = static_cast<int>(rxp + ryp) + 2; if (R > 60) R = 60; const int w = 2 * R + 2, h = w; clear(w, h);
                ellA(w, h, static_cast<float>(w) * 0.5f, static_cast<float>(h) * 0.5f, rxp < 1.5f ? 1.5f : rxp, ryp < 1.2f ? 1.2f : ryp, q.ph * 1.7f + __builtin_sinf(q.ph * 1.3f), 0.9f);
                if (q.kind == 6) flush(static_cast<int>(pxf) - w / 2, static_cast<int>(pyf) - h / 2, w, h, 1, 0, 0.85f, ash0, ash1, false);
                else flush(static_cast<int>(pxf) - w / 2, static_cast<int>(pyf) - h / 2, w, h, 1, fire ? 1 : 0, 0.85f, fire ? bf0 : th0, fire ? bf1 : th1, false); break; }
            case 1: { /* 羽根: ななめの細い葉 */
                const float L = s * 0.9f; int R = static_cast<int>(L) + 2; if (R > 60) R = 60; const int w = 2 * R + 2, h = w; clear(w, h);
                ellA(w, h, static_cast<float>(w) * 0.5f, static_cast<float>(h) * 0.5f, L < 3.0f ? 3.0f : L, L * 0.16f < 1.1f ? 1.1f : L * 0.16f, 0.9f + 0.7f * __builtin_sinf(q.ph * 0.7f), 0.8f);
                flush(static_cast<int>(pxf) - w / 2, static_cast<int>(pyf) - h / 2, w, h, 1, 0, 0.9f, th0, th1, false); break; }
            case 3: { /* 蝶: 金色の 4 枚の羽 (上の羽は大きく、下は小さく)。縁が明るく、中は透けて、はねの筋が見える。体は細い線。まわりにうすい光 */
                const float fl = __builtin_sinf(q.ph * 5.0f); const float af = (fl < 0.0f ? -fl : fl) * 0.8f + 0.2f; const float th = 0.3f * __builtin_sinf(q.ph * 0.8f);
                int R = static_cast<int>(s * 1.25f) + 4; if (R > 90) R = 90; const int w = 2 * R + 2, h = w; clear(w, h);
                const float m = static_cast<float>(w) * 0.5f, cs = __builtin_cosf(th), sn = __builtin_sinf(th), us = 1.0f / (s * 0.95f);
                static const float kW[2][6] = { { 0.50f, -0.30f, 0.56f, 0.34f, -0.55f, 0.0f }, { 0.34f, 0.32f, 0.36f, 0.24f, 0.55f, 0.0f } }; /* 羽: 中心 (u,v)・半径 (a,b)・角度 */
                for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
                    const float dx = static_cast<float>(x) + 0.5f - m, dy = static_cast<float>(y) + 0.5f - m; const float rxp = (dx * cs + dy * sn) * us, ryp = (-dx * sn + dy * cs) * us;
                    const float u = (rxp < 0.0f ? -rxp : rxp) / af, v = ryp; int A = 0, Hh = 0;
                    for (int k = 0; k < 2; k++) {
                        const float du = u - kW[k][0], dv = v - kW[k][1], c = __builtin_cosf(kW[k][4]), s2 = __builtin_sinf(kW[k][4]); const float pa = (du * c + dv * s2) / kW[k][2], pb = (-du * s2 + dv * c) / kW[k][3]; const float d2 = pa * pa + pb * pb; if (d2 >= 1.0f) continue;
                        const float e = 1.0f - __builtin_sqrtf(d2); float cov = e * 6.0f; if (cov > 1.0f) cov = 1.0f; float rim = 1.0f - e * 3.2f; if (rim < 0.0f) rim = 0.0f;
                        const float ang = dv + (k ? -0.2f : 0.3f), vein = __builtin_cosf((ang / (u + 0.12f)) * 5.0f); const float vn = vein > 0.7f ? (vein - 0.7f) * 2.0f : 0.0f;
                        int a2 = static_cast<int>(cov * (0.5f + 0.5f * rim + 0.2f * vn) * 255.0f), h2 = static_cast<int>((0.38f + 0.55f * rim + 0.25f * vn) * 255.0f); if (a2 > 255) a2 = 255; if (h2 > 255) h2 = 255; if (a2 > A) A = a2; if (h2 > Hh) Hh = h2;
                    }
                    const float bd = (rxp < 0.0f ? -rxp : rxp); if (bd < 0.035f + 0.5f / s && ryp > -0.42f && ryp < 0.5f) { A = 240; Hh = 20; }
                    const float r2 = (dx * dx + dy * dy) * us * us; if (r2 < 1.7f) { const int ga = static_cast<int>((1.0f - r2 * 0.59f) * (1.0f - r2 * 0.59f) * 70.0f); if (ga > A) { A = ga; Hh = 130; } }
                    if (A > 0) { accA[y * 192 + x] = static_cast<u8>(A); accH[y * 192 + x] = static_cast<u8>(Hh); }
                }
                flush(static_cast<int>(pxf) - w / 2, static_cast<int>(pyf) - h / 2, w, h, 1, 1, 1.0f, bf0, bf1, false); break; }
            case 8: { /* 大きな炎: 地面から立ちのぼる 3 本の炎の舌。ゆらぎ・先細り・中心が熱い。遠いものは粗く作って大きくのばす */
                float tx2, ty2; const float zt = proj(X, Y + q.sz, Z, tx2, ty2); if (zt < 0.0f) break;
                const float ww = q.w * kpx / z, ddx = tx2 - pxf, ddy = ty2 - pyf, hh = __builtin_sqrtf(ddx * ddx + ddy * ddy); if (hh < 8.0f || ww < 2.0f) break;
                const float mg = ww * 2.3f + 6.0f; const float minx = (pxf < tx2 ? pxf : tx2) - mg, maxx = (pxf > tx2 ? pxf : tx2) + mg, miny = (pyf < ty2 ? pyf : ty2) - mg, maxy = (pyf > ty2 ? pyf : ty2) + mg;
                const int ix0 = minx < 0.0f ? 0 : static_cast<int>(minx), iy0 = miny < 0.0f ? 0 : static_cast<int>(miny), ix1 = maxx > static_cast<float>(W) ? W : static_cast<int>(maxx) + 1, iy1 = maxy > static_cast<float>(H) ? H : static_cast<int>(maxy) + 1; if (ix1 - ix0 < 4 || iy1 - iy0 < 4) break;
                const int wW = ix1 - ix0, wH = iy1 - iy0; int sc = 1; while ((wW / sc + 1) * (wH / sc + 1) > 5200 || wW / sc + 1 > 190 || wH / sc + 1 > 190) sc++;
                const int aw = (wW + sc - 1) / sc, ah = (wH + sc - 1) / sc; clear(aw, ah);
                const float ax = ddx / hh, ay = ddy / hh, nx = -ay, ny = ax;
                static const float kTg[3][3] = { { 0.0f, 1.0f, 1.0f }, { -0.85f, 0.68f, 0.72f }, { 0.9f, 0.52f, 0.62f } };
                for (int tg = 0; tg < 3; tg++) {
                    const float hs = hh * kTg[tg][1]; float lw[64], lc[64]; const float pp = q.ph * 0.9f + static_cast<float>(tg) * 2.1f;
                    for (int k = 0; k < 64; k++) { const float t = static_cast<float>(k) / 63.0f; const float b = (0.7f + 1.3f * t) * __builtin_powf(1.0f - t * 0.985f, 0.9f); lw[k] = ww * kTg[tg][2] * b * (1.0f + 0.18f * __builtin_sinf(t * 11.0f + pp * 3.0f)); lc[k] = ww * 0.8f * __builtin_sinf(pp + t * 3.5f) * t; }
                    const float bpx = pxf + nx * kTg[tg][0] * ww, bpy = pyf + ny * kTg[tg][0] * ww;
                    for (int cy2 = 0; cy2 < ah; cy2++) for (int cx2 = 0; cx2 < aw; cx2++) {
                        const float px = static_cast<float>(ix0) + (static_cast<float>(cx2) + 0.5f) * static_cast<float>(sc) - bpx, py = static_cast<float>(iy0) + (static_cast<float>(cy2) + 0.5f) * static_cast<float>(sc) - bpy;
                        const float along = px * ax + py * ay; if (along < -3.0f || along > hs) continue; const float t = along < 0.0f ? 0.0f : along / hs; const int k = static_cast<int>(t * 63.0f);
                        const float lat = px * nx + py * ny - lc[k]; const float wd = lw[k]; if (wd < 0.8f) continue; const float l = (lat < 0.0f ? -lat : lat) / wd; if (l >= 1.0f) continue;
                        const float e = 1.0f - l; float cov = e * 3.0f; if (cov > 1.0f) cov = 1.0f; cov = cov * cov * (3.0f - 2.0f * cov) * (1.0f - 0.25f * t); { const float bf = (along + 3.0f) / (hh * 0.12f + 4.0f); if (bf < 1.0f) cov *= bf; } float ht = (0.28f + 0.5f * e) * (1.0f - 0.45f * t); if (ht < 0.0f) ht = 0.0f;
                        const int A = static_cast<int>(cov * 250.0f), Hh = static_cast<int>(ht * 255.0f); if (A > accA[cy2 * 192 + cx2]) accA[cy2 * 192 + cx2] = static_cast<u8>(A); if (Hh > accH[cy2 * 192 + cx2]) accH[cy2 * 192 + cx2] = static_cast<u8>(Hh);
                    }
                }
                flush(ix0, iy0, wW, wH, sc, 0, 0.86f, th0, th1, true); break; }
            default: { /* 炎の帯: 頭から尾へ細くなる曲がった帯 (光る円をならべる)。頭が熱くて黄〜白、尾は赤 */
                float txf, tyf; const float zt = proj(TX, TY, TZ, txf, tyf); if (zt < 0.0f) break;
                const float dx = txf - pxf, dy = tyf - pyf, dist = __builtin_sqrtf(dx * dx + dy * dy); if (dist < 4.0f || dist > 170.0f) break;
                const float w0 = s * 0.8f < 3.0f ? 3.0f : (s * 0.8f > 26.0f ? 26.0f : s * 0.8f); const float nx = -dy / dist, ny = dx / dist, cv = 0.22f * dist * __builtin_sinf(q.ph * 0.5f); const float acv = cv < 0.0f ? -cv : cv, mg = w0 * 1.7f + acv + 3.0f;
                const float minx = (pxf < txf ? pxf : txf) - mg, miny = (pyf < tyf ? pyf : tyf) - mg, maxx = (pxf > txf ? pxf : txf) + mg, maxy = (pyf > tyf ? pyf : tyf) + mg;
                int bw = static_cast<int>(maxx - minx) + 1, bh = static_cast<int>(maxy - miny) + 1; if (bw > 168 || bh > 168) break; const int bx = static_cast<int>(minx), by = static_cast<int>(miny); clear(bw, bh);
                int N = static_cast<int>(dist / 3.0f); if (N < 4) N = 4; if (N > 36) N = 36;
                for (int k = 0; k <= N; k++) { const float t = static_cast<float>(k) / static_cast<float>(N), bend = cv * __builtin_sinf(3.1415927f * t); const float ex = pxf + dx * t + nx * bend - static_cast<float>(bx), ey = pyf + dy * t + ny * bend - static_cast<float>(by); const float rr = w0 * (1.0f - 0.8f * t); if ((k & 1) == 0) discG(bw, bh, ex, ey, (rr < 1.5f ? 1.5f : rr) * 1.7f, 0.3f, 0.34f); discG(bw, bh, ex, ey, rr < 1.5f ? 1.5f : rr, 1.1f - 0.8f * t, 1.0f); }
                flush(bx, by, bw, bh, 1, 1, 0.95f, th0, th1, true); break; }
            }
        }
        /* 作った絵をまとめて書き出して、写す */
        if (ncp > 0) { DcClean(spb, spUsed); int lastPw = -1; for (int k = 0; k < ncp; k++) { const Cp& c = cps[k]; if (c.w != lastPw) { g_Sw.rowStride(cb, static_cast<ptrdiff_t>(c.w) * 4); lastPw = c.w; } Reg r { c.x, c.y, 0, c.w, c.h, 1 }; g_Sw.copyB2T(cb, base + c.off, tex, nullptr, &r, 0); } }
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
