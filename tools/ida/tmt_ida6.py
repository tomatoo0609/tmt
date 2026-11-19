# tmt: IDA script 6 (12.0.0 / 6cbb1669)
#  A) 浮遊の同期: 相手側でプレイヤーの位置がどう決まるか (PlayerNetState 586 の受信と補間、SideStepStart 126 の受信)
#  B) チーム変更: チーム決定後 (試合開始直前) のチームの持ち場所と、ホストが配る処理
# 出力: OUT フォルダに sync6_funcs.txt / team6_funcs.txt / tmt6_notes.txt
import idautils, idc, ida_funcs, ida_hexrays, ida_bytes, os, re
OUT = r'C:\Users\takah\Music\tmt_out'
os.makedirs(OUT, exist_ok=True)
groups = {'sync': {}, 'team': {}}
notes = []
LIMIT = 260

def dec(grp, ea, tag=''):
    f = ida_funcs.get_func(ea)
    if not f: return None
    d = groups[grp]
    if f.start_ea in d or len(d) >= LIMIT: return f.start_ea
    try: d[f.start_ea] = [tag, str(ida_hexrays.decompile(f.start_ea))]
    except Exception as e: d[f.start_ea] = [tag, 'ERR %s' % e]
    return f.start_ea

def callers(grp, ea, tag, n=30):
    out = []
    for x in list(idautils.XrefsTo(ea))[:n]:
        s = dec(grp, x.frm, tag)
        out.append('%x(%x)' % (x.frm, s or 0))
    notes.append('%s: %s' % (tag, ' '.join(out)))

def strrefs(grp, pat, tag, per=4):
    rx = re.compile(pat)
    for s in idautils.Strings():
        t = str(s)
        if len(t) > 120 or not rx.search(t): continue
        refs = [x.frm for x in idautils.XrefsTo(s.ea)]
        notes.append('%s STR %x %r <- %s' % (tag, s.ea, t, ' '.join('%x' % r for r in refs[:12])))
        for r in refs[:per]: dec(grp, r, '%s %s' % (tag, t))

# ===== A) 同期 =====
A = 'sync'
dec(A, 0x2ABE090, 'Maneuver slide start (sends SideStepStart 126)')
dec(A, 0x2ABEFF0, 'slide local start (outputs pos to sp+92)')
dec(A, 0x27FD070, 'PlayerNetState586 write')
dec(A, 0x27FDB50, 'PlayerNetState586 read')
dec(A, 0x27E7D08, 'SideStepStart126 write')
dec(A, 0x27E7F3C, 'SideStepStart126 read')
dec(A, 0x28D17D0, 'remote event loop (handles SideStepStart)')
dec(A, 0x2A8D990, 'PlayerNetControl big update (interpolation at +3304)')
# PlayerNetState の vtable (GOT 0x5F3ED90) を読むコード = 送る / 受ける場所
for x in idautils.XrefsTo(0x5F3ED90): dec(A, x.frm, 'uses PlayerNetState vtable GOT')
callers(A, 0x27FCDC4, 'PlayerNetState ctor callers')
callers(A, 0x2A8D990, 'callers of PlayerNetControl update')
# 補間・ワープのしきい値
strrefs(A, r'^(sqrTeleportationThreshold|SyncPos|Interpolat|Extrapolat|NetPos|DeadReckoning|PosLerp|.*Teleport.*Threshold.*)$', 'SYNC')
strrefs(A, r'^spl::PlayerNetControl$', 'NETCTRL', 6)
strrefs(A, r'^spl::PlayerNetState(_Human|_Squid|_Jump)?$', 'NETSTATE', 6)

# ===== B) チーム =====
B = 'team'
dec(B, 0x14E0310, 'per-player team query (returns 0..4)')
callers(B, 0x14E0310, 'CALLER of 14E0310', 40)
dec(B, 0x32D10A0, 'TeamDataStore (10 x 88 bytes)')
callers(B, 0x32D10A0, 'CALLER of 32D10A0')
dec(B, 0x32CB7B0, 'team build')
callers(B, 0x32CB7B0, 'CALLER of 32CB7B0')
dec(B, 0x3899340, 'lobby obj, team index at +0x10F4')
callers(B, 0x3899340, 'CALLER of 3899340')
dec(B, 0x38978F0, 'FinalizeTeamData?')
dec(B, 0x3908910, 'sets v2[267..276]=3 (Charlie)')
callers(B, 0x3908910, 'CALLER of 3908910')
callers(B, 0x3908CE8, 'CALLER of 3908CE8 (v2 -> TeamChangeChild sync)')
strrefs(B, r'(ChangeTeam|SwapTeam|MoveTeam|AssignTeam|RequestTeam|TeamDecide|DecideTeam|TeamFix|FixTeam|TeamConfirm|ConfirmTeam|TeamSetting|TeamInfo|TeamId|TeamIdx)', 'TEAM')
strrefs(B, r'(InRoom, WaitForOpponent|LobbyMatching_SqTeam|same_team_match|PrivateMatch|PrivateRoom|RoomSetting|MatchSetting|VersusSetting)', 'ROOM', 3)

for g, fn in (('sync', 'sync6_funcs.txt'), ('team', 'team6_funcs.txt')):
    with open(os.path.join(OUT, fn), 'w', encoding='utf-8') as o:
        for ea, (tag, code) in sorted(groups[g].items()):
            o.write('\n\n######## %x  [%s]\n%s\n' % (ea, tag, code))
with open(os.path.join(OUT, 'tmt6_notes.txt'), 'w', encoding='utf-8') as o:
    o.write('\n'.join(notes))
print('tmt6 done: sync %d funcs, team %d funcs, %d notes' % (len(groups['sync']), len(groups['team']), len(notes)))
