# 既存の hutao_tex_data.h の BC1 を復号して半分の大きさに縮めて作り直す (起動時メモリ対策)
# 使い方: python3 tools/hutao_shrink.py in.h out.h
import re, sys, numpy as np
def bc1(img):
    H, W = img.shape[:2]
    bl = img[:, :, :3].reshape(H//4, 4, W//4, 4, 3).transpose(0, 2, 1, 3, 4).reshape(-1, 16, 3).astype(np.float32)
    mean = bl.mean(1); d = bl - mean[:, None]; cov = np.einsum('nki,nkj->nij', d, d)
    v = np.ones((len(bl), 3), np.float32)
    for _ in range(5): v = np.einsum('nij,nj->ni', cov, v); v /= (np.linalg.norm(v, axis=1, keepdims=True) + 1e-9)
    pr = np.einsum('nki,ni->nk', d, v); lo = pr.argmin(1); hi = pr.argmax(1); ar = np.arange(len(bl))
    def q565(c): return ((np.round(c[:, 0]*31/255).astype(np.int32) << 11) | (np.round(c[:, 1]*63/255).astype(np.int32) << 5) | np.round(c[:, 2]*31/255).astype(np.int32))
    c0 = q565(bl[ar, hi]); c1 = q565(bl[ar, lo])
    def x565(c): r = (c >> 11) & 31; gg = (c >> 5) & 63; b = c & 31; return np.stack([(r << 3 | r >> 2), (gg << 2 | gg >> 4), (b << 3 | b >> 2)], 1).astype(np.float32)
    p0 = x565(c0); p1 = x565(c1); pal = np.stack([p0, p1, (2*p0+p1)/3, (p0+2*p1)/3], 1)
    idx = ((bl[:, :, None, :] - pal[:, None, :, :])**2).sum(3).argmin(2).astype(np.uint32)
    out = np.zeros((len(bl), 8), np.uint8)
    out[:, 0] = c0 & 255; out[:, 1] = c0 >> 8; out[:, 2] = c1 & 255; out[:, 3] = c1 >> 8
    for r in range(4): out[:, 4+r] = (idx[:, r*4] | idx[:, r*4+1] << 2 | idx[:, r*4+2] << 4 | idx[:, r*4+3] << 6).astype(np.uint8)
    return out.reshape(-1)

def arr(name, ty, a):
    return 'static constexpr %s %s[%d] = {%s};\n' % (ty, name, len(a), ','.join(str(int(x)) for x in a))
def dec(b, lg):
    n = 1 << lg; nb = n // 4; b = b.reshape(nb, nb, 8).astype(np.int32)
    c0 = b[:, :, 0] | (b[:, :, 1] << 8); c1 = b[:, :, 2] | (b[:, :, 3] << 8)
    def x(c): r = (c >> 11) & 31; g = (c >> 5) & 63; bl = c & 31; return np.stack([r << 3 | r >> 2, g << 2 | g >> 4, bl << 3 | bl >> 2], -1).astype(np.float32)
    p0 = x(c0); p1 = x(c1); pal = np.stack([p0, p1, (2*p0+p1)/3, (p0+2*p1)/3], 2)
    img = np.zeros((n, n, 3), np.float32)
    for y in range(4):
        for xx in range(4):
            k = (b[:, :, 4+y] >> (2*xx)) & 3
            img[y::4, xx::4] = np.take_along_axis(pal, k[:, :, None, None], 2)[:, :, 0, :]
    return img
src = open(sys.argv[1]).read(); out = open(sys.argv[2], 'w')
for m in re.finditer(r'kTex(\d)\[(\d+)\] = \{([^}]*)\}', src):
    i = int(m.group(1)); n = int(m.group(2)); a = np.array([int(v) for v in m.group(3).split(',')], np.uint8)
    lg = {524288: 10, 131072: 9}[n]; im = dec(a, lg)
    h = (im[0::2, 0::2] + im[1::2, 0::2] + im[0::2, 1::2] + im[1::2, 1::2]) / 4
    out.write(arr('kTex%d' % i, 'uint8_t', bc1(h))); print('tex', i, lg, '->', lg-1)
