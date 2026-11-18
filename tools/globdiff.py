import sys, collections
sys.path.insert(0, '/home/claude/tools')
from porter import *
from immdiff_fields import field
P = '/home/claude/pkg/newmod_handoff/game/'
O = Img('/home/claude/work/old.img', P + 'main_old_28c4287a.nso')
N = Img('/home/claude/work/new.img', P + 'main_new_6cbb1669.nso')
def adrp_based(w, k, rn):
    # was rn produced (within 12 instrs back) from an adrp chain?
    for j in range(1, 13):
        if k - j < 0: return False
        x = int(w[k - j])
        if (x & 31) == rn:
            if (x & 0x9F000000) == 0x90000000: return True
            if (x & 0x3B000000) == 0x39000000 or (x & 0xFF800000) == 0x91000000:
                rn = (x >> 5) & 31; continue
            return False
    return False
for s in sys.argv[1:]:
    a = int(s, 16)
    refs = [r for r in adrp_refs(O, a, slack=0) if r[2] == a][:60]
    chg = collections.Counter(); same = collections.Counter(); used = 0
    for (sa, su, t) in refs:
        na, sc, L, s2 = text_map(O, N, sa)
        if sc < 26: continue
        used += 1
        ow = O.w[su // 4: su // 4 + 30]; nw = N.w[(na + su - sa) // 4: (na + su - sa) // 4 + 30]
        for k in range(1, 30):
            x, y = int(ow[k]), int(nw[k])
            kd, p = field(x); kd2, q = field(y)
            if not kd or kd != kd2: continue
            if (x & 0x9F000000) == 0x90000000: continue
            rn = (x >> 5) & 31
            if adrp_based(ow, k, rn) and not ((x >> 5) & 31) == (int(ow[0]) & 31): continue
            if p != q: chg[(kd, p, q)] += 1
            else: same[(kd, p)] += 1
    top = sorted(chg.items(), key=lambda t: -t[1])[:8]
    print(f'{a:#x} sites {used}/{len(refs)} changes:', ', '.join(f'{k}{p:#x}->{q:#x}x{c}(same{same[(k,p)]})' for (k, p, q), c in top))
