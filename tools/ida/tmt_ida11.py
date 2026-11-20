# tmt: IDA script 11 (12.0.0 / 6cbb1669) - ゲームのエフェクトを自分で出す手順を確定する
# 前回 (ida10) で分かったこと: sub_A70500(System, handle出力, a3 = EmitterSet 番号, a4 = 資源番号, a5 = グループ, a6, a7) が
#   エフェクト (EmitterSet) を 1 つ作る本体。これを呼ぶゲーム側の関数 (System をどこから得るか・位置をどう渡すか) を見る。
# 出力: eff11_funcs.txt (呼び出し元と、その呼び出し元) / eff11_notes.txt
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
def callers(ea, tag, per=14):
    out = []
    for x in list(idautils.XrefsTo(ea))[:per]:
        f = ida_funcs.get_func(x.frm)
        if f: out.append(f.start_ea)
    notes.append('%s %x <- %s' % (tag, ea, ' '.join('%x' % o for o in out)))
    return out
c1 = callers(0xA70500, 'callers of CreateEmitterSet')
for c in c1: dec(c, 'caller of A70500')
c2 = []
for c in c1[:8]:
    c2 += callers(c, 'callers of %x' % c, 6)
for c in c2[:30]: dec(c, 'caller2')
# 位置を渡す側: 作ったハンドルの位置を書く関数 (ハンドル a2 の使われ方)
dec(0xA50990, 'EmitterSet init (called from A70500)')
dec(0xA5112C, 'EmitterSet release')
for a in (0x1191B10, 0x2A7A6EC, 0x1171290):
    dec(a, 'resource/handle helper %x' % a)
with open(os.path.join(OUT, 'eff11_funcs.txt'), 'w', encoding='utf-8') as o:
    for ea, (tag, code) in sorted(done.items()): o.write('\n\n######## %x  [%s]\n%s\n' % (ea, tag, code))
with open(os.path.join(OUT, 'eff11_notes.txt'), 'w', encoding='utf-8') as o: o.write('\n'.join(notes))
print('tmt11 done: %d funcs' % len(done))
