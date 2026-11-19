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

/* ---- 色 (白と相性のよい、青みの藍 + 若葉 + 薔薇) ---- */
namespace decoc {
    constexpr Col kNavy { 0.07f, 0.07f, 0.08f }, kDeep { 0.19f, 0.19f, 0.20f }, kMid { 0.40f, 0.40f, 0.41f }, kLite { 0.66f, 0.66f, 0.67f }, kPale { 0.85f, 0.85f, 0.86f }, kSnow { 0.96f, 0.96f, 0.97f },
        kAmber { 0.60f, 0.60f, 0.61f }, kAmberD { 0.34f, 0.34f, 0.35f }, kTeal { 0.22f, 0.22f, 0.23f }; /* 鳥は白黒 */
    constexpr Col kLeafD { 0.14f, 0.42f, 0.36f }, kLeafM { 0.24f, 0.55f, 0.44f }, kLeafL { 0.46f, 0.72f, 0.56f }, kLeafP { 0.70f, 0.86f, 0.74f };
    constexpr Col kRoseD { 0.05f, 0.05f, 0.06f }, kRoseM { 0.26f, 0.26f, 0.27f }, kRoseL { 0.62f, 0.62f, 0.63f }, kRoseP { 0.90f, 0.90f, 0.91f }; /* バラも白黒 */
}

/* ---- 葉: 4 つの面に割ったローポリの葉。bx,by = 根もと、ang = 向き (度)、ln = 長さ、wd = 幅 ---- */
inline void DecoLeaf(Cv& v, float bx, float by, float angDeg, float ln, float wd, Col dark, Col light, int seed) {
    const float a = angDeg * 0.017453293f, ca = __builtin_cosf(a), sa = __builtin_sinf(a);
    static const float U[5] = { 0.0f, 0.26f, 0.55f, 0.80f, 1.0f }, W[5] = { 0.0f, 0.78f, 1.0f, 0.62f, 0.0f };
    float mx[5], my[5], lx[5], ly[5], rx[5], ry[5];
    for (int i = 0; i < 5; i++) {
        const float u = U[i] * ln, w = W[i] * wd * 0.5f * (0.92f + 0.16f * DecoRnd(seed * 7 + i));
        mx[i] = bx + u * ca; my[i] = by + u * sa;
        lx[i] = mx[i] + w * sa; ly[i] = my[i] - w * ca; rx[i] = mx[i] - w * sa; ry[i] = my[i] + w * ca;
    }
    for (int i = 0; i < 4; i++) {
        const float s = 0.94f + 0.10f * DecoRnd(seed * 13 + i);
        DecoTri(v, mx[i], my[i], lx[i], ly[i], lx[i + 1], ly[i + 1], DecoShade(i & 1 ? dark : light, s), 1.0f, 0.3f);
        DecoTri(v, mx[i], my[i], lx[i + 1], ly[i + 1], mx[i + 1], my[i + 1], DecoShade(i & 1 ? light : dark, s), 1.0f, 0.3f);
        DecoTri(v, mx[i], my[i], rx[i], ry[i], rx[i + 1], ry[i + 1], DecoShade(i & 1 ? light : dark, s), 1.0f, 0.3f);
        DecoTri(v, mx[i], my[i], rx[i + 1], ry[i + 1], mx[i + 1], my[i + 1], DecoShade(i & 1 ? dark : light, s), 1.0f, 0.3f);
    }
    DecoSeg(v, mx[0], my[0], mx[4], my[4], 0.9f, DecoMix(light, Col { 1.0f, 1.0f, 1.0f }, 0.55f), 0.8f); /* 葉脈 */
}
/* 枝: 根もとから、ゆるく曲がる茎に、左右互い違いに葉を付ける */
inline void DecoSprig(Cv& v, float bx, float by, float angDeg, float len, float leaf, int n, int seed, float bend, bool mono = false) {
    const Col lD = mono ? decoc::kDeep : decoc::kLeafD, lM = mono ? decoc::kMid : decoc::kLeafM, lL = mono ? decoc::kLite : decoc::kLeafL;
    float x = bx, y = by, ang = angDeg; const float seg = len / static_cast<float>(n);
    for (int i = 0; i < n; i++) {
        const float a = ang * 0.017453293f, nx = x + seg * __builtin_cosf(a), ny = y + seg * __builtin_sinf(a);
        DecoSeg(v, x, y, nx, ny, 2.0f, lD);
        const float side = (i & 1) ? 1.0f : -1.0f, sz = leaf * (1.0f - 0.45f * static_cast<float>(i) / static_cast<float>(n)) * (0.9f + 0.2f * DecoRnd(seed + i));
        DecoLeaf(v, nx, ny, ang + side * (38.0f + 14.0f * DecoRnd(seed * 3 + i)), sz, sz * 0.42f, i & 1 ? lD : lM, i & 1 ? lM : lL, seed + i * 5);
        x = nx; y = ny; ang += bend;
    }
    DecoLeaf(v, x, y, ang, leaf * 0.7f, leaf * 0.3f, lM, lL, seed + 99); /* 先の葉 */
}

