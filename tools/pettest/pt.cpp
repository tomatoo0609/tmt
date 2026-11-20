
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
volatile float g_PetFade=0; volatile bool g_PetOn=true, g_PetFlip=false; volatile int g_PetKind=7, g_PetCol=7, g_PetNum=4, g_PetSpd=5, g_FovLv=0;
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
    /* 蝶 (半透明): 自分の足元のまわりを飛ぶ。蝶のある所だけ、前のフレームの画面 (蝶のまわりの四角を、細かさそのままで読んだもの) と混ぜて写す。
       入り切りは、はかなく消える (出るときは少しずつ、消えるときは小さくなりながら 0.3 秒) */
    void PetDrawSp(void* cb, void* tex, u64 base, int W, int H) {
        static u64 lastT = 0; static float tt = 0.0f, fade = 0.0f, P[8][3], ph[8]; static bool inited = false; static u8 accA[192 * 192], accH[192 * 192];
        struct Bx { int x, y, w, h; bool ok; }; static Bx prev[8];
        struct Reg { int x, y, z, w, h, d; };
        const u64 now = NowTick(); float dt = lastT ? static_cast<float>(now - lastT) / 19200000.0f : 0.0f; lastT = now; if (dt > 0.1f) dt = 0.1f; if (dt < 0.0f) dt = 0.0f;
        if (g_PetOn) { fade += dt / 0.4f; if (fade > 1.0f) fade = 1.0f; } else { fade -= dt / 0.3f; if (fade < 0.0f) fade = 0.0f; }
        g_PetFade = fade;
        if (fade <= 0.0f) { inited = false; for (int k = 0; k < 8; k++) prev[k].ok = false; return; }
        if (!g_T2B || !g_PixCached || W <= 0 || H <= 0 || W > 3840 || H > 2160) return;
        const uintptr_t cp = g_CamPos; if (!cp || !CamValid() || !Readable(cp - 0xC, 24)) return;
        const size_t used = (MenuVis() || FuncVis() || SplashOn() || HudWanted() || UpdWanted()) ? ((g_PixHi + 255u) & ~static_cast<size_t>(255u)) : 0;
        const size_t spOff = kPixBytes - kPetPal - kPetSpBytes, slotB = (kPetRbMax / 8) & ~static_cast<size_t>(4095u), rbBase = (spOff - 8 * slotB) & ~static_cast<size_t>(255u);
        if (rbBase < used) return;
        float f[6]; std::memcpy(f, reinterpret_cast<const void*>(cp - 0xC), 24);
        float fx = f[3] - f[0], fy = f[4] - f[1], fz = f[5] - f[2]; const float m2 = fx * fx + fy * fy + fz * fz;
        if (!(m2 == m2) || m2 < 1.0f || m2 > 1600.0f) return;
        const float im = 1.0f / __builtin_sqrtf(m2); fx *= im; fy *= im; fz *= im;
        const float ch = __builtin_sqrtf(fx * fx + fz * fz); float rx = 1.0f, rz = 0.0f; if (ch > 0.05f) { rx = -fz / ch; rz = fx / ch; }
        if (g_PetFlip) { rx = -rx; rz = -rz; }
        const float ux = -rz * fy, uy = rz * fx - rx * fz, uz = rx * fy; /* 上 = 右 x 前 */
        float C[3] = { f[0] + fx * 3.5f, f[1] - 1.3f, f[2] + fz * 3.5f }; /* 自分の足元 (分からないときは、カメラの前 3.5m) */
        { const uintptr_t fp = g_FootPos; if (fp && FootValid() && Readable(fp, 12)) { float q3[3]; std::memcpy(q3, reinterpret_cast<const void*>(fp), 12); if (q3[0] == q3[0] && q3[1] == q3[1] && q3[2] == q3[2]) { const float ddx = q3[0] - f[0], ddy = q3[1] - f[1], ddz = q3[2] - f[2]; if (ddx * ddx + ddy * ddy + ddz * ddz < 900.0f) { C[0] = q3[0]; C[1] = q3[1]; C[2] = q3[2]; } } } }
        tt += dt; const int n = g_PetNum < 1 ? 1 : (g_PetNum > 8 ? 8 : g_PetNum);
        const float vh = static_cast<float>(g_FovLv > 0 ? 55 + 5 * g_FovLv : 55) * 0.5f * 0.017453293f;
        const float kpx = static_cast<float>(H) * 0.5f * __builtin_cosf(vh) / __builtin_sinf(vh);
        const float cx = static_cast<float>(W) * 0.5f, cy = static_cast<float>(H) * 0.5f;
        auto proj = [&](float X, float Y, float Z, float& px, float& py) -> float { const float dx = X - f[0], dy = Y - f[1], dz = Z - f[2]; const float z = dx * fx + dy * fy + dz * fz; if (z < 0.7f) return -1.0f; const float xr = dx * rx + dz * rz, yu = dx * ux + dy * uy + dz * uz; px = cx + xr / z * kpx; py = cy - yu / z * kpx; return z; };
        u32* spb = reinterpret_cast<u32*>(g_PixStorage + static_cast<size_t>(g_SwCur) * kPixSlot + spOff); size_t spUsed = 0;
        struct Cp { int x, y, w, h; size_t off; }; static Cp cps[256]; int ncp = 0;
        static const PetCol bf0 = { 224, 128, 24 }, bf1 = { 255, 214, 96 };
        int lr[256], lg[256], lb[256]; for (int k = 0; k < 256; k++) { const float hh = static_cast<float>(k) * (1.0f / 255.0f); lr[k] = static_cast<int>(bf0.r + (bf1.r - bf0.r) * hh); lg[k] = static_cast<int>(bf0.g + (bf1.g - bf0.g) * hh); lb[k] = static_cast<int>(bf0.b + (bf1.b - bf0.b) * hh); }
        for (int i = 0; i < n; i++) {
            const float fi = static_cast<float>(i);
            /* 飛ぶ道: 足元のまわりをゆっくり回りながら、近づいたり離れたり、上下にゆれる */
            const float a = tt * (0.45f + 0.07f * fi) * (i & 1 ? -1.0f : 1.0f) + fi * 1.2566f, r = 1.1f + 0.15f * fi + 0.4f * __builtin_sinf(tt * 0.7f + fi * 1.7f), hgt = 0.9f + 0.18f * fi + 0.45f * __builtin_sinf(tt * 1.1f + fi * 2.3f);
            const float tx = C[0] + r * __builtin_cosf(a), ty = C[1] + hgt, tz = C[2] + r * __builtin_sinf(a);
            if (!inited) { P[i][0] = tx; P[i][1] = ty; P[i][2] = tz; ph[i] = fi * 1.9f; }
            else { float k = dt * 3.0f; if (k > 1.0f) k = 1.0f; P[i][0] += (tx - P[i][0]) * k; P[i][1] += (ty - P[i][1]) * k; P[i][2] += (tz - P[i][2]) * k; }
            ph[i] += dt * 11.0f;
        }
        inited = true;
        bool grB = false; int pixB = 160000; (void)grB;
        for (int i = 0; i < n; i++) {
            float pxf, pyf; const float z = proj(P[i][0], P[i][1] + (1.0f - fade) * 0.4f, P[i][2], pxf, pyf); if (z < 0.0f) { prev[i].ok = false; continue; }
            float s = 0.26f * kpx / z * (0.6f + 0.4f * fade); if (s < 3.0f) { prev[i].ok = false; continue; } if (s > 55.0f) s = 55.0f;
            const float fl = __builtin_sinf(ph[i]); const float af = (fl < 0.0f ? -fl : fl) * 0.8f + 0.2f; const float th = 0.3f * __builtin_sinf(ph[i] * 0.16f + static_cast<float>(i));
            int R = static_cast<int>(s * 1.25f) + 4; if (R > 74) R = 74; const int w = 2 * R + 2, h = w;
            for (int y = 0; y < h; y++) { std::memset(accA + y * 192, 0, static_cast<size_t>(w)); std::memset(accH + y * 192, 0, static_cast<size_t>(w)); }
            const float m = static_cast<float>(w) * 0.5f, cs = __builtin_cosf(th), sn = __builtin_sinf(th), us = 1.0f / (s * 0.95f);
            static const float kW[2][6] = { { 0.50f, -0.30f, 0.56f, 0.34f, -0.55f, 0.0f }, { 0.34f, 0.32f, 0.36f, 0.24f, 0.55f, 0.0f } }; /* 羽: 中心 (u,v)・半径 (a,b)・角度 */
            const float wc[2] = { __builtin_cosf(kW[0][4]), __builtin_cosf(kW[1][4]) }, ws[2] = { __builtin_sinf(kW[0][4]), __builtin_sinf(kW[1][4]) };
            for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
                const float dx = static_cast<float>(x) + 0.5f - m, dy = static_cast<float>(y) + 0.5f - m; const float rxp = (dx * cs + dy * sn) * us, ryp = (-dx * sn + dy * cs) * us;
                const float u = (rxp < 0.0f ? -rxp : rxp) / af, v = ryp; int A = 0, Hh = 0;
                for (int k = 0; k < 2; k++) {
                    const float du = u - kW[k][0], dv = v - kW[k][1]; const float pa = (du * wc[k] + dv * ws[k]) / kW[k][2], pb = (-du * ws[k] + dv * wc[k]) / kW[k][3]; const float d2 = pa * pa + pb * pb; if (d2 >= 1.0f) continue;
                    const float e = 1.0f - __builtin_sqrtf(d2); float cov = e * 6.0f; if (cov > 1.0f) cov = 1.0f; float rim = 1.0f - e * 3.2f; if (rim < 0.0f) rim = 0.0f;
                    const float ang = dv + (k ? -0.2f : 0.3f), vein = __builtin_cosf((ang / (u + 0.12f)) * 5.0f); const float vn = vein > 0.7f ? (vein - 0.7f) * 2.0f : 0.0f;
                    int a2 = static_cast<int>(cov * (0.5f + 0.5f * rim + 0.2f * vn) * 255.0f), h2 = static_cast<int>((0.38f + 0.55f * rim + 0.25f * vn) * 255.0f); if (a2 > 255) a2 = 255; if (h2 > 255) h2 = 255; if (a2 > A) A = a2; if (h2 > Hh) Hh = h2;
                }
                const float bd = rxp < 0.0f ? -rxp : rxp; if (bd < 0.035f + 0.5f / s && ryp > -0.42f && ryp < 0.5f) { A = 240; Hh = 20; }
                const float r2 = (dx * dx + dy * dy) * us * us; if (r2 < 1.7f) { const int ga = static_cast<int>((1.0f - r2 * 0.59f) * (1.0f - r2 * 0.59f) * 70.0f); if (ga > A) { A = ga; Hh = 130; } }
                if (A > 0) { accA[y * 192 + x] = static_cast<u8>(A); accH[y * 192 + x] = static_cast<u8>(Hh); }
            }
            const int x0 = static_cast<int>(pxf) - w / 2, y0 = static_cast<int>(pyf) - h / 2;
            /* 前のフレームの四角 (蝶のまわり) が、今の蝶をすっぽり含むときだけ描く。含まないときは、読み出しだけして次に描く */
            const Bx pb = prev[i]; const bool canDraw = pb.ok && x0 >= pb.x && y0 >= pb.y && x0 + w <= pb.x + pb.w && y0 + h <= pb.y + pb.h && x0 >= 0 && y0 >= 0 && x0 + w <= W && y0 + h <= H;
            if (canDraw) {
                const u32* rb = reinterpret_cast<const u32*>(g_PixStorage + rbBase + static_cast<size_t>(i) * slotB); DcInval(rb, static_cast<size_t>(pb.w) * pb.h * 4u);
                const int ia = static_cast<int>(fade * 0.95f * 256.0f);
                short e0[192], e1[192]; for (int ay = 0; ay < h; ay++) { int a0 = -1, a1 = -1; const u8* ra = accA + ay * 192; for (int ax = 0; ax < w; ax++) if (ra[ax] > 2) { if (a0 < 0) a0 = ax; a1 = ax; } e0[ay] = static_cast<short>(a0); e1[ay] = static_cast<short>(a1); }
                const int BH = 8;
                for (int by = 0; by < h; by += BH) {
                    const int bh = by + BH > h ? h - by : BH; int mn = 100000, mx = -1; for (int ay = by; ay < by + bh; ay++) if (e1[ay] >= 0) { if (e0[ay] < mn) mn = e0[ay]; if (e1[ay] > mx) mx = e1[ay]; }
                    if (mx < 0) continue; const int bx0 = mn, bw = mx - mn + 1; const size_t need = static_cast<size_t>(bw) * bh * 4u;
                    if (ncp >= 250 || spUsed + need > kPetSpBytes || pixB <= 0) break; pixB -= bw * bh; u32* o = spb + spUsed / 4;
                    int ov[8], nov = 0; for (int k = ncp - 1; k >= 0 && nov < 8; k--) { const Cp& e = cps[k]; if (e.x < x0 + bx0 + bw && e.x + e.w > x0 + bx0 && e.y < y0 + by + bh && e.y + e.h > y0 + by) ov[nov++] = k; } /* ほかの蝶と重なる所は、その絵を背景にする */
                    for (int y = 0; y < bh; y++) {
                        const u32* ra = rb + static_cast<size_t>(y0 + by + y - pb.y) * pb.w + (x0 + bx0 - pb.x);
                        for (int x = 0; x < bw; x++) {
                            u32 bg = ra[x]; for (int q2 = 0; q2 < nov; q2++) { const Cp& e = cps[ov[q2]]; const int ex2 = x0 + bx0 + x - e.x, ey2 = y0 + by + y - e.y; if (ex2 >= 0 && ex2 < e.w && ey2 >= 0 && ey2 < e.h) { bg = spb[(e.off - spOff) / 4 + static_cast<size_t>(ey2) * e.w + ex2]; break; } } const int A = accA[(by + y) * 192 + bx0 + x], Hh = accH[(by + y) * 192 + bx0 + x]; const int a = A * ia / 255;
                            if (a <= 0) { o[y * bw + x] = bg | 0xFF000000u; continue; }
                            const int br = bg & 255, bgc = (bg >> 8) & 255, bb = (bg >> 16) & 255; const int k = 256 - (a * 90 >> 8);
                            const int r = PetClamp(((br * k) >> 8) + (lr[Hh] * a >> 8)), g = PetClamp(((bgc * k) >> 8) + (lg[Hh] * a >> 8)), b = PetClamp(((bb * k) >> 8) + (lb[Hh] * a >> 8));
                            o[y * bw + x] = 0xFF000000u | (static_cast<u32>(b) << 16) | (static_cast<u32>(g) << 8) | static_cast<u32>(r);
                        }
                    }
                    cps[ncp++] = Cp { x0 + bx0, y0 + by, bw, bh, spOff + spUsed }; spUsed += need;
                }
            }
            /* 次のフレーム用に、蝶のまわりの四角を読む (この絵を写す前の、ゲームの画面) */
            { int bx = x0 - 16, by2 = y0 - 16, bw2 = w + 32, bh2 = h + 32; if (bx < 0) { bw2 += bx; bx = 0; } if (by2 < 0) { bh2 += by2; by2 = 0; } if (bx + bw2 > W) bw2 = W - bx; if (by2 + bh2 > H) bh2 = H - by2;
              if (bw2 > 8 && bh2 > 8 && static_cast<size_t>(bw2) * bh2 * 4u <= slotB) {
                  g_Sw.rowStride(cb, static_cast<ptrdiff_t>(bw2) * 4);
                  for (int r2 = 0; r2 < bh2; r2++) { Reg rg { bx, by2 + r2, 0, bw2, 1, 1 }; g_T2B(cb, tex, nullptr, &rg, g_BufAddr[0] + rbBase + static_cast<size_t>(i) * slotB + static_cast<u64>(r2) * bw2 * 4u, 0); }
                  prev[i] = Bx { bx, by2, bw2, bh2, true };
              } else prev[i].ok = false; }
        }
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
