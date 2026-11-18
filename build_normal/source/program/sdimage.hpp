#pragma once
/*
 * sdimage: PNG / BMP を読み込んで、塗る/塗らない の白黒ビットマップ (最大 kMaxDim x kMaxDim) にする。
 *
 *   - ファイルの読み込みは Source (読み取り関数) 経由なので、ゲーム内でも PC でも同じコードで動く。
 *   - 画像は最後まで RAM に載せず、1行ずつ縮小しながら読む (大きい写真でもメモリは増えない)。
 *   - 対応: PNG (インターレースなし。グレー/RGB/パレット/グレー+透明/RGBA、1〜16bit)、
 *           BMP (非圧縮の 8/24/32bit)。
 *   - 写真のように中間の明るさが多い絵は、ディザ (誤差拡散) で濃淡を点の密度にする。
 *     線画のようにはっきりした絵は、しきい値で白黒に分ける。
 */
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace sdimage {

    constexpr int kMaxDim = 128;       /* 縮小後の長辺 (456: 64 → 128。細かい絵ほど、大きさ・インクの大きさを調整して使う) */
    constexpr int kMaxSrcDim = 8192;   /* これより大きい画像は読まない */

    enum Status {
        Ok = 0,
        BadFormat = 1,  /* PNG/BMP ではない、または未対応の種類 */
        TooBig = 2,     /* 大きすぎる */
        BadData = 3,    /* ファイルが壊れている / 途中で終わっている */
    };

    struct Source {
        /* off から n バイト読む。ちょうど n バイト読めたら true。 */
        bool (*read)(void* ctx, uint64_t off, void* buf, uint32_t n);
        void* ctx;
        uint64_t size;
    };

    struct Bitmap {
        int w = 0;
        int h = 0;
        uint8_t bits[(kMaxDim * kMaxDim + 7) / 8] = {};
        bool On(int i) const { return (bits[i >> 3] >> (7 - (i & 7))) & 1; }
        void Set(int i, bool v) {
            if (v) bits[i >> 3] |= static_cast<uint8_t>(0x80 >> (i & 7));
            else   bits[i >> 3] &= static_cast<uint8_t>(~(0x80 >> (i & 7)));
        }
    };

    /* 色つきで受け取りたいとき用 (ジャンプスケア): 1 ピクセルごとに呼ばれる */
    using PxInitFn = void (*)(int w, int h);
    using PxSinkFn = void (*)(int x, int y, int r, int g, int b, int a);
    inline PxInitFn g_PxInit = nullptr;
    inline PxSinkFn g_PxSink = nullptr;

    namespace detail {

        /* ---- バッファ付きの順次読み込み -------------------------------------- */
        class Reader {
        public:
            void Init(const Source* s, uint64_t pos) { src_ = s; pos_ = pos; have_ = idx_ = 0; }
            int Get() {
                if (idx_ == have_ && !Fill())
                    return -1;
                return buf_[idx_++];
            }
            uint64_t Tell() const { return pos_ - (have_ - idx_); }
            void Seek(uint64_t p) { pos_ = p; have_ = idx_ = 0; }
            bool Read(void* out, uint32_t n) {
                uint8_t* o = static_cast<uint8_t*>(out);
                while (n--) {
                    int c = Get();
                    if (c < 0) return false;
                    *o++ = static_cast<uint8_t>(c);
                }
                return true;
            }
            bool U32BE(uint32_t& v) {
                uint8_t b[4];
                if (!Read(b, 4)) return false;
                v = (uint32_t(b[0]) << 24) | (uint32_t(b[1]) << 16) | (uint32_t(b[2]) << 8) | b[3];
                return true;
            }
        private:
            bool Fill() {
                const uint64_t remain = src_->size > pos_ ? src_->size - pos_ : 0;
                if (remain == 0) return false;
                const uint32_t n = remain < sizeof(buf_) ? static_cast<uint32_t>(remain) : static_cast<uint32_t>(sizeof(buf_));
                if (!src_->read(src_->ctx, pos_, buf_, n)) return false;
                pos_ += n;
                have_ = n;
                idx_ = 0;
                return true;
            }
            const Source* src_ = nullptr;
            uint64_t pos_ = 0;
            uint8_t buf_[4096];
            uint32_t have_ = 0, idx_ = 0;
        };

        /* ---- 縮小用の集計 ------------------------------------------------------ */
        struct Accum {
            int sw = 0, sh = 0, tw = 0, th = 0;
            uint32_t luma[kMaxDim * kMaxDim];
            uint32_t alpha[kMaxDim * kMaxDim];
            uint32_t cnt[kMaxDim * kMaxDim];
            uint8_t txMap[kMaxSrcDim];

            void Init(int w, int h) {
                sw = w; sh = h;
                if (g_PxInit) g_PxInit(w, h);
                const int m = w > h ? w : h;
                if (m <= kMaxDim) { tw = w; th = h; }
                else {
                    tw = (w * kMaxDim + m / 2) / m; if (tw < 1) tw = 1;
                    th = (h * kMaxDim + m / 2) / m; if (th < 1) th = 1;
                }
                std::memset(luma, 0, sizeof(luma));
                std::memset(alpha, 0, sizeof(alpha));
                std::memset(cnt, 0, sizeof(cnt));
                for (int x = 0; x < w; x++)
                    txMap[x] = static_cast<uint8_t>((static_cast<uint64_t>(x) * tw) / w);
            }
            /* y 行目の (x, 明るさ 0-255, 不透明度 0-255) を足す */
            void Add(int x, int y, int lum, int a) {
                const int ty = static_cast<int>((static_cast<uint64_t>(y) * th) / sh);
                const int i = ty * tw + txMap[x];
                luma[i] += static_cast<uint32_t>(lum);
                alpha[i] += static_cast<uint32_t>(a);
                cnt[i]++;
            }
        };

        /* 集計から 塗る/塗らない のビットマップを作る */
        inline void Finish(Accum& a, bool invert, Bitmap& out) {
            const int n = a.tw * a.th;
            static int lum[kMaxDim * kMaxDim];
            static bool opaque[kMaxDim * kMaxDim];
            int mn = 255, mx = 0, mid = 0, nopaque = 0;
            for (int i = 0; i < n; i++) {
                const uint32_t c = a.cnt[i] ? a.cnt[i] : 1;
                lum[i] = static_cast<int>(a.luma[i] / c);
                opaque[i] = (a.alpha[i] / c) >= 128;
                if (!opaque[i]) continue;
                nopaque++;
                if (lum[i] < mn) mn = lum[i];
                if (lum[i] > mx) mx = lum[i];
                if (lum[i] >= 48 && lum[i] <= 208) mid++;
            }
            out.w = a.tw;
            out.h = a.th;
            std::memset(out.bits, 0, sizeof(out.bits));
            if (nopaque == 0) return;

            const bool photo = mid * 100 > nopaque * 30;
            if (!photo) {
                for (int i = 0; i < n; i++) {
                    if (!opaque[i]) continue;
                    const bool dark = lum[i] < 128;
                    out.Set(i, dark != invert);
                }
                return;
            }

            /* 写真: 明るさを 0-255 いっぱいに引き伸ばして、Floyd-Steinberg で点の密度にする */
            static int v[kMaxDim * kMaxDim];
            const int span = (mx - mn) > 0 ? (mx - mn) : 1;
            for (int i = 0; i < n; i++) {
                int t = ((lum[i] - mn) * 255) / span;
                if (t < 0) t = 0;
                if (t > 255) t = 255;
                v[i] = invert ? 255 - t : t;
            }
            for (int y = 0; y < a.th; y++) {
                for (int x = 0; x < a.tw; x++) {
                    const int i = y * a.tw + x;
                    const int old = v[i];
                    const bool ink = old < 128;
                    const int err = old - (ink ? 0 : 255);
                    if (opaque[i]) out.Set(i, ink);
                    if (x + 1 < a.tw)                    v[i + 1]          += err * 7 / 16;
                    if (y + 1 < a.th) {
                        if (x > 0)                       v[i + a.tw - 1]   += err * 3 / 16;
                                                         v[i + a.tw]       += err * 5 / 16;
                        if (x + 1 < a.tw)                v[i + a.tw + 1]   += err * 1 / 16;
                    }
                }
            }
        }

        /* ---- inflate (zlib の展開)。puff.c と同じ考え方の小さな実装 ------------ */
        struct Huffman {
            int16_t count[16];
            int16_t symbol[288];
        };

        struct Inflate {
            int (*getb)(void*) = nullptr;
            void (*putb)(void*, uint8_t) = nullptr;
            void* ctx = nullptr;
            uint32_t bitbuf = 0;
            int bitcnt = 0;
            uint32_t wpos = 0;
            bool err = false;
            uint8_t window[32768];

            int Bits(int need) {
                uint32_t val = bitbuf;
                while (bitcnt < need) {
                    const int b = getb(ctx);
                    if (b < 0) { err = true; return 0; }
                    val |= static_cast<uint32_t>(b) << bitcnt;
                    bitcnt += 8;
                }
                bitbuf = val >> need;
                bitcnt -= need;
                return static_cast<int>(val & ((1u << need) - 1));
            }

            void Out(uint8_t b) {
                window[wpos & 32767] = b;
                wpos++;
                putb(ctx, b);
            }

            static int Construct(Huffman* h, const int16_t* length, int n) {
                for (int len = 0; len <= 15; len++) h->count[len] = 0;
                for (int s = 0; s < n; s++) h->count[length[s]]++;
                if (h->count[0] == n) return 0;
                int left = 1;
                for (int len = 1; len <= 15; len++) {
                    left <<= 1;
                    left -= h->count[len];
                    if (left < 0) return left;
                }
                int16_t offs[16];
                offs[1] = 0;
                for (int len = 1; len < 15; len++) offs[len + 1] = static_cast<int16_t>(offs[len] + h->count[len]);
                for (int s = 0; s < n; s++)
                    if (length[s] != 0) h->symbol[offs[length[s]]++] = static_cast<int16_t>(s);
                return left;
            }

            int Decode(const Huffman* h) {
                int code = 0, first = 0, index = 0;
                for (int len = 1; len <= 15; len++) {
                    code |= Bits(1);
                    if (err) return -1;
                    const int count = h->count[len];
                    if (code - count < first) return h->symbol[index + (code - first)];
                    index += count;
                    first += count;
                    first <<= 1;
                    code <<= 1;
                }
                return -10;
            }

            int Codes(const Huffman* lencode, const Huffman* distcode) {
                static const int16_t lens[29] = {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
                static const int16_t lext[29] = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
                static const int16_t dists[30] = {1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
                static const int16_t dext[30] = {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};
                int symbol;
                do {
                    symbol = Decode(lencode);
                    if (symbol < 0) return symbol;
                    if (symbol < 256) {
                        Out(static_cast<uint8_t>(symbol));
                    } else if (symbol > 256) {
                        symbol -= 257;
                        if (symbol >= 29) return -10;
                        int len = lens[symbol] + Bits(lext[symbol]);
                        symbol = Decode(distcode);
                        if (symbol < 0) return symbol;
                        const uint32_t dist = static_cast<uint32_t>(dists[symbol] + Bits(dext[symbol]));
                        if (err) return -1;
                        if (dist > wpos || dist > 32768) return -11;
                        while (len--) Out(window[(wpos - dist) & 32767]);
                    }
                } while (symbol != 256);
                return 0;
            }

            int Stored() {
                bitbuf = 0;
                bitcnt = 0;
                const int a = getb(ctx), b = getb(ctx), c = getb(ctx), d = getb(ctx);
                if (a < 0 || b < 0 || c < 0 || d < 0) return -1;
                const unsigned len = static_cast<unsigned>(a | (b << 8));
                const unsigned nlen = static_cast<unsigned>(c | (d << 8));
                if (len != (~nlen & 0xFFFF)) return -2;
                for (unsigned i = 0; i < len; i++) {
                    const int x = getb(ctx);
                    if (x < 0) return -1;
                    Out(static_cast<uint8_t>(x));
                }
                return 0;
            }

            int Fixed() {
                Huffman lencode, distcode;
                int16_t lengths[288];
                int s = 0;
                for (; s < 144; s++) lengths[s] = 8;
                for (; s < 256; s++) lengths[s] = 9;
                for (; s < 280; s++) lengths[s] = 7;
                for (; s < 288; s++) lengths[s] = 8;
                Construct(&lencode, lengths, 288);
                for (s = 0; s < 30; s++) lengths[s] = 5;
                Construct(&distcode, lengths, 30);
                return Codes(&lencode, &distcode);
            }

            int Dynamic() {
                static const int16_t order[19] = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
                int16_t lengths[320];
                Huffman lencode, distcode;
                const int nlen = Bits(5) + 257;
                const int ndist = Bits(5) + 1;
                const int ncode = Bits(4) + 4;
                if (err) return -1;
                if (nlen > 286 || ndist > 30) return -3;
                int index;
                for (index = 0; index < ncode; index++) lengths[order[index]] = static_cast<int16_t>(Bits(3));
                for (; index < 19; index++) lengths[order[index]] = 0;
                if (err) return -1;
                if (Construct(&lencode, lengths, 19) != 0) return -4;
                index = 0;
                while (index < nlen + ndist) {
                    int symbol = Decode(&lencode);
                    if (symbol < 0) return symbol;
                    if (symbol < 16) {
                        lengths[index++] = static_cast<int16_t>(symbol);
                    } else {
                        int len = 0, rep;
                        if (symbol == 16) {
                            if (index == 0) return -5;
                            len = lengths[index - 1];
                            rep = 3 + Bits(2);
                        } else if (symbol == 17) {
                            rep = 3 + Bits(3);
                        } else {
                            rep = 11 + Bits(7);
                        }
                        if (err) return -1;
                        if (index + rep > nlen + ndist) return -6;
                        while (rep--) lengths[index++] = static_cast<int16_t>(len);
                    }
                }
                if (lengths[256] == 0) return -9;
                int e = Construct(&lencode, lengths, nlen);
                if (e < 0 || (e > 0 && nlen - lencode.count[0] != 1)) return -7;
                e = Construct(&distcode, lengths + nlen, ndist);
                if (e < 0 || (e > 0 && ndist - distcode.count[0] != 1)) return -8;
                return Codes(&lencode, &distcode);
            }

            /* zlib ヘッダ付きのデータを最後まで展開する。成功なら 0。 */
            int Run() {
                const int cmf = getb(ctx), flg = getb(ctx);
                if (cmf < 0 || flg < 0) return -1;
                if ((cmf & 0x0F) != 8 || ((cmf << 8) | flg) % 31 != 0 || (flg & 0x20)) return -12;
                int last;
                do {
                    last = Bits(1);
                    const int type = Bits(2);
                    if (err) return -1;
                    int r;
                    switch (type) {
                        case 0: r = Stored(); break;
                        case 1: r = Fixed(); break;
                        case 2: r = Dynamic(); break;
                        default: r = -13; break;
                    }
                    if (r != 0) return r;
                } while (!last);
                return 0;
            }
        };

        /* ---- PNG ------------------------------------------------------------------ */
        constexpr uint32_t kMaxStride = kMaxSrcDim * 8 + 1;

        struct Png {
            Reader rd;
            Accum* acc = nullptr;
            Inflate* inf = nullptr;

            int w = 0, h = 0, depth = 0, color = 0, channels = 0;
            uint32_t stride = 0;
            int bpp = 1;
            uint8_t pal[256 * 3];
            uint8_t trns[256];
            int npal = 0;

            /* IDAT 供給 */
            uint32_t chunkRemain = 0;
            bool idatDone = false;

            /* 行の組み立て */
            uint8_t* cur = nullptr;
            uint8_t* prev = nullptr;
            uint32_t rowPos = 0;
            int rowY = 0;
            uint8_t filter = 0;

            int GetIdat() {
                while (chunkRemain == 0) {
                    if (idatDone) return -1;
                    uint32_t crc, len, type;
                    if (!rd.U32BE(crc) || !rd.U32BE(len) || !rd.U32BE(type)) { idatDone = true; return -1; }
                    if (type != 0x49444154u) { idatDone = true; return -1; } /* "IDAT" */
                    chunkRemain = len;
                }
                chunkRemain--;
                return rd.Get();
            }

            uint32_t SampleAt(const uint8_t* row, uint32_t idx) const {
                switch (depth) {
                    case 8: return row[idx];
                    case 16: return row[idx * 2];
                    default: {
                        const uint32_t bit = idx * static_cast<uint32_t>(depth);
                        const uint32_t byte = row[bit >> 3];
                        const int shift = 8 - depth - static_cast<int>(bit & 7);
                        return (byte >> shift) & ((1u << depth) - 1);
                    }
                }
            }

            void ProcessRow(const uint8_t* row, int y) {
                const uint32_t maxv = (depth >= 8) ? 255u : ((1u << depth) - 1);
                for (int x = 0; x < w; x++) {
                    int r, g, b, a = 255;
                    const uint32_t ux = static_cast<uint32_t>(x);
                    switch (color) {
                        case 0: {
                            r = g = b = static_cast<int>(SampleAt(row, ux) * 255 / maxv);
                            break;
                        }
                        case 2: {
                            r = static_cast<int>(SampleAt(row, ux * 3));
                            g = static_cast<int>(SampleAt(row, ux * 3 + 1));
                            b = static_cast<int>(SampleAt(row, ux * 3 + 2));
                            break;
                        }
                        case 3: {
                            const uint32_t i = SampleAt(row, ux);
                            if (static_cast<int>(i) < npal) {
                                r = pal[i * 3]; g = pal[i * 3 + 1]; b = pal[i * 3 + 2];
                            } else { r = g = b = 0; }
                            a = trns[i];
                            break;
                        }
                        case 4: {
                            r = g = b = static_cast<int>(SampleAt(row, ux * 2));
                            a = static_cast<int>(SampleAt(row, ux * 2 + 1));
                            break;
                        }
                        default: { /* 6 */
                            r = static_cast<int>(SampleAt(row, ux * 4));
                            g = static_cast<int>(SampleAt(row, ux * 4 + 1));
                            b = static_cast<int>(SampleAt(row, ux * 4 + 2));
                            a = static_cast<int>(SampleAt(row, ux * 4 + 3));
                            break;
                        }
                    }
                    acc->Add(x, y, (r * 299 + g * 587 + b * 114) / 1000, a);
                    if (g_PxSink) g_PxSink(x, y, r, g, b, a);
                }
            }

            void FinishRow() {
                /* cur[1..stride] がフィルタ済みの行 */
                uint8_t* line = cur + 1;
                const uint8_t* up = prev + 1;
                const uint32_t s = stride;
                const uint32_t B = static_cast<uint32_t>(bpp);
                switch (filter) {
                    case 1: for (uint32_t i = B; i < s; i++) line[i] = static_cast<uint8_t>(line[i] + line[i - B]); break;
                    case 2: for (uint32_t i = 0; i < s; i++) line[i] = static_cast<uint8_t>(line[i] + up[i]); break;
                    case 3:
                        for (uint32_t i = 0; i < s; i++) {
                            const int a = i >= B ? line[i - B] : 0;
                            line[i] = static_cast<uint8_t>(line[i] + ((a + up[i]) >> 1));
                        }
                        break;
                    case 4:
                        for (uint32_t i = 0; i < s; i++) {
                            const int a = i >= B ? line[i - B] : 0;
                            const int b = up[i];
                            const int c = i >= B ? up[i - B] : 0;
                            const int p = a + b - c;
                            int pa = p - a; if (pa < 0) pa = -pa;
                            int pb = p - b; if (pb < 0) pb = -pb;
                            int pc = p - c; if (pc < 0) pc = -pc;
                            const int pred = (pa <= pb && pa <= pc) ? a : (pb <= pc ? b : c);
                            line[i] = static_cast<uint8_t>(line[i] + pred);
                        }
                        break;
                    default: break;
                }
                ProcessRow(line, rowY);
                rowY++;
                uint8_t* t = cur; cur = prev; prev = t;
                rowPos = 0;
            }

            void Put(uint8_t b) {
                if (rowY >= h) return;
                if (rowPos == 0) { filter = b; rowPos = 1; return; }
                cur[rowPos++] = b;
                if (rowPos == stride + 1) FinishRow();
            }
        };

        inline int PngGetThunk(void* c) { return static_cast<Png*>(c)->GetIdat(); }
        inline void PngPutThunk(void* c, uint8_t b) { static_cast<Png*>(c)->Put(b); }

        inline Status DecodePng(const Source& src, bool invert, Bitmap& out) {
            static Png png;
            static Accum acc;
            static Inflate inf;
            static uint8_t rows[2][kMaxStride + 8];

            png.rd.Init(&src, 8);
            png.npal = 0;
            std::memset(png.trns, 255, sizeof(png.trns));
            bool haveHdr = false, haveIdat = false;
            while (!haveIdat) {
                uint32_t len, type;
                if (!png.rd.U32BE(len) || !png.rd.U32BE(type)) return BadData;
                if (type == 0x49484452u) { /* IHDR */
                    uint8_t h[13];
                    if (len != 13 || !png.rd.Read(h, 13)) return BadData;
                    const uint32_t pw = (uint32_t(h[0]) << 24) | (uint32_t(h[1]) << 16) | (uint32_t(h[2]) << 8) | h[3];
                    const uint32_t ph = (uint32_t(h[4]) << 24) | (uint32_t(h[5]) << 16) | (uint32_t(h[6]) << 8) | h[7];
                    if (pw == 0 || ph == 0 || pw > static_cast<uint32_t>(kMaxSrcDim) || ph > static_cast<uint32_t>(kMaxSrcDim)) return TooBig;
                    png.w = static_cast<int>(pw);
                    png.h = static_cast<int>(ph);
                    png.depth = h[8];
                    png.color = h[9];
                    if (h[10] != 0 || h[11] != 0 || h[12] != 0) return BadFormat; /* 圧縮/フィルタ方式違い、インターレース */
                    switch (png.color) {
                        case 0: png.channels = 1; if (png.depth != 1 && png.depth != 2 && png.depth != 4 && png.depth != 8 && png.depth != 16) return BadFormat; break;
                        case 2: png.channels = 3; if (png.depth != 8 && png.depth != 16) return BadFormat; break;
                        case 3: png.channels = 1; if (png.depth != 1 && png.depth != 2 && png.depth != 4 && png.depth != 8) return BadFormat; break;
                        case 4: png.channels = 2; if (png.depth != 8 && png.depth != 16) return BadFormat; break;
                        case 6: png.channels = 4; if (png.depth != 8 && png.depth != 16) return BadFormat; break;
                        default: return BadFormat;
                    }
                    const uint32_t bitsPP = static_cast<uint32_t>(png.channels * png.depth);
                    png.stride = (static_cast<uint32_t>(png.w) * bitsPP + 7) / 8;
                    png.bpp = static_cast<int>(bitsPP / 8 > 0 ? bitsPP / 8 : 1);
                    if (png.stride > kMaxStride - 1) return TooBig;
                    haveHdr = true;
                    png.rd.Seek(png.rd.Tell() + 4); /* CRC */
                } else if (type == 0x504C5445u) { /* PLTE */
                    if (len > 768 || len % 3 != 0 || !png.rd.Read(png.pal, len)) return BadData;
                    png.npal = static_cast<int>(len / 3);
                    png.rd.Seek(png.rd.Tell() + 4);
                } else if (type == 0x74524E53u) { /* tRNS */
                    if (len <= 256) {
                        if (!png.rd.Read(png.trns, len)) return BadData;
                        png.rd.Seek(png.rd.Tell() + 4);
                    } else png.rd.Seek(png.rd.Tell() + len + 4);
                } else if (type == 0x49444154u) { /* IDAT */
                    png.chunkRemain = len;
                    haveIdat = true;
                } else if (type == 0x49454E44u) { /* IEND */
                    return BadData;
                } else {
                    png.rd.Seek(png.rd.Tell() + len + 4);
                }
                if (png.rd.Tell() > src.size) return BadData;
            }
            if (!haveHdr) return BadData;

            acc.Init(png.w, png.h);
            png.acc = &acc;
            png.inf = &inf;
            png.idatDone = false;
            png.cur = rows[0];
            png.prev = rows[1];
            std::memset(rows, 0, sizeof(rows));
            png.rowPos = 0;
            png.rowY = 0;

            inf.getb = PngGetThunk;
            inf.putb = PngPutThunk;
            inf.ctx = &png;
            inf.bitbuf = 0;
            inf.bitcnt = 0;
            inf.wpos = 0;
            inf.err = false;

            const int r = inf.Run();
            if (r != 0 && png.rowY < png.h) return BadData;
            if (png.rowY < png.h) return BadData;
            Finish(acc, invert, out);
            return Ok;
        }

        /* ---- BMP ------------------------------------------------------------------ */
        inline uint32_t Le32(const uint8_t* p) { return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24); }
        inline uint16_t Le16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }

        inline Status DecodeBmp(const Source& src, bool invert, Bitmap& out) {
            static Accum acc;
            static uint8_t rowbuf[kMaxSrcDim * 4 + 8];
            uint8_t hdr[64];
            if (src.size < 54 || !src.read(src.ctx, 0, hdr, 54)) return BadData;
            const uint32_t dataOff = Le32(hdr + 10);
            const uint32_t dibSize = Le32(hdr + 14);
            if (dibSize < 40) return BadFormat;
            const int32_t bw = static_cast<int32_t>(Le32(hdr + 18));
            const int32_t bhSigned = static_cast<int32_t>(Le32(hdr + 22));
            const int bpp = Le16(hdr + 28);
            const uint32_t comp = Le32(hdr + 30);
            uint32_t ncol = Le32(hdr + 46);
            if (comp != 0 && !(comp == 3 && bpp == 32)) return BadFormat;
            if (bpp != 8 && bpp != 24 && bpp != 32) return BadFormat;
            const bool topDown = bhSigned < 0;
            const int w = bw;
            const int h = topDown ? -bhSigned : bhSigned;
            if (w <= 0 || h <= 0 || w > kMaxSrcDim || h > kMaxSrcDim) return TooBig;

            uint8_t pal[256 * 4];
            if (bpp == 8) {
                if (ncol == 0 || ncol > 256) ncol = 256;
                if (!src.read(src.ctx, 14 + dibSize, pal, ncol * 4)) return BadData;
            }
            const uint32_t stride = ((static_cast<uint32_t>(w) * static_cast<uint32_t>(bpp) + 31) / 32) * 4;
            if (static_cast<uint64_t>(dataOff) + static_cast<uint64_t>(stride) * static_cast<uint64_t>(h) > src.size) return BadData;

            acc.Init(w, h);
            for (int fileRow = 0; fileRow < h; fileRow++) {
                if (!src.read(src.ctx, dataOff + static_cast<uint64_t>(fileRow) * stride, rowbuf, stride)) return BadData;
                const int y = topDown ? fileRow : (h - 1 - fileRow);
                for (int x = 0; x < w; x++) {
                    int r, g, b;
                    if (bpp == 8) {
                        const uint8_t* c = pal + rowbuf[x] * 4;
                        b = c[0]; g = c[1]; r = c[2];
                    } else {
                        const uint8_t* c = rowbuf + x * (bpp / 8);
                        b = c[0]; g = c[1]; r = c[2];
                    }
                    acc.Add(x, y, (r * 299 + g * 587 + b * 114) / 1000, 255);
                    if (g_PxSink) g_PxSink(x, y, r, g, b, 255);
                }
            }
            Finish(acc, invert, out);
            return Ok;
        }

    } /* namespace detail */

    /* ファイルの種類を見分けて読み込む */
    inline Status Decode(const Source& src, bool invert, Bitmap& out) {
        uint8_t magic[8];
        if (src.size < 8 || !src.read(src.ctx, 0, magic, 8)) return BadData;
        static const uint8_t png[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
        if (std::memcmp(magic, png, 8) == 0)
            return detail::DecodePng(src, invert, out);
        if (magic[0] == 'B' && magic[1] == 'M')
            return detail::DecodeBmp(src, invert, out);
        return BadFormat;
    }

}