/* ---- 鳥: 左向きに羽ばたくローポリ。輪郭の多角形を、内側の点でさらに細かい面に割って、面ごとに色を変える ---- */
struct DecoTr { float cx, cy, sc, ca, sa, mx; };
inline void DecoTrApply(const DecoTr& t, float x, float y, float& ox, float& oy) { const float fx = x * t.mx; ox = t.cx + (fx * t.ca - y * t.sa) * t.sc; oy = t.cy + (fx * t.sa + y * t.ca) * t.sc; }
/* 結晶: 輪郭 P (n 点) と中心 (qx,qy)。内側の輪 (中心へ inner 倍) を作り、輪の間と中心を三角に割る。色は (gx,gy) の向きに c0 → c1、面ごとにゆらぎ */
inline void DecoCrystal(Cv& v, const DecoTr& t, const float* P, int n, float qx, float qy, float inner, Col c0, Col c1, float gx, float gy, int seed) {
    float ox[24], oy[24], ix[24], iy[24]; float rmax = 1.0f;
    for (int i = 0; i < n; i++) { DecoTrApply(t, P[i * 2], P[i * 2 + 1], ox[i], oy[i]); const float dx = P[i * 2] - qx, dy = P[i * 2 + 1] - qy; const float r = __builtin_sqrtf(dx * dx + dy * dy); if (r > rmax) rmax = r; }
    for (int i = 0; i < n; i++) { const float k = inner * (0.85f + 0.3f * DecoRnd(seed * 19 + i)); DecoTrApply(t, qx + (P[i * 2] - qx) * k, qy + (P[i * 2 + 1] - qy) * k, ix[i], iy[i]); }
    float cx, cy; DecoTrApply(t, qx, qy, cx, cy);
    auto col = [&](float x, float y, int s2) { /* x,y: 面の中心 (元の座標系) */ float f = 0.5f + ((x - qx) * gx + (y - qy) * gy) / (rmax * 2.0f) + (DecoRnd(seed * 7 + s2) - 0.5f) * 0.42f; f = f < 0.0f ? 0.0f : (f > 1.0f ? 1.0f : f); return DecoMix(c0, c1, f); };
    for (int i = 0; i < n; i++) {
        const int j = (i + 1) % n; const float x0 = P[i * 2], y0 = P[i * 2 + 1], x1 = P[j * 2], y1 = P[j * 2 + 1];
        DecoTri(v, ox[i], oy[i], ox[j], oy[j], ix[i], iy[i], col((x0 * 2 + x1 + qx) / 4.0f, (y0 * 2 + y1 + qy) / 4.0f, i * 3), 1.0f, 0.5f);
        DecoTri(v, ox[j], oy[j], ix[j], iy[j], ix[i], iy[i], col((x1 * 2 + x0 + qx) / 4.0f, (y1 * 2 + y0 + qy) / 4.0f, i * 3 + 1), 1.0f, 0.5f);
        DecoTri(v, cx, cy, ix[i], iy[i], ix[j], iy[j], col((x0 + x1 + qx * 4) / 6.0f, (y0 + y1 + qy * 4) / 6.0f, i * 3 + 2), 1.0f, 0.5f);
    }
}
inline void DecoBird(Cv& v, float cx, float cy, float sc, float rotDeg, bool mirror) {
    using namespace decoc;
    const DecoTr t { cx, cy, sc, __builtin_cosf(rotDeg * 0.017453293f), __builtin_sinf(rotDeg * 0.017453293f), mirror ? -1.0f : 1.0f };
    static const float WF[] = { -26, 12, -50, 40, -80, 74, -58, 72, -68, 98, -42, 86, -38, 108, -18, 84, 6, 88, 10, 56, 34, 20 };      /* 遠い方の羽 (風切り羽がぎざぎざに出る) */
    static const float TL[] = { 48, -8, 104, -14, 146, -4, 118, 14, 142, 48, 100, 34, 54, 16 };                                          /* 尾 (二つに割れる) */
    static const float BD[] = { -92, -16, -80, -32, -58, -30, -24, -33, 20, -23, 56, -8, 58, 14, 18, 27, -24, 26, -56, 14, -74, 4, -90, -3 }; /* 体 + 丸い頭 */
    static const float WU[] = { -30, -22, -40, -58, -16, -96, 30, -128, 62, -114, 46, -96, 82, -94, 60, -76, 90, -68, 64, -58, 82, -40, 56, -38, 30, -20 }; /* 近い方の羽 (風切り羽が 4 本はね出る) */
    DecoCrystal(v, t, WF, 11, -22, 58, 0.50f, kLite, kPale, 0.2f, 1.0f, 5);
    DecoCrystal(v, t, TL, 7, 90, 10, 0.45f, kNavy, kMid, 1.0f, 0.3f, 9);
    DecoCrystal(v, t, BD, 12, -8, -2, 0.50f, kDeep, kSnow, -0.3f, 1.0f, 3);
    DecoCrystal(v, t, WU, 13, 24, -64, 0.52f, kNavy, kMid, 0.6f, 0.8f, 12);
    float ax, ay, bx, by, qx, qy;
    DecoTrApply(t, -112, -10, ax, ay); DecoTrApply(t, -88, -19, bx, by); DecoTrApply(t, -87, -3, qx, qy); DecoTri(v, ax, ay, bx, by, qx, qy, kAmber); /* くちばし */
    DecoTrApply(t, -100, -5, ax, ay); DecoTrApply(t, -86, -4, bx, by); DecoTrApply(t, -90, 3, qx, qy); DecoTri(v, ax, ay, bx, by, qx, qy, kAmberD);
    float ex, ey; DecoTrApply(t, -72, -17, ex, ey); DecoDisc(v, ex, ey, 2.3f * sc + 0.5f, kSnow); DecoDisc(v, ex, ey, 1.1f * sc + 0.2f, kNavy); /* 目 */
}

