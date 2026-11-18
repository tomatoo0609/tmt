#!/usr/bin/env python3
"""Align the whole old/new text 1:1 (piecewise-constant delta) and collect instructions whose only
change is a struct-offset immediate (ldr/str unsigned imm, ldp/stp imm7, add/sub imm, movz/movk).
Output: work/immdiff.txt  lines: kind old_off new_off count  and work/delta.npy (block deltas)."""
import sys, collections
sys.path.insert(0, '/home/claude/tools')
from porter import *

P = '/home/claude/pkg/newmod_handoff/game/'
O = Img('/home/claude/work/old.img', P + 'main_old_28c4287a.nso')
N = Img('/home/claude/work/new.img', P + 'main_new_6cbb1669.nso')

def norm(w):
    m = mask(w)
    ls = (w & 0x3B000000) == 0x39000000; m[ls] = w[ls] & 0xFFC003FF          # ldr/str uimm
    lp = (w & 0x3A000000) == 0x28000000; m[lp] = w[lp] & 0xFFC07FFF          # ldp/stp imm7
    ad = (w & 0x1F000000) == 0x11000000; m[ad] = w[ad] & 0xFFC003FF          # add/sub imm
    mv = (w & 0x1F800000) == 0x12800000; m[mv] = w[mv] & 0xFFE0001F          # movz/movn/movk
    lu = (w & 0x3B200C00) == 0x38000000; m[lu] = w[lu] & 0xFFE00FFF          # ldur/stur imm9
    return m

ON, NN = norm(O.w), norm(N.w)
BLK = 0x400  # words per block
nb = len(O.w) // BLK
deltas = np.zeros(nb, dtype=np.int64)
d = 0
from numpy.lib.stride_tricks import sliding_window_view
for bi in range(nb):
    i = bi * BLK + BLK // 2
    pat = ON[i - 16:i + 16]
    if len(pat) < 32 or i < 0x400: deltas[bi] = d; continue
    lo = max(0, i - 16 - 0x400); hi = min(len(NN) - 32, i - 16 + 0x3C00)
    sw = sliding_window_view(NN[lo:hi + 32], 32)
    sc = (sw == pat).sum(axis=1)
    j = int(sc.argmax()); best = int(sc[j])
    sc[max(0, j - 4):j + 5] = 0
    if best >= 30 and int(sc.max()) <= 22:
        d = (lo + j - (i - 16)) * 4
    deltas[bi] = d
np.save('/home/claude/work/delta.npy', deltas)

cnt = collections.Counter(); same = collections.Counter()
def field(w):
    if (w & 0x3B000000) == 0x39000000:
        size = w >> 30; V = (w >> 26) & 1; opc = (w >> 22) & 3
        sc = 4 if (V and size == 0 and (opc & 2)) else size
        return 'ls', ((w >> 10) & 0xFFF) << sc
    if (w & 0x3A000000) == 0x28000000:
        opc = w >> 30; V = (w >> 26) & 1
        sc = (2 + opc) if V else (2 + (opc >> 1))
        imm = (w >> 15) & 0x7F
        if imm & 0x40: imm -= 0x80
        return 'lp', imm << sc
    if (w & 0x1F000000) == 0x11000000:
        return 'ad', ((w >> 10) & 0xFFF) << (12 if (w >> 22) & 1 else 0)
    if (w & 0x1F800000) == 0x12800000:
        return 'mv%d' % ((w >> 21) & 3), (w >> 5) & 0xFFFF
    if (w & 0x3B200C00) == 0x38000000:
        imm = (w >> 12) & 0x1FF
        if imm & 0x100: imm -= 0x200
        return 'lu', imm
    return None, None

OW, NW = O.w, N.w
for bi in range(nb):
    dd = int(deltas[bi]) // 4
    s = bi * BLK; e = s + BLK
    if s + dd < 4 or e + dd + 4 >= len(NW): continue
    o = ON[s - 4:e + 4]; n = NN[s - 4 + dd:e + 4 + dd]
    eqn = o == n
    raw = OW[s - 4:e + 4] != NW[s - 4 + dd:e + 4 + dd]
    # context: 3 neighbours each side equal in normalised form
    good = eqn.copy()
    for k in range(1, 4):
        good[k:] &= eqn[:-k]; good[:-k] &= eqn[k:]
    for c in np.nonzero(good & ~raw)[0].tolist():
        k1, a = field(int(OW[s - 4 + c]))
        if k1: same[(k1 if k1[0] != 'm' else 'mv', a)] += 1
    cand = np.nonzero(good & raw)[0]
    for c in cand.tolist():
        gi = s - 4 + c
        ow, nw = int(OW[gi]), int(NW[gi + dd])
        # skip lo12 of an adrp (base register produced by adrp within 6 instrs)
        rn = (ow >> 5) & 31
        if any(((int(OW[gi - k]) & 0x9F000000) == 0x90000000) and (int(OW[gi - k]) & 31) == rn for k in range(1, 7)):
            continue
        k1, a = field(ow); k2, b = field(nw)
        if k1 and k1 == k2 and a != b:
            cnt[(k1, a, b)] += 1
import pickle; pickle.dump((cnt, same), open('/home/claude/work/imm.pkl', 'wb'))
with open('/home/claude/work/immdiff.txt', 'w') as f:
    for (k, a, b), c in sorted(cnt.items(), key=lambda x: (x[0][1], -x[1])):
        f.write(f'{k} {a:#x} {b:#x} {c}\n')
print('blocks', nb, 'pairs', len(cnt), 'final delta', hex(d))
