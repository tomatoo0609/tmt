# tmt: IDA script 9 (12.0.0 / 6cbb1669) - 募集通知 (sub_331C410, 種類 12) の送り先の制限を探す
# フレンド以外には通知が少ししか届かない。送る相手の絞り込み・回数制限・間隔制限がどこにあるかを見る。
# 331C410 とその呼び先 (2 階層・最大 70 関数) を逆コンパイルして出力する。
import idautils, idc, ida_funcs, ida_hexrays, os
OUT = r'C:\Users\takah\Music\tmt_out'
os.makedirs(OUT, exist_ok=True)
done = {}; notes = []

def dec(ea, tag=''):
    f = ida_funcs.get_func(ea)
    if not f: return None
    if f.start_ea in done: return f.start_ea
    try: done[f.start_ea] = [tag, str(ida_hexrays.decompile(f.start_ea))]
    except Exception as e: done[f.start_ea] = [tag, 'ERR %s' % e]
    return f.start_ea

def callees(ea):
    f = ida_funcs.get_func(ea); out = []
    if not f: return out
    for h in idautils.FuncItems(f.start_ea):
        if idc.print_insn_mnem(h) in ('BL', 'B'):
            t = idc.get_operand_value(h, 0)
            tf = ida_funcs.get_func(t)
            if tf and tf.start_ea == t and t not in out: out.append(t)
    return out

root = 0x331C410
dec(root, 'notify send (type 12 = recruit)')
lvl1 = callees(root); notes.append('callees of 331C410: ' + ' '.join('%x' % x for x in lvl1))
for c in lvl1:
    if len(done) >= 70: break
    dec(c, 'callee of 331C410')
for c in lvl1:
    for d in callees(c):
        if len(done) >= 70: break
        dec(d, 'callee2 via %x' % c)
# 募集を作る側
for a in (0x3269910, 0x32845D0, 0x390C980):
    dec(a, 'recruit build')
# 呼び出し元 (ほかの通知の種類の呼び方を見る)
for x in list(idautils.XrefsTo(root))[:12]:
    notes.append('ref 331C410 <- %x' % x.frm); dec(x.frm, 'caller of 331C410')
with open(os.path.join(OUT, 'notify9_funcs.txt'), 'w', encoding='utf-8') as o:
    for ea, (tag, code) in sorted(done.items()):
        o.write('\n\n######## %x  [%s]\n%s\n' % (ea, tag, code))
with open(os.path.join(OUT, 'notify9_notes.txt'), 'w', encoding='utf-8') as o:
    o.write('\n'.join(notes))
print('tmt9 done: %d funcs, %d notes' % (len(done), len(notes)))