/* ---- バラ: 上から見たローポリ。中心から 4 つの輪、外の輪はふちがぎざぎざの花びら ---- */
inline void DecoRose(Cv& v, float cx, float cy, float R, float rot, int seed) {
    using namespace decoc;
    constexpr int N = 7; float px[4][N], py[4][N];
    static const float RR[4] = { 0.20f, 0.44f, 0.72f, 1.0f };
    for (int k = 0; k < 4; k++) for (int i = 0; i < N; i++) {
        const float a = rot + (static_cast<float>(i) + 0.5f * static_cast<float>(k & 1) + 0.25f * (DecoRnd(seed * 31 + k * 9 + i) - 0.5f)) * 6.2831853f / static_cast<float>(N);
        float r = RR[k] * R * (k == 3 ? ((i & 1) ? 0.86f : 1.0f) : 1.0f) * (0.94f + 0.12f * DecoRnd(seed * 17 + k * 5 + i));
        px[k][i] = cx + r * __builtin_cosf(a); py[k][i] = cy + r * __builtin_sinf(a);
    }
    auto tone = [&](int k, int i, float lit) { /* k 番目の輪 (中ほど濃く、外へ淡く)。光は左上から */
        const Col base = k == 0 ? kRoseD : (k == 1 ? DecoMix(kRoseD, kRoseM, 0.6f) : (k == 2 ? kRoseM : DecoMix(kRoseL, kRoseP, 0.25f)));
        return DecoShade(DecoMix(base, kRoseP, lit * 0.35f), 0.92f + 0.16f * DecoRnd(seed * 3 + k * 13 + i)); };
    for (int k = 3; k >= 1; k--) for (int i = 0; i < N; i++) { /* 外から内へ */
        const int j = (i + 1) % N; const float lit = (i % 3 == 0) ? 0.9f : (i % 3 == 1 ? 0.35f : 0.0f);
        DecoTri(v, px[k][i], py[k][i], px[k][j], py[k][j], px[k - 1][i], py[k - 1][i], tone(k, i, lit), 1.0f, 0.4f);
        DecoTri(v, px[k][j], py[k][j], px[k - 1][j], py[k - 1][j], px[k - 1][i], py[k - 1][i], tone(k, j, 0.5f * lit), 1.0f, 0.4f);
    }
    for (int i = 0; i < N; i++) { const int j = (i + 1) % N; DecoTri(v, cx, cy, px[0][i], py[0][i], px[0][j], py[0][j], DecoShade(kRoseD, 0.80f + 0.30f * DecoRnd(seed + i))); }
    for (int i = 0; i < N; i += 2) DecoSeg(v, px[1][i], py[1][i], px[1][(i + 1) % N], py[1][(i + 1) % N], 0.9f, kRoseP, 0.55f); /* 花びらのふち */
}
/* つぼみ: 小さな 2 枚の花びら + がく */
inline void DecoBud(Cv& v, float cx, float cy, float s, float angDeg, int seed) {
    using namespace decoc;
    const float a = angDeg * 0.017453293f, ca = __builtin_cosf(a), sa = __builtin_sinf(a);
    auto P = [&](float x, float y, float& ox, float& oy) { ox = cx + (x * ca - y * sa) * s; oy = cy + (x * sa + y * ca) * s; };
    float x0, y0, x1, y1, x2, y2, x3, y3, x4, y4; P(0, 0, x0, y0); P(-10, -8, x1, y1); P(0, -22, x2, y2); P(10, -8, x3, y3); P(0, -3, x4, y4);
    DecoTri(v, x0, y0, x1, y1, x2, y2, kRoseM); DecoTri(v, x0, y0, x2, y2, x3, y3, kRoseD); DecoTri(v, x4, y4, x1, y1, x3, y3, DecoShade(kRoseL, 0.95f + 0.1f * DecoRnd(seed)));
    float g0, h0, g1, h1, g2, h2; P(-9, 4, g0, h0); P(0, -4, g1, h1); P(9, 4, g2, h2);
    DecoTri(v, g0, h0, g1, h1, x0, y0, kMid); DecoTri(v, g2, h2, g1, h1, x0, y0, kDeep);
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
/* メニューの鳥 (右上の角からはみ出す)。角は (kMbCx, kMbCy) */
constexpr int kMbW = 80, kMbH = 70, kMbCx = 34, kMbCy = 4;
inline void DecoMenuBird(Spr& s) {
    Cv v { s.px, s.w, s.h, 0, 0 }; SprClear(s);
    DecoBird(v, static_cast<float>(kMbCx) - 2.0f, static_cast<float>(kMbCy) + 30.0f, 0.25f, -8.0f, false); /* 角 (kMbCx,kMbCy) をまたぐ小さな鳥 */
}
/* メニューのバラ (左下の角)。角 (メニューの左下) は (kMrCx, kMrCy)。縦は板の中の縁に沿って上へ、横は板の下へはみ出して右へ */
constexpr int kMrW = 170, kMrH = 130, kMrCx = 2, kMrCy = 106;
inline void DecoMenuRose(Spr& s) {
    Cv v { s.px, s.w, s.h, 0, 0 }; SprClear(s);
    const float cx = static_cast<float>(kMrCx), cy = static_cast<float>(kMrCy);
    DecoSprig(v, cx + 6.0f, cy - 4.0f, -90.0f, 96.0f, 15.0f, 5, 11, 2.0f, true);  /* 左の縁 (板の中) に沿って上へ */
    DecoSprig(v, cx + 22.0f, cy + 8.0f, -2.0f, 128.0f, 17.0f, 5, 41, -2.0f, true); /* 下の縁に沿って右へ */
    DecoBud(v, cx + 9.0f, cy - 100.0f, 0.8f, 0.0f, 5); DecoBud(v, cx + 152.0f, cy + 8.0f, 0.8f, 88.0f, 6);
    DecoRose(v, cx + 10.0f, cy - 54.0f, 9.0f, 0.5f, 3); DecoRose(v, cx + 76.0f, cy + 9.0f, 10.0f, 1.1f, 4);
    DecoRose(v, cx + 15.0f, cy + 4.0f, 15.0f, 0.2f, 1);
}

/* スプラッシュの飾り。板は (0,0)-(BW,BH) */
constexpr int kSaX = 300, kSaW = 520, kSaH = 200; /* 右側の絵: 板の x = 300 から右 */
inline void DecoSplashArt(Spr& s, int BW) {
    Cv v { s.px, s.w, s.h, kSaX, 0 }; SprClear(s); (void)BW;
    /* 淡いローポリの下地: 左から右へ少しずつ濃く */
    constexpr int gx = 11, gy = 5; float PX[gy + 1][gx + 1], PY[gy + 1][gx + 1];
    const float cw = static_cast<float>(kSaW) / gx, ch = static_cast<float>(kSaH) / gy;
    for (int j = 0; j <= gy; j++) for (int i = 0; i <= gx; i++) {
        const bool edge = i == 0 || j == 0 || i == gx || j == gy;
        PX[j][i] = static_cast<float>(kSaX) + cw * i + (edge ? 0.0f : (DecoRnd(j * 31 + i) - 0.5f) * cw * 0.6f); PY[j][i] = ch * j + (edge ? 0.0f : (DecoRnd(j * 17 + i + 400) - 0.5f) * ch * 0.6f); }
    for (int j = 0; j < gy; j++) for (int i = 0; i < gx; i++) for (int q = 0; q < 2; q++) {
        const float t = (static_cast<float>(i) + 0.5f) / gx; const float k = DecoRnd(j * 53 + i * 7 + q * 3 + 900);
        const Col c = DecoMix(decoc::kPale, decoc::kLite, 0.15f + 0.55f * k * t); const float al = (0.04f + 0.62f * t * t) * (0.45f + 0.55f * k);
        if (q == 0) DecoTri(v, PX[j][i], PY[j][i], PX[j][i + 1], PY[j][i + 1], PX[j + 1][i], PY[j + 1][i], c, al, 0.3f);
        else DecoTri(v, PX[j][i + 1], PY[j][i + 1], PX[j + 1][i + 1], PY[j + 1][i + 1], PX[j + 1][i], PY[j + 1][i], DecoShade(c, 0.985f), al * 0.9f, 0.3f);
    }
    /* 鳥 (大きく) + 散る面 */
    DecoBird(v, 628.0f, 84.0f, 0.74f, -6.0f, false);
    static const float SH[9][6] = { { 732, 30, 752, 24, 742, 42 }, { 764, 66, 782, 62, 772, 78 }, { 716, 80, 730, 76, 722, 90 }, { 700, 22, 710, 18, 706, 30 }, { 770, 112, 782, 108, 776, 120 }, { 744, 138, 758, 136, 750, 148 }, { 780, 30, 790, 28, 785, 38 }, { 690, 150, 702, 146, 696, 158 }, { 730, 112, 740, 108, 734, 120 } };
    for (int i = 0; i < 9; i++) DecoTri(v, SH[i][0], SH[i][1], SH[i][2], SH[i][3], SH[i][4], SH[i][5], i % 3 == 0 ? decoc::kTeal : (i % 3 == 1 ? decoc::kLite : decoc::kMid), 0.95f);
}
/* 右上の角 (板の右上 = (kStrCx, kStrCy)) に、枠の外へ付く枝と葉 */
constexpr int kStrW = 260, kStrH = 170, kStrCx = 190, kStrCy = 60;
inline void DecoSplashTR(Spr& s) {
    Cv v { s.px, s.w, s.h, 0, 0 }; SprClear(s);
    const float cx = static_cast<float>(kStrCx), cy = static_cast<float>(kStrCy);
    DecoSprig(v, cx + 24.0f, cy - 12.0f, 172.0f, 170.0f, 38.0f, 6, 21, -3.5f); /* 角から、枠の上の縁に沿って左へ */
    DecoSprig(v, cx + 10.0f, cy - 6.0f, 98.0f, 120.0f, 32.0f, 5, 61, 4.0f);   /* 右の縁に沿って下へ */
    DecoSprig(v, cx + 4.0f, cy - 20.0f, -38.0f, 70.0f, 30.0f, 3, 81, 5.0f);    /* 外へ伸びる */
    DecoLeaf(v, cx - 6.0f, cy + 4.0f, 128.0f, 50.0f, 22.0f, decoc::kLeafD, decoc::kLeafM, 7); DecoLeaf(v, cx + 2.0f, cy + 12.0f, 60.0f, 44.0f, 19.0f, decoc::kLeafM, decoc::kLeafL, 8);
}
/* 左下の角 (板の左下 = (kSblCx, kSblCy)) */
constexpr int kSblW = 260, kSblH = 170, kSblCx = 60, kSblCy = 110;
inline void DecoSplashBL(Spr& s) {
    Cv v { s.px, s.w, s.h, 0, 0 }; SprClear(s);
    const float cx = static_cast<float>(kSblCx), cy = static_cast<float>(kSblCy);
    DecoSprig(v, cx - 24.0f, cy + 12.0f, -8.0f, 170.0f, 38.0f, 6, 121, 3.5f);  /* 角から、下の縁に沿って右へ */
    DecoSprig(v, cx - 10.0f, cy + 6.0f, -82.0f, 120.0f, 32.0f, 5, 161, -4.0f); /* 左の縁に沿って上へ */
    DecoSprig(v, cx - 4.0f, cy + 20.0f, 142.0f, 70.0f, 30.0f, 3, 181, -5.0f);  /* 外へ伸びる */
    DecoLeaf(v, cx + 6.0f, cy - 4.0f, -52.0f, 50.0f, 22.0f, decoc::kLeafD, decoc::kLeafM, 17); DecoLeaf(v, cx - 2.0f, cy - 12.0f, -120.0f, 44.0f, 19.0f, decoc::kLeafM, decoc::kLeafL, 18);
}
