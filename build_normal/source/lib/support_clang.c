/*
 * clang でビルドするときに、devkitA64 (newlib / libgcc) が普段用意している関数の代わり。
 * すべて hidden にして、ゲーム本体の同名関数を上書きしないようにする。
 */
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

#define HIDDEN __attribute__((visibility("hidden")))

/* ---- newlib の fake heap 用シンボル (exlaunch の init.cpp が書き込む) ---- */
HIDDEN char *fake_heap_start;
HIDDEN char *fake_heap_end;

/* ---- 小さなバンプアロケータ (free は何もしない) ---- */
static char *s_HeapCur;

HIDDEN void *malloc(size_t size) {
    if (!s_HeapCur)
        s_HeapCur = fake_heap_start;
    uintptr_t p = ((uintptr_t)s_HeapCur + 15) & ~(uintptr_t)15;
    if (!fake_heap_start || p + size > (uintptr_t)fake_heap_end)
        return NULL;
    s_HeapCur = (char *)(p + size);
    return (void *)p;
}

HIDDEN void free(void *ptr) { (void)ptr; }

/* ---- メモリ・文字列関数 ---- */
HIDDEN void *memcpy(void *dst, const void *src, size_t n) {
    unsigned char *d = dst;
    const unsigned char *s = src;
    while (n--) *d++ = *s++;
    return dst;
}

HIDDEN void *memmove(void *dst, const void *src, size_t n) {
    unsigned char *d = dst;
    const unsigned char *s = src;
    if (d < s) { while (n--) *d++ = *s++; }
    else { d += n; s += n; while (n--) *--d = *--s; }
    return dst;
}

HIDDEN void *memset(void *dst, int c, size_t n) {
    unsigned char *d = dst;
    while (n--) *d++ = (unsigned char)c;
    return dst;
}

HIDDEN int memcmp(const void *a, const void *b, size_t n) {
    const unsigned char *x = a, *y = b;
    for (; n; n--, x++, y++)
        if (*x != *y) return *x - *y;
    return 0;
}

HIDDEN int bcmp(const void *a, const void *b, size_t n) { return memcmp(a, b, n); }

HIDDEN int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

HIDDEN size_t strlen(const char *s) {
    const char *p = s;
    while (*p) p++;
    return (size_t)(p - s);
}

/* ---- 命令キャッシュの同期 (フックでコードを書き換えた後に必要) ---- */
HIDDEN void __clear_cache(void *begin, void *end) {
    uint64_t ctr;
    __asm__ volatile("mrs %0, ctr_el0" : "=r"(ctr));
    const uintptr_t dline = 4u << ((ctr >> 16) & 0xF);
    const uintptr_t iline = 4u << (ctr & 0xF);
    const uintptr_t b = (uintptr_t)begin, e = (uintptr_t)end;

    for (uintptr_t a = b & ~(dline - 1); a < e; a += dline)
        __asm__ volatile("dc cvau, %0" :: "r"(a) : "memory");
    __asm__ volatile("dsb ish" ::: "memory");
    for (uintptr_t a = b & ~(iline - 1); a < e; a += iline)
        __asm__ volatile("ic ivau, %0" :: "r"(a) : "memory");
    __asm__ volatile("dsb ish\n\tisb" ::: "memory");
}

/* ---- ログ用の最小限の vsnprintf ---- */
typedef struct { char *buf; size_t cap; size_t len; } Out;

static void put(Out *o, char c) {
    if (o->len + 1 < o->cap) o->buf[o->len] = c;
    o->len++;
}

static void put_str(Out *o, const char *s, int width, int left, int prec) {
    int n = 0;
    while (s[n] && (prec < 0 || n < prec)) n++;
    if (!left) for (int i = n; i < width; i++) put(o, ' ');
    for (int i = 0; i < n; i++) put(o, s[i]);
    if (left) for (int i = n; i < width; i++) put(o, ' ');
}

static void put_uint(Out *o, unsigned long long v, unsigned base, int upper, int neg,
                     int width, int left, char pad, int prec) {
    char tmp[32];
    int n = 0;
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    do { tmp[n++] = digits[v % base]; v /= base; } while (v);
    while (n < prec && n < (int)sizeof(tmp)) tmp[n++] = '0';
    int total = n + (neg ? 1 : 0);
    if (!left && pad == ' ') for (int i = total; i < width; i++) put(o, ' ');
    if (neg) put(o, '-');
    if (!left && pad == '0') for (int i = total; i < width; i++) put(o, '0');
    while (n) put(o, tmp[--n]);
    if (left) for (int i = total; i < width; i++) put(o, ' ');
}

