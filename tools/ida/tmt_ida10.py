# tmt: IDA script 10 (12.0.0 / 6cbb1669) - ゲームの本物のエフェクト (パーティクル) を出す入口を探す
# 花びら・葉・羽根・光の粒などの名前を含む文字列と、それを使う関数を出す。さらに「エフェクトを出す関数」らしいもの
# (名前に Effect / Ptcl / Emitter を含む文字列の参照元) を逆コンパイルする。出力: eff10_strings.txt / eff10_funcs.txt
import idautils, idc, ida_funcs, ida_hexrays, ida_bytes, os, re
OUT = r'C:\Users\takah\Music\tmt_out'
os.makedirs(OUT, exist_ok=True)
pat = re.compile(r'(petal|flower|leaf|feather|sparkle|glitter|confetti|blossom|sakura|Ptcl|Emitter|EffectMgr|EffectHolder|PlayEffect|Effect_)', re.I)
rows = []; funcs = {}
sc = idautils.Strings(); sc.setup(strtypes=[idc.STRTYPE_C])
for s in sc:
    try: t = str(s)
    except Exception: continue
    if len(t) > 80 or not pat.search(t): continue
    refs = [x.frm for x in idautils.XrefsTo(s.ea)]
    rows.append('%x  %r  <- %s' % (s.ea, t, ' '.join('%x' % r for r in refs[:8])))
    for r in refs[:3]:
        f = ida_funcs.get_func(r)
        if f and f.start_ea not in funcs and len(funcs) < 60:
            try: funcs[f.start_ea] = (t, str(ida_hexrays.decompile(f.start_ea)))
            except Exception as e: funcs[f.start_ea] = (t, 'ERR %s' % e)
rows.sort()
with open(os.path.join(OUT, 'eff10_strings.txt'), 'w', encoding='utf-8') as o: o.write('\n'.join(rows[:3000]))
with open(os.path.join(OUT, 'eff10_funcs.txt'), 'w', encoding='utf-8') as o:
    for ea, (t, c) in sorted(funcs.items()): o.write('\n\n######## %x  [string %r]\n%s\n' % (ea, t, c))
print('tmt10 done: %d strings, %d funcs' % (len(rows), len(funcs)))
