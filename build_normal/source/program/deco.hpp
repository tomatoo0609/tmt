/* tmt: 飾りの絵 (ローポリの鳥・葉・バラ) と、描画の下回り。main.cpp の無名名前空間の中から include する。
   前提: u32 と Col { r, g, b } が先に定義されていること。色は RGBA8 (R が下位バイト)。
   画素は「ストレート (掛けていない) アルファ」: 0 = 透明。透明な所へ描くと、その濃さがそのままアルファになる。 */

struct Cv { u32* px; int w, h, ox, oy; }; /* ox,oy: 画面上の左上 */
inline u32 Pack(Col k, float a = 1.0f) {
    auto b = [](float v) { int i = static_cast<int>(v * 255.0f + 0.5f); return static_cast<u32>(i < 0 ? 0 : (i > 255 ? 255 : i)); };
    return b(k.r) | (b(k.g) << 8) | (b(k.b) << 16) | (b(a) << 24);
}
inline void Blend(Cv& v, int x, int y, Col k, float a) {
    x -= v.ox; y -= v.oy;
    if (x < 0 || y < 0 || x >= v.w || y >= v.h || a <= 0.0f) return;
    u32& d = v.px[y * v.w + x];
    if (a >= 1.0f) { d = Pack(k); return; }
    const u32 da = d >> 24;
    const float dr = static_cast<float>(d & 255) / 255.0f, dg = static_cast<float>((d >> 8) & 255) / 255.0f, db = static_cast<float>((d >> 16) & 255) / 255.0f;
    if (da >= 255) { d = Pack(Col { dr + (k.r - dr) * a, dg + (k.g - dg) * a, db + (k.b - db) * a }); return; }
    const float fa = static_cast<float>(da) / 255.0f, ra = a + fa * (1.0f - a), w = fa * (1.0f - a) / ra, s = a / ra; /* 透明に近い所へ: 色は掛けずにそのまま混ぜる */
    d = Pack(Col { k.r * s + dr * w, k.g * s + dg * w, k.b * s + db * w }, ra);
}

