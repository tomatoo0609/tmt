# tmt: IDA script 2 - decompile given functions and their direct callees (1 level)
import idautils, idc, ida_funcs, ida_hexrays, os, re
OUT = r'C:\Users\takah\Music\tmt_out'
ROOTS = [0x390736C, 0x331C410, 0x390C980, 0x3269910, 0x32845D0, 0x3906CC4, 0x39074A0, 0x3907B50, 0x3906B9C, 0x14F7290,
         0x38CF174, 0x38CF5B0, 0x3573870, 0x331D560, 0x3908398, 0x39084C8]
done = {}
def dec(ea):
    f = ida_funcs.get_func(ea)
    if not f: return None
    if f.start_ea in done: return f.start_ea
    try: done[f.start_ea] = str(ida_hexrays.decompile(f.start_ea))
    except Exception as e: done[f.start_ea] = 'DECOMPILE ERROR %s' % e
    return f.start_ea
lvl1 = []
for r in ROOTS:
    s = dec(r)
    if s is None: continue
    for m in re.findall(r'sub_([0-9A-F]+)', done[s]): lvl1.append(int(m, 16))
for c in lvl1[:250]: dec(c)
with open(os.path.join(OUT, 'funcs2.txt'), 'w', encoding='utf-8') as o:
    for ea in ROOTS:
        f = ida_funcs.get_func(ea)
        if f: o.write('\n\n######## ROOT %x\n%s\n' % (f.start_ea, done.get(f.start_ea, '')))
    for ea, code in sorted(done.items()):
        if any(ida_funcs.get_func(r) and ida_funcs.get_func(r).start_ea == ea for r in ROOTS): continue
        o.write('\n\n######## %x\n%s\n' % (ea, code))
# xrefs to the notification sender and to the alias functions
with open(os.path.join(OUT, 'xrefs2.txt'), 'w', encoding='utf-8') as o:
    for t in (0x331C410, 0x390736C, 0x38CF174, 0x38CF5B0):
        o.write('%x <- %s\n' % (t, ' '.join('%x' % x.frm for x in idautils.XrefsTo(t))))
print('tmt2 done', len(done))
