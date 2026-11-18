#!/usr/bin/env python3
"""For a vtable pair (old,new), diff every method's body 1:1 and collect changed immediates.
usage: vtdiff.py oldvt newvt [n_entries]"""
import sys, collections
sys.path.insert(0, '/home/claude/tools')
from porter import *
from immdiff_fields import field, norm
P = '/home/claude/pkg/newmod_handoff/game/'
O = Img('/home/claude/work/old.img', P + 'main_old_28c4287a.nso')
N = Img('/home/claude/work/new.img', P + 'main_new_6cbb1669.nso')

def body(I, a, mx=1500):
    i = a // 4; e = i
    while e < i + mx and int(I.w[e]) != 0xD65F03C0: e += 1
    # include tail after first ret up to next prologue-ish (simple: first ret only)
    return i, e + 1

def diff_fn(oa, na, chg, same):
    oi, oe = body(O, oa); ni, ne = body(N, na)
    if oe - oi != ne - ni: return 'len %d vs %d' % (oe - oi, ne - ni)
    ow, nw = O.w[oi:oe], N.w[ni:ne]
    on, nn = norm(ow), norm(nw)
    bad = int((on != nn).sum())
    for k in range(len(ow)):
        a, b = int(ow[k]), int(nw[k])
        kd, x = field(a)
        if not kd: continue
        rn = (a >> 5) & 31
        if any(((int(ow[k - j]) & 0x9F000000) == 0x90000000) and (int(ow[k - j]) & 31) == rn for j in range(1, 7) if k - j >= 0):
            continue
        kd2, y = field(b)
        if a != b and kd == kd2 and on[k] == nn[k]: chg[(x, y)] += 1
        elif a == b: same[x] += 1
    return 'ok' if bad == 0 else 'struct-diff %d' % bad

ov, nv = int(sys.argv[1], 16), int(sys.argv[2], 16)
n = int(sys.argv[3]) if len(sys.argv) > 3 else 120
chg = collections.Counter(); same = collections.Counter(); st = collections.Counter()
for i in range(n):
    oa, na = O.u64(ov + 8 * i), N.u64(nv + 8 * i)
    if not (0x1000 < oa < O.tl and 0x1000 < na < N.tl): break
    r = diff_fn(oa, na, chg, same); st[r.split()[0]] += 1
print(f'{ov:#x}->{nv:#x} methods {i} status {dict(st)}')
for (x, y), c in sorted(chg.items(), key=lambda t: -t[1])[:25]:
    print(f'   {x:#x} -> {y:#x}  x{c}   (unchanged uses of {x:#x}: {same[x]})')