/* ---- 塗り三角 (2x2 でなめらかに) ---- */
inline void DecoTri(Cv& v, float ax, float ay, float bx, float by, float cx, float cy, Col k, float a = 1.0f, float grow = 0.0f) {
    if (grow > 0.0f) { /* 少しふくらませる (面と面のあいだに、すき間の線が出ないように) */
        const float mx = (ax + bx + cx) / 3.0f, my = (ay + by + cy) / 3.0f;
        auto g = [&](float& x, float& y) { float dx = x - mx, dy = y - my; float L = __builtin_sqrtf(dx * dx + dy * dy); if (L > 0.001f) { x += dx / L * grow; y += dy / L * grow; } };
        g(ax, ay); g(bx, by); g(cx, cy);
    }
    int minx = static_cast<int>((ax < bx ? (ax < cx ? ax : cx) : (bx < cx ? bx : cx)) - 1.0f), maxx = static_cast<int>((ax > bx ? (ax > cx ? ax : cx) : (bx > cx ? bx : cx)) + 2.0f);
    int miny = static_cast<int>((ay < by ? (ay < cy ? ay : cy) : (by < cy ? by : cy)) - 1.0f), maxy = static_cast<int>((ay > by ? (ay > cy ? ay : cy) : (by > cy ? by : cy)) + 2.0f);
    if (minx < v.ox) minx = v.ox; if (miny < v.oy) miny = v.oy; if (maxx > v.ox + v.w) maxx = v.ox + v.w; if (maxy > v.oy + v.h) maxy = v.oy + v.h;
    const float area = (bx - ax) * (cy - ay) - (cx - ax) * (by - ay); if (area > -0.001f && area < 0.001f) return;
    const bool pos = area > 0.0f;
    for (int y = miny; y < maxy; y++) for (int x = minx; x < maxx; x++) {
        int cov = 0;
        for (int sy = 0; sy < 2; sy++) for (int sx = 0; sx < 2; sx++) {
            const float px = static_cast<float>(x) + (sx ? 0.75f : 0.25f), py = static_cast<float>(y) + (sy ? 0.75f : 0.25f);
            const float e0 = (bx - ax) * (py - ay) - (by - ay) * (px - ax), e1 = (cx - bx) * (py - by) - (cy - by) * (px - bx), e2 = (ax - cx) * (py - cy) - (ay - cy) * (px - cx);
            if (pos ? (e0 >= 0 && e1 >= 0 && e2 >= 0) : (e0 <= 0 && e1 <= 0 && e2 <= 0)) cov++;
        }
        if (cov) Blend(v, x, y, k, a * static_cast<float>(cov) * 0.25f);
    }
}
/* 太さのある線 (四角 = 三角 2 つ) */
inline void DecoSeg(Cv& v, float x0, float y0, float x1, float y1, float t, Col k, float a = 1.0f) {
    const float dx = x1 - x0, dy = y1 - y0; float L = __builtin_sqrtf(dx * dx + dy * dy); if (L < 0.01f) L = 0.01f;
    const float nx = -dy / L * t * 0.5f, ny = dx / L * t * 0.5f;
    DecoTri(v, x0 + nx, y0 + ny, x0 - nx, y0 - ny, x1 - nx, y1 - ny, k, a);
    DecoTri(v, x0 + nx, y0 + ny, x1 - nx, y1 - ny, x1 + nx, y1 + ny, k, a);
}
inline void DecoDisc(Cv& v, float cx, float cy, float r, Col k, float a = 1.0f) {
    for (int y = static_cast<int>(cy - r - 1.0f); y <= static_cast<int>(cy + r + 1.0f); y++) for (int x = static_cast<int>(cx - r - 1.0f); x <= static_cast<int>(cx + r + 1.0f); x++) {
        const float dx = static_cast<float>(x) + 0.5f - cx, dy = static_cast<float>(y) + 0.5f - cy, d = __builtin_sqrtf(dx * dx + dy * dy); float al = r + 0.5f - d; if (al > 1.0f) al = 1.0f; if (al > 0.0f) Blend(v, x, y, k, al * a); }
}
inline float DecoRnd(int i) { u32 h = static_cast<u32>(i) * 2654435761u + 0x9E3779B9u; h ^= h >> 15; h *= 2246822519u; h ^= h >> 13; h *= 3266489917u; h ^= h >> 16; return static_cast<float>(h & 0xFFFFu) / 65535.0f; }
inline Col DecoShade(Col c, float m) { auto q = [](float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }; return Col { q(c.r * m), q(c.g * m), q(c.b * m) }; }
inline Col DecoMix(Col a, Col b, float t) { return Col { a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t }; }

/* ---- 色: 黒と白だけ (灰色は使わない) ---- */
namespace decoc { constexpr Col kK { 0.0f, 0.0f, 0.0f }, kW { 1.0f, 1.0f, 1.0f }; }

/* 面: 白か黒で塗って、黒い細い線でふちどる (白い板の上でも形が見える) */
inline void DecoFacet(Cv& v, float ax, float ay, float bx, float by, float cx, float cy, bool black, float lw = 1.3f) {
    using namespace decoc;
    DecoTri(v, ax, ay, bx, by, cx, cy, black ? kK : kW, 1.0f, 0.3f);
    DecoSeg(v, ax, ay, bx, by, lw, kK); DecoSeg(v, bx, by, cx, cy, lw, kK); DecoSeg(v, cx, cy, ax, ay, lw, kK);
}