static void put_float(Out *o, double v, int prec, int width, int left) {
    if (prec < 0) prec = 6;
    if (prec > 9) prec = 9;
    char tmp[48];
    int n = 0, neg = 0;
    if (v != v) { put_str(o, "nan", width, left, -1); return; }
    if (v < 0) { neg = 1; v = -v; }
    if (v > 1e18) { put_str(o, neg ? "-inf" : "inf", width, left, -1); return; }
    unsigned long long scale = 1;
    for (int i = 0; i < prec; i++) scale *= 10;
    unsigned long long whole = (unsigned long long)v;
    unsigned long long frac = (unsigned long long)((v - (double)whole) * (double)scale + 0.5);
    if (frac >= scale) { whole++; frac -= scale; }
    for (int i = 0; i < prec; i++) { tmp[n++] = (char)('0' + frac % 10); frac /= 10; }
    if (prec) tmp[n++] = '.';
    do { tmp[n++] = (char)('0' + whole % 10); whole /= 10; } while (whole);
    if (neg) tmp[n++] = '-';
    if (!left) for (int i = n; i < width; i++) put(o, ' ');
    while (n) put(o, tmp[--n]);
    if (left) for (int i = n; i < width; i++) put(o, ' ');
}

HIDDEN int vsnprintf(char *buf, size_t cap, const char *fmt, va_list ap) {
    Out o = { buf, cap, 0 };
    for (; *fmt; fmt++) {
        if (*fmt != '%') { put(&o, *fmt); continue; }
        fmt++;
        int left = 0, width = 0, prec = -1, lng = 0, alt = 0;
        char pad = ' ';
        for (;; fmt++) {
            if (*fmt == '-') left = 1;
            else if (*fmt == '0') pad = '0';
            else if (*fmt == '#') alt = 1;
            else if (*fmt == '+' || *fmt == ' ') {}
            else break;
        }
        if (*fmt == '*') { width = va_arg(ap, int); fmt++; }
        else while (*fmt >= '0' && *fmt <= '9') width = width * 10 + (*fmt++ - '0');
        if (*fmt == '.') {
            fmt++; prec = 0;
            if (*fmt == '*') { prec = va_arg(ap, int); fmt++; }
            else while (*fmt >= '0' && *fmt <= '9') prec = prec * 10 + (*fmt++ - '0');
        }
        for (;; fmt++) {
            if (*fmt == 'l') lng++;
            else if (*fmt == 'z' || *fmt == 'j' || *fmt == 't') lng = 2;
            else if (*fmt == 'h') {}
            else break;
        }
        switch (*fmt) {
            case 'd': case 'i': {
                long long v = lng ? va_arg(ap, long long) : va_arg(ap, int);
                unsigned long long u = v < 0 ? (unsigned long long)(-(v + 1)) + 1 : (unsigned long long)v;
                put_uint(&o, u, 10, 0, v < 0, width, left, pad, prec);
                break;
            }
            case 'u': case 'x': case 'X': case 'o': {
                unsigned long long v = lng ? va_arg(ap, unsigned long long) : va_arg(ap, unsigned int);
                unsigned base = *fmt == 'u' ? 10 : (*fmt == 'o' ? 8 : 16);
                if (alt && base == 16) { put(&o, '0'); put(&o, 'x'); }
                put_uint(&o, v, base, *fmt == 'X', 0, width, left, pad, prec);
                break;
            }
            case 'p': {
                uintptr_t v = (uintptr_t)va_arg(ap, void *);
                put(&o, '0'); put(&o, 'x');
                put_uint(&o, v, 16, 0, 0, width, left, pad, prec);
                break;
            }
            case 'c': put(&o, (char)va_arg(ap, int)); break;
            case 's': { const char *s = va_arg(ap, const char *); put_str(&o, s ? s : "(null)", width, left, prec); break; }
            case 'f': case 'F': case 'g': case 'e': put_float(&o, va_arg(ap, double), prec, width, left); break;
            case '%': put(&o, '%'); break;
            case '\0': fmt--; break;
            default: put(&o, '%'); put(&o, *fmt); break;
        }
    }
    if (cap) buf[o.len < cap ? o.len : cap - 1] = '\0';
    return (int)o.len;
}

HIDDEN int snprintf(char *buf, size_t cap, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vsnprintf(buf, cap, fmt, ap);
    va_end(ap);
    return r;
}
