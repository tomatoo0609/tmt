# tmt: IDA script 8 (12.0.0 / 6cbb1669) - チーム変更の正しい経路を確定する
# 本命: SetTeam (sub_3F684A4) が playerObj+588 に書き、イベントでホスト配布する。
# 必要: その playerObj をどのロビーオブジェクトから、どう取り出すか。
#   sub_437BF40 が lobby+864[index] で playerObj を取り SetTeam(playerObj, team) する。
#   → 437BF40 の呼び出し元 (a1 の出どころ) と、a1 を返すグローバル/関数を特定する。
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

def allrefs(ea, tag, per=16, dd=8):
    out = []
    for i, x in enumerate(list(idautils.XrefsTo(ea))[:per]):
        out.append('%x[%s]' % (x.frm, idc.print_insn_mnem(x.frm)))
        if i < dd: dec(x.frm, tag)
    notes.append('%s %x <- %s' % (tag, ea, ' '.join(out)))

# 配布処理と、そのすべての参照元 (vtable 経由も含めて data 参照を拾う)
dec(0x437BF40, 'SqTeam assign all (lobby+864[idx] -> SetTeam)')
allrefs(0x437BF40, 'ref 437BF40')
# 437BF40 が data テーブル(vtable)から呼ばれる場合: その関数ポインタを含む data への参照
import ida_bytes
seg_lo, seg_hi = 0x4E9D000, 0x6300000
needle = (0x437BF40).to_bytes(8, 'little')
ea = seg_lo
found = []
while ea < seg_hi and len(found) < 12:
    try:
        ea = ida_bytes.bin_search(ea, seg_hi, needle, None, ida_bytes.BIN_SEARCH_FORWARD, ida_bytes.BIN_SEARCH_NOBREAK)
    except Exception:
        ea = idc.BADADDR
    if ea in (idc.BADADDR, None) or ea == -1: break
    found.append(ea)
    for x in idautils.XrefsTo(ea): dec(x.frm, 'uses 437BF40 ptr %x' % ea)
    ea += 8
notes.append('437BF40 ptr in data: %s' % ' '.join('%x' % f for f in found))

# SetTeam 本体と、その呼び出し元すべて (どこが post-decision の権威設定か)
dec(0x3F684A4, 'SetTeam')
allrefs(0x3F684A4, 'ref SetTeam', per=24, dd=12)
# SetTeam を再適用する 3F69188 と、その呼び出し元
dec(0x3F69188, 'SetTeam re-apply (reads +588 when +608==1)')
allrefs(0x3F69188, 'ref 3F69188', per=12, dd=6)
# 同期ループの呼び出し元 (a1 の出どころ = ロビーオブジェクト)
for c in (0x3901FDC, 0x3902320):
    dec(c, 'caller of sync loop 3908CE8')
# TeamChange 状態
dec(0x3F6BD80, 'TeamChange state')
allrefs(0x3F6BD80, 'ref 3F6BD80', per=10, dd=4)

with open(os.path.join(OUT, 'team8_funcs.txt'), 'w', encoding='utf-8') as o:
    for ea, (tag, code) in sorted(done.items()):
        o.write('\n\n######## %x  [%s]\n%s\n' % (ea, tag, code))
with open(os.path.join(OUT, 'team8_notes.txt'), 'w', encoding='utf-8') as o:
    o.write('\n'.join(notes))
print('tmt8 done: %d funcs, %d notes' % (len(done), len(notes)))