/* ---- 葉: 4 つの面だけのひし形のローポリ。面の白黒はたねで変わるので、見る人によって読み方が変わる。bx,by = 根もと ---- */
inline void DecoLeaf(Cv& v, float bx, float by, float angDeg, float ln, float wd, int seed) {
    const float a = angDeg * 0.017453293f, ca = __builtin_cosf(a), sa = __builtin_sinf(a);
    const float u1 = ln * (0.40f + 0.18f * DecoRnd(seed * 5 + 1)), w1 = wd * 0.5f * (0.85f + 0.3f * DecoRnd(seed * 5 + 2)), w2 = wd * 0.5f * (0.85f + 0.3f * DecoRnd(seed * 5 + 3)), ofs = (DecoRnd(seed * 5 + 4) - 0.5f) * ln * 0.12f;
    auto P = [&](float u, float w, float& ox, float& oy) { ox = bx + u * ca - w * sa; oy = by + u * sa + w * ca; };
    float x0, y0, xl, yl, xr, yr, xm, ym, xt, yt; P(0, 0, x0, y0); P(u1, w1, xl, yl); P(u1 + ofs, -w2, xr, yr); P(u1 * 0.8f, 0, xm, ym); P(ln, ofs * 0.6f, xt, yt);
    const int pat = static_cast<int>(DecoRnd(seed * 3 + 9) * 6.0f); /* 白黒の割り振り 6 通り (全部白・全部黒にはならない) */
    static const unsigned char K[6] = { 0x9, 0x6, 0x3, 0xC, 0x5, 0xA }; /* 白黒 2 面ずつ (ローポリ) */ /* 黒が多め (白が大きなかたまりにならないように) */ const unsigned m = K[pat];
    DecoFacet(v, x0, y0, xl, yl, xm, ym, (m & 1) != 0); DecoFacet(v, xm, ym, xl, yl, xt, yt, (m & 2) != 0);
    DecoFacet(v, x0, y0, xr, yr, xm, ym, (m & 4) != 0); DecoFacet(v, xm, ym, xr, yr, xt, yt, (m & 8) != 0);
}
/* 起動のときの組み立て: 0〜1 の進み具合 prog のうち、delay から span のあいだに、部品が枠の外から寄ってくる (0 = 遠い、1 = 定位置) */
inline float DecoEase(float prog, float delay, float span = 0.40f) { float t = (prog - delay) / span; if (t < 0.0f) t = 0.0f; if (t > 1.0f) t = 1.0f; const float q = 1.0f - t; return 1.0f - q * q * q; }
/* 枝: 根もとから、ゆるく曲がる茎に、左右互い違いに葉を付ける */
inline void DecoSprig(Cv& v, float bx, float by, float angDeg, float len, float leaf, int n, int seed, float bend, float prog = 1.0f, float odx = 0.0f, float ody = 0.0f) {
    float x = bx, y = by, ang = angDeg; const float seg = len / static_cast<float>(n);
    for (int i = 0; i < n; i++) {
        const float a = ang * 0.017453293f, nx = x + seg * __builtin_cosf(a), ny = y + seg * __builtin_sinf(a);
        Cv w = v; { const float e = DecoEase(prog, 0.05f + 0.07f * static_cast<float>(i)), sh = (1.0f - e) * 110.0f; w.ox -= static_cast<int>(odx * sh); w.oy -= static_cast<int>(ody * sh); if (prog < 1.0f && e <= 0.01f) { x = nx; y = ny; ang += bend; continue; } } /* i 番目の茎と葉は、枠の外から寄ってくる */
        DecoSeg(w, x, y, nx, ny, 1.8f, decoc::kK);
        const float side = (i & 1) ? 1.0f : -1.0f, sz = leaf * (1.0f - 0.40f * static_cast<float>(i) / static_cast<float>(n)) * (0.9f + 0.2f * DecoRnd(seed + i));
        DecoLeaf(w, nx, ny, ang + side * (36.0f + 16.0f * DecoRnd(seed * 3 + i)), sz, sz * 0.46f, seed + i * 5);
        x = nx; y = ny; ang += bend;
    }
    { Cv w = v; const float e = DecoEase(prog, 0.05f + 0.07f * static_cast<float>(n)), sh = (1.0f - e) * 110.0f; w.ox -= static_cast<int>(odx * sh); w.oy -= static_cast<int>(ody * sh); if (prog >= 1.0f || e > 0.01f) DecoLeaf(w, x, y, ang, leaf * 0.7f, leaf * 0.32f, seed + 99); }
}

