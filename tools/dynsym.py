#!/usr/bin/env python3
"""Dump dynamic symbols (and PLT relocations) of a decompressed NSO flat image.
usage: dynsym.py <img> [syms|plt]"""
import sys, struct
b = open(sys.argv[1], 'rb').read()
u32 = lambda o: struct.unpack_from('<I', b, o)[0]
u64 = lambda o: struct.unpack_from('<Q', b, o)[0]
mod0 = u32(4)
assert b[mod0:mod0 + 4] == b'MOD0', 'no MOD0'
dyn = mod0 + struct.unpack_from('<i', b, mod0 + 4)[0]
d = {}
o = dyn
while True:
    t, v = u64(o), u64(o + 8); o += 16
    if t == 0: break
    d.setdefault(t, v)
symtab, strtab, strsz = d[6], d[5], d[10]
nsym = u32(d[4] + 4) if 4 in d else (strtab - symtab) // 24
def name(i):
    off = u32(symtab + 24 * i)
    e = b.index(b'\0', strtab + off)
    return b[strtab + off:e].decode('latin1')
mode = sys.argv[2] if len(sys.argv) > 2 else 'syms'
if mode == 'syms':
    for i in range(1, nsym):
        st = symtab + 24 * i
        val = u64(st + 8); shndx = struct.unpack_from('<H', b, st + 6)[0]
        if shndx and val: print(f'{val:#x} {name(i)}')
else:
    jmprel, sz = d[23], d[2]
    for k in range(sz // 24):
        r = jmprel + 24 * k
        off, info = u64(r), u64(r + 8)
        print(f'{off:#x} {name(info >> 32)}')
