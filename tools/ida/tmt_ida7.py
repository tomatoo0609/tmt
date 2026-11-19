# tmt: IDA script 7 (12.0.0 / 6cbb1669) - チーム (ホスト・決定後の権威データと書き込み経路)
# 狙い: sub_14E0310 が返すチームの出どころ sub_14E01B0 と、その権威データ
#       (off_5F2D490 / off_5F2D6C8 / off_5F31BE0系) を「書いている」関数を特定する。
#       決定後にホストがプレイヤーのチームを書き換えて配る正規の入口を探す。
import idautils, idc, ida_funcs, ida_hexrays, ida_bytes, os, re
OUT = r'C:\Users\takah\Music\tmt_out'
os.makedirs(OUT, exist_ok=True)
done = {}; notes = []
LIMIT = 220

def dec(ea, tag=''):
    f = ida_funcs.get_func(ea)
    if not f: return None
    if f.start_ea in done or len(done) >= LIMIT: return f.start_ea
    try: done[f.start_ea] = [tag, str(ida_hexrays.decompile(f.start_ea))]
    except Exception as e: done[f.start_ea] = [tag, 'ERR %s' % e]
    return f.start_ea

def refs(ea, tag, per=12, decode=4):
    out = []
    for i, x in enumerate(list(idautils.XrefsTo(ea))[:per]):
        t = idc.print_insn_mnem(x.frm)
        out.append('%x[%s]' % (x.frm, t))
        if i < decode: dec(x.frm, tag)
    notes.append('%s %x <- %s' % (tag, ea, ' '.join(out)))

# 権威データを読む本体と、その木構造の持ち主
dec(0x14E01B0, 'team authoritative fill (sub_14E01B0)')
dec(0x14E0310, 'per-player team query')

# グローバルへの参照 (読み/書き両方)。名前から addr を取る
def ga(name):
    e = idc.get_name_ea_simple(name)
    return None if e == idc.BADADDR else e
for nm in ['off_5F2D490', 'off_5F2D6C8', 'off_5F31BE0', 'off_5F31BE8', 'off_5F31BF0', 'off_5F2D348']:
    e = ga(nm)
    if e is None: notes.append('%s NOT FOUND' % nm); continue
    # そのグローバルが指す先 (ポインタ) の中身
    tgt = idc.get_qword(e)
    notes.append('%s = %x -> %x' % (nm, e, tgt))
    refs(e, 'REF ' + nm, per=20, decode=6)

# ホスト側のチーム確定・配布
dec(0x389BC00, 'host team decide/distribute (caller of 3899340)')
dec(0x38978F0, 'FinalizeTeamData')
dec(0x3899340, 'lobby team build')
dec(0x32D10A0, 'TeamDataStore')
dec(0x32CB7B0, 'team build')
# TeamDataStore / team build を呼ぶ側の、さらに呼び出し元 (配布のきっかけ)
for base in (0x389BC00, 0x3899340):
    refs(base, 'CALLER2 of %x' % base, per=12, decode=3)

# チームを「書き込む」語を持つ関数 (setter 候補)
SET = re.compile(r'(SetTeam|SetPlayerTeam|ChangeTeam|AssignTeam|TeamAssign|DecideTeam|team_assign|PutTeam|WriteTeam|UpdateTeam|TeamUpdate)', re.I)
for s in idautils.Strings():
    t = str(s)
    if len(t) > 100 or not SET.search(t): continue
    rs = [x.frm for x in idautils.XrefsTo(s.ea)]
    notes.append('SET_STR %x %r <- %s' % (s.ea, t, ' '.join('%x' % r for r in rs[:10])))
    for r in rs[:3]: dec(r, 'SET ' + t)

# 決定後のロビー状態: VsBeforeFix / VsTeamSetting の分岐を持つ関数 (44acaf0 周辺)
dec(0x44ACAF0, 'lobby step enum (VsBeforeFix/VsTeamSetting)')
refs(0x44ACAF0, 'enum users', per=10, decode=4)

with open(os.path.join(OUT, 'team7_funcs.txt'), 'w', encoding='utf-8') as o:
    for ea, (tag, code) in sorted(done.items()):
        o.write('\n\n######## %x  [%s]\n%s\n' % (ea, tag, code))
with open(os.path.join(OUT, 'team7_notes.txt'), 'w', encoding='utf-8') as o:
    o.write('\n'.join(notes))
print('tmt7 done: %d funcs, %d notes' % (len(done), len(notes)))