/* ---- 鳥: ほんの十数枚の三角だけ。鳥だと分かる人にも、折り紙や破片に見える人にも読めるように ---- */
struct DecoTr { float cx, cy, sc, ca, sa, mx; };
inline void DecoTrApply(const DecoTr& t, float x, float y, float& ox, float& oy) { const float fx = x * t.mx; ox = t.cx + (fx * t.ca - y * t.sa) * t.sc; oy = t.cy + (fx * t.sa + y * t.ca) * t.sc; }
inline void DecoBird(Cv& v, float cx, float cy, float sc, float rotDeg, bool mirror, float prog = 1.0f) {
    const DecoTr t { cx, cy, sc, __builtin_cosf(rotDeg * 0.017453293f), __builtin_sinf(rotDeg * 0.017453293f), mirror ? -1.0f : 1.0f };
    static const short F[][7] = { /* 三角 3 点 + 黒(1)/白(0) */
        { -26, 14, -62, 70, 14, 52, 0 },                                   /* 遠い方の羽 */
        { 30, 0, 136, -8, 84, 30, 1 }, { 84, 30, 136, -8, 118, 44, 0 },    /* 尾 */
        { -104, -10, -70, -32, -60, -2, 1 },                               /* 頭とくちばし */
        { -70, -32, -60, -2, -8, -28, 0 }, { -60, -2, -8, -28, 6, 22, 1 }, { -8, -28, 6, 22, 52, -4, 0 }, /* 体 */
        { -16, -26, 24, -128, 62, -36, 1 }, { 24, -128, 62, -36, 90, -88, 0 }, /* 近い方の羽 */
    };
    int fi = 0;
    for (const auto& f : F) { float ax, ay, bx, by, qx, qy; DecoTrApply(t, f[0], f[1], ax, ay); DecoTrApply(t, f[2], f[3], bx, by); DecoTrApply(t, f[4], f[5], qx, qy);
        if (prog < 1.0f) { const float e = DecoEase(prog, 0.22f + 0.045f * static_cast<float>(fi)); fi++; if (e <= 0.01f) continue; const float mx = (ax + bx + qx) / 3.0f - cx, my = (ay + by + qy) / 3.0f - cy; float L = __builtin_sqrtf(mx * mx + my * my); if (L < 1.0f) L = 1.0f; const float sh = (1.0f - e) * 170.0f, sx = mx / L * sh, sy = my / L * sh; ax += sx; bx += sx; qx += sx; ay += sy; by += sy; qy += sy; }
        DecoFacet(v, ax, ay, bx, by, qx, qy, f[6] != 0, 1.2f + 0.8f * t.sc); }
}

/* ---- 小さな絵 (スプライト): 一度だけ描いて、あとは貼るだけ ---- */
struct Spr { u32* px; int w, h; };
inline void SprClear(Spr& s) { for (int i = 0; i < s.w * s.h; i++) s.px[i] = 0x00FFFFFFu; }
/* 貼る: アルファで重ねる。clip があれば、角の丸い四角 (cx,cy,cw,ch,cr) の外は貼らない */
inline void SprBlit(Cv& v, const Spr& s, int x0, int y0, const int* clip = nullptr) {
    for (int y = 0; y < s.h; y++) for (int x = 0; x < s.w; x++) {
        const u32 m = s.px[y * s.w + x]; const u32 a = m >> 24; if (!a) continue;
        const int X = x0 + x, Y = y0 + y;
        if (clip) {
            const int qx = X - clip[0], qy = Y - clip[1]; if (qx < 0 || qy < 0 || qx >= clip[2] || qy >= clip[3]) continue;
            const int r = clip[4]; const int ix = qx < r ? r - qx : (qx >= clip[2] - r ? qx - (clip[2] - r - 1) : 0), iy = qy < r ? r - qy : (qy >= clip[3] - r ? qy - (clip[3] - r - 1) : 0);
            if (ix * ix + iy * iy > r * r) continue;
        }
        Blend(v, X, Y, Col { static_cast<float>(m & 255) / 255.0f, static_cast<float>((m >> 8) & 255) / 255.0f, static_cast<float>((m >> 16) & 255) / 255.0f }, static_cast<float>(a) / 255.0f);
    }
}
/* 透けた所を探して、横に連なった塊 (実体のある画素) を渡す。band 行ずつまとめる。画面へ写すときの「切り抜き」用 */
template <class Emit> inline void SprRuns(const u32* img, int w, int h, int bx0, int by0, int bx1, int by1, int band, Emit emit) {
    if (bx0 < 0) bx0 = 0; if (by0 < 0) by0 = 0; if (bx1 > w) bx1 = w; if (by1 > h) by1 = h;
    for (int y = by0; y < by1; y += band) {
        const int yb = (y + band > by1) ? by1 - y : band; int rs = -1;
        for (int x = bx0; x <= bx1; x++) {
            bool on = false;
            if (x < bx1) for (int q = 0; q < yb && !on; q++) on = (img[(y + q) * w + x] >> 24) >= 72u;
            if (on) { if (rs < 0) rs = x; } else if (rs >= 0) { emit(rs, y, x - rs, yb); rs = -1; }
        }
    }
}

