import struct, sys, os
d = open(sys.argv[1], 'rb').read(); out = sys.argv[2]; os.makedirs(out, exist_ok=True)
assert d[:4] == b'SARC', d[:4]
bom = d[6:8]; e = '<' if bom == b'\xff\xfe' else '>'
hl, = struct.unpack_from(e+'H', d, 4); doff, = struct.unpack_from(e+'I', d, 0xC)
o = hl; assert d[o:o+4] == b'SFAT'
shl, cnt, mult = struct.unpack_from(e+'HHI', d, o+4); o += shl
nodes = [struct.unpack_from(e+'IIII', d, o + 16*i) for i in range(cnt)]
o += 16*cnt; assert d[o:o+4] == b'SFNT'; sh, = struct.unpack_from(e+'H', d, o+4); names = o + sh
for h, attr, s, en in nodes:
    nm = 'f_%08x' % h
    if attr & 0x01000000:
        no = names + (attr & 0xFFFF) * 4; nm = d[no:d.index(b'\0', no)].decode()
    p = os.path.join(out, nm); os.makedirs(os.path.dirname(p) or out, exist_ok=True)
    open(p, 'wb').write(d[doff+s:doff+en]); print(nm, en-s)
