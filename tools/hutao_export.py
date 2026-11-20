# PMX -> 面ごとの色つき軽量メッシュ (hutao_mesh_data.h)。使い方: PMX のあるフォルダで python3 -I hutao_export.py <出力.h>
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
alias = {'tex/发.png': 'tex/hair2.png'}
tex = {}
for i, t in enumerate(texn):
    t = alias.get(t, t)
    if os.path.exists(t): tex[i] = np.array(Image.open(t).convert('RGBA'))
def samp(im, u, v):
    h, w = im.shape[:2]; return im[int((v % 1.0)*(h-1)), int((u % 1.0)*(w-1))].astype(np.float32)
keepF, colF, nrmF = [], [], []; fi = 0
for dif, ti, fc in mats:
    for k in range(fi, fi+fc):
        a, b, c = F[k]
        if dif[3] < 0.05: continue
        im = tex.get(ti)
        if im is None: col = np.array([dif[0]*255, dif[1]*255, dif[2]*255, dif[3]*255], np.float32)
        else:
            uvs = [UV[a], UV[b], UV[c]]; cen = (UV[a]+UV[b]+UV[c])/3
            smp = [samp(im, *cen)] + [samp(im, *(cen*0.5+q*0.5)) for q in uvs]
            col = np.mean(smp, axis=0); col[:3] *= np.array(dif[:3]); col[3] *= dif[3]
        if col[3] < 128: continue
        n = np.cross(V[b]-V[a], V[c]-V[a]); l = np.linalg.norm(n)
        if l < 1e-9: continue
        n /= l
        if np.dot(n, N[a]+N[b]+N[c]) < 0: n = -n      # 頂点の向き (外向き) にそろえる
        n[2] = -n[2]                                      # z を反転 (PMX は左手系)
        keepF.append((a, b, c)); colF.append(np.clip(col[:3], 0, 255).astype(np.uint8)); nrmF.append(np.clip(np.round(n*127), -127, 127).astype(np.int8))
    fi += fc
keepF = np.array(keepF, np.uint16); colF = np.array(colF, np.uint8); nrmF = np.array(nrmF, np.int8)
Vq = np.clip(np.round(V*1000), -32000, 32000).astype(np.int16)
def arr(name, ty, a):
    f = a.reshape(-1); s = ','.join(str(int(x)) for x in f)
    return 'static constexpr %s %s[%d] = {%s};\n' % (ty, name, len(f), s)
with open(sys.argv[1], 'w') as o:
    o.write('constexpr int kNV = %d, kNF = %d;\n' % (nv, len(keepF)))
    o.write(arr('kV', 'int16_t', Vq)); o.write(arr('kF', 'uint16_t', keepF)); o.write(arr('kC', 'uint8_t', colF)); o.write(arr('kN', 'int8_t', nrmF))
print('verts', nv, 'faces', len(keepF), 'of', len(F))
