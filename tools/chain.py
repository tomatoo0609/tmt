#!/usr/bin/env python3
"""Taint-track the value loaded from a global through ldr chains and report field offsets old vs new.
usage: chain.py <global-hex> [...]
prints, per global, counter of (depth, old_off -> new_off)"""
import sys, collections
sys.path.insert(0, '/home/claude/tools')
from porter import *
from immdiff_fields import field
P = '/home/claude/pkg/newmod_handoff/game/'
O = Img('/home/claude/work/old.img', P + 'main_old_28c4287a.nso')
N = Img('/home/claude/work/new.img', P + 'main_new_6cbb1669.nso')

def walk(w, start, n=48):
    """yield (k, depth, kind, off, isload) for accesses whose base is tainted"""
    first = int(w[start]); taint = {first & 31: 0}
    out = []
    for k in range(1, n):
        x = int(w[start + k])
        if (x & 0xFC000000) in (0x94000000,) : taint = {r: d for r, d in taint.items() if r >= 19 and r != 30}; continue  # bl clobbers caller-saved
        if (x & 0xFE1F0000) == 0xD61F0000 or x == 0xD65F03C0: break  # br/ret
        kd, off = field(x)
        rn = (x >> 5) & 31; rt = x & 31
        if kd in ('ls', 'lp', 'lu') and rn in taint:
            isload = (x >> 22) & 1
            out.append((k, taint[rn], kd, off, isload))
            if isload and kd in ('ls', 'lu') and (x >> 30) == 3 and not ((x >> 26) & 1):
                taint[rt] = taint[rn] + 1
                continue
            if isload and kd != 'lp': taint.pop(rt, None)
            continue
        if (x & 0xFF800000) == 0x91000000 and rn in taint:  # add imm
            out.append((k, taint[rn], 'ad', off, 0)); taint[rt] = taint[rn]; continue
        if (x & 0xFFE0FFE0) == 0xAA0003E0 and ((x >> 16) & 31) in taint:  # mov xd, xm
            taint[rt] = taint[(x >> 16) & 31]; continue
        # any other write to a tainted reg untaints it (rough)
        if rt in taint and not (kd in ('ls', 'lp') and not ((x >> 22) & 1)): taint.pop(rt, None)
        if not taint: break
    return out

for s in sys.argv[1:]:
    g = int(s, 16)
    refs = [r for r in adrp_refs(O, g, slack=0) if r[2] == g]
    res = collections.Counter(); used = 0
    for (sa, su, t) in refs[:400]:
        na, sc, L, s2 = text_map(O, N, sa)
        if sc < 27: continue
        nu = na + (su - sa)
        a = walk(O.w, su // 4); b = walk(N.w, nu // 4)
        if [(k, d, kd) for k, d, kd, _, _ in a] != [(k, d, kd) for k, d, kd, _, _ in b]: continue
        used += 1
        for (k, d, kd, p, l), (_, _, _, q, _) in zip(a, b):
            res[(d, p, q)] += 1
    print(f'== {g:#x}  sites {used}/{len(refs)}')
    byd = collections.defaultdict(list)
    for (d, p, q), c in res.items(): byd[(d, p)].append((q, c))
    for (d, p) in sorted(byd):
        qs = sorted(byd[(d, p)], key=lambda t: -t[1])
        tot = sum(c for _, c in qs)
        flag = '' if qs[0][0] == p and len(qs) == 1 else '  <-- CHANGED' if qs[0][0] != p else '  (mixed)'
        if d <= 2: print(f'   depth{d} +{p:#x}: ' + ', '.join(f'{q:#x}x{c}' for q, c in qs[:3]) + flag)
