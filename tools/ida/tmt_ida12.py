# tmt: IDA script 12 (12.0.0 / 6cbb1669) - 自分のキャラを消す (透明化) ための手がかり集め
# 分かっていること: プレイヤー本体 pl = *(カメラが追う物 + 0x11F38)。pl+0x29D0 = 部品 (BodyCalc の a1)、pl+0x29D8 = モデル側 (+0x2A0 = アーマーのモデル)。
# 欲しいもの: ゲームがキャラのモデルを「見せる/見せない」「薄くする」ときに触る場所 (フラグ・関数)。
# 出力: vis12_funcs.txt (逆コンパイル) / vis12_notes.txt (どの文字列・どの命令からたどったか)
import idautils, idc, ida_funcs, ida_hexrays, ida_ua, ida_bytes, os
OUT = r'C:\Users\takah\Music\tmt_out'
os.makedirs(OUT, exist_ok=True)
done = {}; notes = []
def dec(ea, tag=''):
    f = ida_funcs.get_func(ea)
    if not f: return None
    if f.start_ea in done: return f.start_ea
    if len(done) >= 90: return f.start_ea
    try: done[f.start_ea] = [tag, str(ida_hexrays.decompile(f.start_ea))]
    except Exception as e: done[f.start_ea] = [tag, 'ERR %s' % e]
    return f.start_ea
def funcs_using(ea, per=6):
    out = []
    for x in list(idautils.XrefsTo(ea))[:per]:
        f = ida_funcs.get_func(x.frm)
        if f and f.start_ea not in out: out.append(f.start_ea)
    return out
# 1) 文字列からたどる
KEYS = ['PlayerAlpha', 'CutOffAlpha', 'PlayerCustomClothCull', 'SplPlayerSquid_Invisible', 'InvisibleForPlayer', 'SilhouetteState',
        'PlayerFullModelRef', 'IsUpdateOnlyVisible', 'VisibleOn', 'InvisibleByPhotograph', 'FilmingPlayerOnly', 'PlayerDemoFade']
for s in idautils.Strings():
    t = str(s)
    for k in KEYS:
        if k in t and len(t) < 70:
            fs = funcs_using(s.ea)
            notes.append('string %x %r <- %s' % (s.ea, t, ' '.join('%x' % f for f in fs)))
            for f in fs[:3]: dec(f, 'uses string %r' % t)
            break
# 2) プレイヤー本体の +0x29D8 (モデル側) / +0x29D0 (部品) を触る命令を含む関数 (プレイヤー周りのコード 0x2700000-0x2B00000)
hits = {}
ea = 0x2700000
while ea < 0x2B00000:
    if ida_bytes.is_code(ida_bytes.get_flags(ea)):
        insn = ida_ua.insn_t()
        if ida_ua.decode_insn(insn, ea):
            for op in insn.ops:
                if op.type == ida_ua.o_displ and op.addr in (0x29D8, 0x29D0):
                    f = ida_funcs.get_func(ea)
                    if f: hits.setdefault(f.start_ea, set()).add(op.addr)
            ea += insn.size
            continue
    ea += 4
notes.append('functions touching +0x29D8/+0x29D0: %d' % len(hits))
for fa, offs in sorted(hits.items()):
    notes.append('  %x %s' % (fa, ' '.join('+%x' % o for o in sorted(offs))))
for fa in sorted(hits, key=lambda a: (0x29D8 not in hits[a], a))[:45]: dec(fa, 'touches player model/part %s' % ' '.join('+%x' % o for o in sorted(hits[fa])))
# 3) アーマーの出し入れ (モデル側を使う例) の周り
for a in (0x293D7DC, 0x293D8E0, 0x2AF06F0, 0x2879D3C):
    dec(a, 'known: armor / model action %x' % a)
with open(os.path.join(OUT, 'vis12_funcs.txt'), 'w', encoding='utf-8') as o:
    for ea, (tag, code) in sorted(done.items()): o.write('\n\n######## %x  [%s]\n%s\n' % (ea, tag, code))
with open(os.path.join(OUT, 'vis12_notes.txt'), 'w', encoding='utf-8') as o: o.write('\n'.join(notes))
print('tmt12 done: %d funcs' % len(done))
