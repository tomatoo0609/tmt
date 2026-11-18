import numpy as np
from porter import mask
def norm(w):
    m = mask(w)
    ls = (w & 0x3B000000) == 0x39000000; m[ls] = w[ls] & 0xFFC003FF          # ldr/str uimm
    lp = (w & 0x3A000000) == 0x28000000; m[lp] = w[lp] & 0xFFC07FFF          # ldp/stp imm7
    ad = (w & 0x1F000000) == 0x11000000; m[ad] = w[ad] & 0xFFC003FF          # add/sub imm
    mv = (w & 0x1F800000) == 0x12800000; m[mv] = w[mv] & 0xFFE0001F          # movz/movn/movk
    lu = (w & 0x3B200C00) == 0x38000000; m[lu] = w[lu] & 0xFFE00FFF          # ldur/stur imm9
    return m


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

