import idautils, ida_funcs, ida_hexrays, os, re
OUT = r'C:\Users\takah\Music\tmt_out'
done = {}
def dec(ea):
    f = ida_funcs.get_func(ea)
    if not f or f.start_ea in done: return
    try: done[f.start_ea] = str(ida_hexrays.decompile(f.start_ea))
    except Exception as e: done[f.start_ea] = 'ERR %s' % e
ROOTS = [0x14E130C, 0x3579FC0, 0x22DEA94, 0x14F7290, 0x331453C, 0x3254234]
for r in ROOTS: dec(r)
first = list(done.values())
for code in first:
    for m in re.findall(r'sub_([0-9A-F]+)', code): dec(int(m, 16))
with open(os.path.join(OUT, 'funcs3.txt'), 'w', encoding='utf-8') as o:
    for ea, code in sorted(done.items()): o.write('\n\n######## %x%s\n%s\n' % (ea, ' ROOT' if ea in ROOTS else '', code))
print('tmt3 done', len(done))
