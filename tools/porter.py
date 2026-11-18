#!/usr/bin/env python3
"""Port game-relative offsets from the old Splatoon 3 build (28c4287a, 11.3.0) to the new one (6cbb1669, 12.0.0).
Usage: porter.py old.img new.img old.nso new.nso <hex offsets...>
Each offset is classified by segment and ported by:
  text  -> masked instruction-window match (position-dependent immediates removed)
  data/bss/ro -> code references (ADRP + ADD/LDR/STR) mapped old->new, then re-decoded
  plt   -> by import symbol name (.rela.plt)
"""
import sys, struct, collections
import numpy as np

def hdr(path):
    h = open(path, 'rb').read(0x100)
    u = lambda o: struct.unpack_from('<I', h, o)[0]
    return dict(text=(u(0x14), u(0x18)), ro=(u(0x24), u(0x28)), data=(u(0x34), u(0x38)), bss=u(0x3C))

class Img:
    def __init__(self, img, nso):
        self.b = np.fromfile(img, dtype=np.uint8)
        self.h = hdr(nso)
        t0, tl = self.h['text']
        self.tl = tl
        self.w = self.b[:tl // 4 * 4].view(np.uint32)
        self.m = mask(self.w)
        self.dend = self.h['data'][0] + self.h['data'][1]
    def seg(self, a):
        if a < self.h['text'][1]: return 'text'
        if a < self.h['data'][0]: return 'ro'
        if a < self.dend: return 'data'
        return 'bss'
    def u32(self, a): return int(self.b[a:a + 4].view(np.uint32)[0])
    def u64(self, a): return int(self.b[a:a + 8].view(np.uint64)[0])

def mask(w):
    m = w.copy()
    adr = (w & 0x9F000000) == 0x90000000; m[adr] = w[adr] & 0x9F00001F
    adr2 = (w & 0x9F000000) == 0x10000000; m[adr2] = w[adr2] & 0x9F00001F
    bl = (w & 0x7C000000) == 0x14000000; m[bl] = w[bl] & 0xFC000000
    lit = (w & 0x3B000000) == 0x18000000; m[lit] = w[lit] & 0xFF00001F
    add = (w & 0xFF800000) == 0x91000000; m[add] = w[add] & 0xFFC003FF
    # ldr/str unsigned imm: imm may change only if target is page-relative; keep (struct offsets matter)
    return m

def text_map(O, N, a, before=12, after=20, lo=-0x4000, hi=0x14000):
    i = a // 4
    s = max(0, i - before); e = i + after
    pat = O.m[s:e]
    k = i - s
    best = None
    c0 = max(0, i + lo // 4 - k); c1 = min(len(N.m) - len(pat), i + hi // 4)
    win = N.m[c0:c1 + len(pat)]
    from numpy.lib.stride_tricks import sliding_window_view
    sw = sliding_window_view(win, len(pat))
    score = (sw == pat).sum(axis=1)
    j = int(score.argmax()); sc = int(score[j])
    # second best (distinct)
    s2 = score.copy(); s2[max(0, j - 2):j + 3] = 0
    j2 = int(s2.argmax())
    na = (c0 + j + k) * 4
    return na, sc, len(pat), int(s2[j2])

def adrp_refs(I, target, slack=0x200):
    """Return list of (site_of_adrp, site_of_use, exact_target) where exact_target within [target-slack, target+slack]."""
    w = I.w
    isadrp = (w & 0x9F000000) == 0x90000000
    idx = np.nonzero(isadrp)[0]
    ww = w[idx].astype(np.int64)
    imm = ((ww >> 29) & 3) | (((ww >> 5) & 0x7FFFF) << 2)
    imm = np.where(imm & (1 << 20), imm - (1 << 21), imm)
    page = ((idx.astype(np.int64) * 4) & ~0xFFF) + (imm << 12)
    lo_p = (target - slack) & ~0xFFF; hi_p = (target + slack) & ~0xFFF
    sel = idx[(page >= lo_p) & (page <= hi_p)]
    pg = dict(zip(idx.tolist(), page.tolist()))
    out = []
    for si in sel.tolist():
        rd = int(w[si]) & 31
        for k in range(1, 10):
            if si + k >= len(w): break
            x = int(w[si + k])
            off = None
            if (x & 0xFF800000) == 0x91000000 and ((x >> 5) & 31) == rd:
                off = ((x >> 10) & 0xFFF) << (12 if (x >> 22) & 1 else 0)
            elif (x & 0x3B000000) == 0x39000000 and ((x >> 5) & 31) == rd:
                size = x >> 30; V = (x >> 26) & 1; opc = (x >> 22) & 3
                sc = size
                if V and size == 0 and (opc & 2): sc = 4
                off = ((x >> 10) & 0xFFF) << sc
            if off is not None:
                t = pg[si] + off
                if abs(t - target) <= slack: out.append((si * 4, (si + k) * 4, t))
                break
            # stop if rd overwritten by something else
            if (x & 31) == rd and not ((x & 0x3B000000) == 0x39000000 and ((x >> 22) & 1) == 0):
                break
    return out

def decode_site(I, use_site, adrp_site):
    w = I.u32(adrp_site); x = I.u32(use_site)
    imm = ((w >> 29) & 3) | (((w >> 5) & 0x7FFFF) << 2)
    if imm & (1 << 20): imm -= 1 << 21
    page = (adrp_site & ~0xFFF) + (imm << 12)
    if (w & 0x9F000000) != 0x90000000: return None
    if (x & 0xFF800000) == 0x91000000:
        return page + (((x >> 10) & 0xFFF) << (12 if (x >> 22) & 1 else 0))
    if (x & 0x3B000000) == 0x39000000:
        size = x >> 30; V = (x >> 26) & 1; opc = (x >> 22) & 3
        sc = size
        if V and size == 0 and (opc & 2): sc = 4
        return page + (((x >> 10) & 0xFFF) << sc)
    return None

def data_map(O, N, a, maxsites=12):
    refs = adrp_refs(O, a)
    exact = [r for r in refs if r[2] == a]
    use = exact if exact else refs
    votes = collections.Counter(); detail = []
    for (sa, su, t) in use[:maxsites * 3]:
        na, sc, L, s2 = text_map(O, N, sa)
        if sc < L * 0.7 or sc - s2 < 3: continue
        nu = na + (su - sa)
        nt = decode_site(N, nu, na)
        if nt is None: continue
        votes[nt + (a - t)] += 1
        detail.append((hex(sa), hex(na), sc, L))
        if sum(votes.values()) >= maxsites: break
    return votes, len(refs), len(exact)

def main():
    O = Img(sys.argv[1], sys.argv[3]); N = Img(sys.argv[2], sys.argv[4])
    for s in sys.argv[5:]:
        a = int(s, 16); seg = O.seg(a)
        if seg == 'text':
            na, sc, L, s2 = text_map(O, N, a)
            ok = N.u32(na) == O.u32(a)
            print(f'{a:#x} text -> {na:#x}  score {sc}/{L} 2nd {s2} sameword={ok} old={O.u32(a):08x} new={N.u32(na):08x}')
        else:
            v, nr, ne = data_map(O, N, a)
            top = v.most_common(3)
            print(f'{a:#x} {seg} -> ' + ', '.join(f'{k:#x}x{c}' for k, c in top) + f'  (refs {nr}, exact {ne})')

if __name__ == '__main__':
    main()