/* ======== 組み立て (スプライトに描く。座標は各スプライトの左上が原点) ======== */
/* メニューの鳥 (右上の角をまたぐ)。角は (kMbCx, kMbCy) */
constexpr int kMbW = 80, kMbH = 70, kMbCx = 34, kMbCy = 4;
inline void DecoMenuBird(Spr& s) {
    Cv v { s.px, s.w, s.h, 0, 0 }; SprClear(s);
    DecoBird(v, static_cast<float>(kMbCx) - 2.0f, static_cast<float>(kMbCy) + 30.0f, 0.25f, -8.0f, false);
}

/* スプラッシュの飾り。板は (0,0)-(BW,BH) */
constexpr int kSaX = 270, kSaW = 550, kSaH = 160; /* 右側の絵: 板の x = 300 から右 */
inline void DecoSplashArt(Spr& s, int BW, float prog = 1.0f) {
    using namespace decoc;
    Cv v { s.px, s.w, s.h, kSaX, 0 }; SprClear(s); (void)BW;
    /* 細い線のローポリの下地 (右へ行くほど密) */
    constexpr int gx = 11, gy = 5; float PX[gy + 1][gx + 1], PY[gy + 1][gx + 1];
    const float cw = static_cast<float>(kSaW) / gx, ch = static_cast<float>(kSaH) / gy;
    for (int j = 0; j <= gy; j++) for (int i = 0; i <= gx; i++) {
        const bool edge = i == 0 || j == 0 || i == gx || j == gy;
        PX[j][i] = static_cast<float>(kSaX) + cw * i + (edge ? 0.0f : (DecoRnd(j * 31 + i) - 0.5f) * cw * 0.6f); PY[j][i] = ch * j + (edge ? 0.0f : (DecoRnd(j * 17 + i + 400) - 0.5f) * ch * 0.6f); }
    for (int j = 0; j < gy; j++) for (int i = 0; i < gx; i++) {
        /* 「tmt Ver.…」の右あたりで見えなくなるまで、左へ向かって少しずつ薄く (鳥のうしろでも、濃くなりすぎない) */
        const float xm = static_cast<float>(kSaX) + cw * (static_cast<float>(i) + 0.5f); float f = (xm - 285.0f) / (800.0f - 285.0f); f = f < 0.0f ? 0.0f : (f > 1.0f ? 1.0f : f);
        float al = 0.50f * f * f * (3.0f - 2.0f * f) * 0.9f + 0.04f * f; if (al < 0.01f) continue;
        Cv v0 = v; if (prog < 1.0f) { /* 起動: この区画の線は、枠の外から寄ってきて、濃くなる */
            const float e = DecoEase(prog, 0.50f * DecoRnd(j * 13 + i * 5 + 300), 0.50f); if (e <= 0.01f) continue; al *= e;
            const float dx = xm - (static_cast<float>(kSaX) + 0.55f * kSaW), dy = ch * (static_cast<float>(j) + 0.5f) - 0.5f * kSaH; float L = __builtin_sqrtf(dx * dx + dy * dy); if (L < 1.0f) L = 1.0f; const float sh = (1.0f - e) * 150.0f; v.ox -= static_cast<int>(dx / L * sh); v.oy -= static_cast<int>(dy / L * sh); }
        if (DecoRnd(j * 53 + i * 7 + 900) < 0.7f) DecoSeg(v, PX[j][i], PY[j][i], PX[j][i + 1], PY[j][i + 1], 0.9f, kK, al);
        if (DecoRnd(j * 29 + i * 11 + 950) < 0.7f) DecoSeg(v, PX[j][i], PY[j][i], PX[j + 1][i], PY[j + 1][i], 0.9f, kK, al);
        if (DecoRnd(j * 41 + i * 3 + 990) < 0.7f) DecoSeg(v, PX[j][i + 1], PY[j][i + 1], PX[j + 1][i], PY[j + 1][i], 0.9f, kK, al);
        if (f > 0.5f && DecoRnd(j * 19 + i * 23 + 1100) > 0.86f) DecoTri(v, PX[j][i], PY[j][i], PX[j][i + 1], PY[j][i + 1], PX[j + 1][i], PY[j + 1][i], kK, al * 0.55f, 0.3f); /* ところどころ薄く塗る */
        v = v0;
    }
    DecoBird(v, 650.0f, 54.0f, 0.38f, -6.0f, false, prog); /* バーより上に収まる大きさ */
    static const float SH[7][6] = { { 732, 30, 752, 24, 742, 42 }, { 764, 66, 782, 62, 772, 78 }, { 716, 80, 730, 76, 722, 90 }, { 770, 112, 782, 108, 776, 120 }, { 744, 138, 758, 136, 750, 148 }, { 690, 150, 702, 146, 696, 158 }, { 700, 22, 710, 18, 706, 30 } };
    for (int i = 0; i < 7; i++) { Cv w = v; if (prog < 1.0f) { const float e = DecoEase(prog, 0.35f + 0.06f * static_cast<float>(i)); if (e <= 0.01f) continue; const float sh = (1.0f - e) * 150.0f; w.ox -= static_cast<int>(sh * ((i & 1) ? 1.0f : 0.6f)); w.oy -= static_cast<int>(sh * ((i & 2) ? 0.8f : -0.8f)); }
        DecoFacet(w, SH[i][0], SH[i][1] * 0.42f + 4.0f, SH[i][2], SH[i][3] * 0.42f + 4.0f, SH[i][4], SH[i][5] * 0.42f + 4.0f, (i & 1) == 0, 1.0f); }
}
/* 右上の角 (板の右上 = (kStrCx, kStrCy)) に、枠の外へ付く枝と葉 */
constexpr int kStrW = 260, kStrH = 170, kStrCx = 190, kStrCy = 60;
inline void DecoSplashTR(Spr& s, float prog = 1.0f) {
    Cv v { s.px, s.w, s.h, 0, 0 }; SprClear(s);
    const float cx = static_cast<float>(kStrCx), cy = static_cast<float>(kStrCy);
    DecoSprig(v, cx + 24.0f, cy - 12.0f, 172.0f, 140.0f, 34.0f, 4, 21, -3.5f, prog, 0.7f, -0.7f);
    DecoSprig(v, cx + 10.0f, cy - 6.0f, 98.0f, 90.0f, 28.0f, 3, 61, 4.0f, prog, 0.7f, -0.7f);
}
/* 左下の角 (板の左下 = (kSblCx, kSblCy)) */
constexpr int kSblW = 260, kSblH = 170, kSblCx = 60, kSblCy = 110;
inline void DecoSplashBL(Spr& s, float prog = 1.0f) {
    Cv v { s.px, s.w, s.h, 0, 0 }; SprClear(s);
    const float cx = static_cast<float>(kSblCx), cy = static_cast<float>(kSblCy);
    DecoSprig(v, cx - 24.0f, cy + 12.0f, -8.0f, 140.0f, 34.0f, 4, 121, 3.5f, prog, -0.7f, 0.7f);
    DecoSprig(v, cx - 10.0f, cy + 6.0f, -82.0f, 90.0f, 28.0f, 3, 161, -4.0f, prog, -0.7f, 0.7f);
}
