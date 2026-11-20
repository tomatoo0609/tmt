# PMX -> 軽量メッシュ (hutao_mesh_data.h) + 圧縮テクスチャ (hutao_tex_data.h)。
# 使い方: PMX のあるフォルダ (tex/ に hair2.png = 发.png の別名) で python3 -I hutao_export.py <mesh.h> <tex.h>
import struct, sys, os, numpy as np
from PIL import Image
data = open('胡桃.pmx','rb').read()
g = list(data[9:17]); enc, auv, vis, tis, mis, bis = g[0], g[1], g[2], g[3], g[4], g[5]
def rs(p):
    n = struct.unpack_from('<i', data, p)[0]; p += 4
    return data[p:p+n].decode('utf-16-le' if enc == 0 else 'utf-8', 'replace'), p+n
p = 17
for _ in range(4): _, p = rs(p)
nv = struct.unpack_from('<i', data, p)[0]; p += 4
V = np.zeros((nv,3), np.float32); N = np.zeros((nv,3), np.float32); UV = np.zeros((nv,2), np.float32)
for i in range(nv):
    V[i] = struct.unpack_from('<3f', data, p); N[i] = struct.unpack_from('<3f', data, p+12); UV[i] = struct.unpack_from('<2f', data, p+24); p += 32 + auv*16
    wt = data[p]; p += 1
    p += [bis, bis*2+4, bis*4+16, bis*2+28][wt]; p += 4
nfi = struct.unpack_from('<i', data, p)[0]; p += 4
F = np.frombuffer(data, dtype={1:np.uint8,2:np.uint16,4:np.uint32}[vis], count=nfi, offset=p).reshape(-1,3).astype(np.int64); p += nfi*vis
nt = struct.unpack_from('<i', data, p)[0]; p += 4; texn = []
for _ in range(nt): t, p = rs(p); texn.append(t)
nm = struct.unpack_from('<i', data, p)[0]; p += 4; mats = []
for _ in range(nm):
    _, p = rs(p); _, p = rs(p)
    dif = struct.unpack_from('<4f', data, p); p += 16+16+12+1+16+4
    ti = struct.unpack_from({1:'<b',2:'<h',4:'<i'}[tis], data, p)[0]; p += tis*2+1
    tf = data[p]; p += 1; p += 1 if tf else tis
    _, p = rs(p); fc = struct.unpack_from('<i', data, p)[0]; p += 4
    mats.append((dif, ti, fc//3))
print('diffuse:', sorted(set(tuple(round(x,2) for x in m[0]) for m in mats)))
alias = {'tex/发.png': 'tex/hair2.png'}
# 使うテクスチャ: (PMX の番号 → 圧縮して入れる番号, 一辺)
atlas = {}; order = []
sizes = {'tex/面.png': 1024, 'tex/发.png': 512, 'tex/服.png': 1024}
for i, t in enumerate(texn):
    if t in sizes and os.path.exists(alias.get(t, t)): atlas[i] = len(order); order.append((t, sizes[t]))
imgs = {}
for i, a in atlas.items():
    t, sz = order[a]; imgs[i] = np.array(Image.open(alias.get(t, t)).convert('RGBA'))
def samp(im, u, v):
    h, w = im.shape[:2]; return im[int((v % 1.0)*(h-1)), int((u % 1.0)*(w-1))].astype(np.float32)
keepF, colF, nrmF, texF = [], [], [], []; fi = 0
for dif, ti, fc in mats:
    for k in range(fi, fi+fc):
        a, b, c = F[k]
        if dif[3] < 0.05: continue
        im = imgs.get(ti)
        if im is None: col = np.array([dif[0]*255, dif[1]*255, dif[2]*255, dif[3]*255], np.float32); tid = 255
        else:
            cen = (UV[a]+UV[b]+UV[c])/3
            col = np.mean([samp(im, *cen)] + [samp(im, *(cen*0.5+q*0.5)) for q in (UV[a], UV[b], UV[c])], axis=0); tid = atlas[ti]
        if col[3] < 128: continue
        n = np.cross(V[b]-V[a], V[c]-V[a]); l = np.linalg.norm(n)
        if l < 1e-9: continue
        n /= l
        if np.dot(n, N[a]+N[b]+N[c]) < 0: n = -n
        n[2] = -n[2]
        keepF.append((a, b, c)); colF.append(np.clip(col[:3], 0, 255).astype(np.uint8)); nrmF.append(np.clip(np.round(n*127), -127, 127).astype(np.int8)); texF.append(tid)
    fi += fc
keepF = np.array(keepF, np.uint16); colF = np.array(colF, np.uint8); nrmF = np.array(nrmF, np.int8); texF = np.array(texF, np.uint8)
Vq = np.clip(np.round(V*1000), -32000, 32000).astype(np.int16)
Nv = N.copy(); Nv[:,2] = -Nv[:,2]; Nv /= np.maximum(np.linalg.norm(Nv, axis=1, keepdims=True), 1e-9); Nq = np.clip(np.round(Nv*127), -127, 127).astype(np.int8)
print('uv range', UV.min(0), UV.max(0)); assert np.abs(UV).max() < 7.9
Uq = np.clip(np.round(UV*4096), -32000, 32000).astype(np.int16)
def arr(name, ty, a):
    f = a.reshape(-1); return 'static constexpr %s %s[%d] = {%s};\n' % (ty, name, len(f), ','.join(str(int(x)) for x in f))
with open(sys.argv[1], 'w') as o:
    o.write('constexpr int kNV = %d, kNF = %d;\n' % (nv, len(keepF)))
    o.write(arr('kV', 'int16_t', Vq)); o.write(arr('kU', 'int16_t', Uq)); o.write(arr('kNv', 'int8_t', Nq))
    o.write(arr('kF', 'uint16_t', keepF)); o.write(arr('kC', 'uint8_t', colF)); o.write(arr('kN', 'int8_t', nrmF)); o.write(arr('kT', 'uint8_t', texF))
# --- BC1 (4 色固定。端の色 2 つ + 間の 2 色、1 画素 2bit) ---
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
with open(sys.argv[2], 'w') as o:
    for a, (t, sz) in enumerate(order):
        im = Image.open(alias.get(t, t)).convert('RGBA').resize((sz, sz), Image.LANCZOS)
        im = im.copy(); arrp = np.array(im)
        o.write(arr('kTex%d' % a, 'uint8_t', bc1(arrp))); print('tex', a, t, sz)
print('verts', nv, 'faces', len(keepF), 'of', len(F), 'tex ids used', sorted(set(texF.tolist())))
