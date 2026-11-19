# tmt: IDA script 5 - deep team assignment analysis
# Targets: sub_14E0310 (player state check per slot), sub_32D10A0 (team data store),
# sub_32CB7B0 (team build), callers of 32D10A0, and the team assignment write-back path.
# Also look for functions that WRITE team values and any "team change" request senders.
import idautils, idc, ida_funcs, ida_hexrays, os, re
OUT = r'C:\Users\takah\Music\tmt_out'
done = {}; notes = []

def dec(ea, tag=''):
    f = ida_funcs.get_func(ea)
    if not f: return None
    if f.start_ea not in done:
        try: done[f.start_ea] = [tag, str(ida_hexrays.decompile(f.start_ea))]
        except Exception as e: done[f.start_ea] = [tag, 'ERR %s' % e]
    return f.start_ea

# 1) sub_14E0310 - called in the player loop (v6 = sub_14E0310(v4, 0))
dec(0x14E0310, 'PlayerSlotState')
# Get its callers
for x in idautils.XrefsTo(0x14E0310):
    dec(x.frm, 'CALLER of 14E0310')

# 2) sub_32D10A0 - stores per-player team result
dec(0x32D10A0, 'TeamDataStore')
for x in idautils.XrefsTo(0x32D10A0):
    dec(x.frm, 'CALLER of 32D10A0')
    notes.append('32D10A0 called from %x' % x.frm)

# 3) sub_32CB7B0 already decompiled but get its callers too
for x in idautils.XrefsTo(0x32CB7B0):
    dec(x.frm, 'CALLER of 32CB7B0')
    notes.append('32CB7B0 called from %x' % x.frm)

# 4) sub_14FB0A0 - called in 3899340 for spectator list building
dec(0x14FB0A0, 'SpectatorListBuild')

# 5) The field a1+4340+4*v4 stores per-player team index - find who writes to that area
# a1 in 3899340 is the lobby object; offset 4340 = 0x10F4
# Look for xrefs to the struct at that offset range
# We'll search for instructions that write to offsets 0x10F4-0x111C (4340 to 4340+40)
notes.append('--- Searching for team index writes at offset 0x10F4-0x111C ---')

# 6) Look for team change request strings
TEAM_KEYS = re.compile(r'(ChangeTeam|SwapTeam|MoveTeam|SetTeam|team_change|teamChange|AssignTeam|RequestTeam)', re.I)
for s in idautils.Strings():
    t = str(s)
    if len(t) > 150: continue
    if TEAM_KEYS.search(t):
        refs = [x.frm for x in idautils.XrefsTo(s.ea)]
        notes.append('TEAM_STR %x %s <- %s' % (s.ea, t, ' '.join('%x' % r for r in refs[:10])))
        for r in refs[:4]: dec(r, 'TEAM_STR ' + t)

# 7) Look for "LobbyMatching_SqTeam" refs and decompile
for s in idautils.Strings():
    t = str(s)
    if t == 'LobbyMatching_SqTeam':
        refs = [x.frm for x in idautils.XrefsTo(s.ea)]
        notes.append('SqTeam STR %x refs: %s' % (s.ea, ' '.join('%x' % r for r in refs[:20])))
        for r in refs[:6]: dec(r, 'SqTeam ref')
        break

# 8) Look for "same_team_match" refs
for s in idautils.Strings():
    t = str(s)
    if 'same_team_match' in t:
        refs = [x.frm for x in idautils.XrefsTo(s.ea)]
        notes.append('same_team STR %x refs: %s' % (s.ea, ' '.join('%x' % r for r in refs[:20])))
        for r in refs[:6]: dec(r, 'same_team ref')
        break

# 9) Look for the lobby state manager - "InRoom, WaitForOpponent, InGame, Spectator"
for s in idautils.Strings():
    t = str(s)
    if 'InRoom, WaitForOpponent' in t:
        refs = [x.frm for x in idautils.XrefsTo(s.ea)]
        notes.append('LobbyState STR %x refs: %s' % (s.ea, ' '.join('%x' % r for r in refs[:20])))
        for r in refs[:6]: dec(r, 'LobbyState ref')
        break

# 10) sub_38978F0 - called at end of 3899340, likely finalizes team data
dec(0x38978F0, 'FinalizeTeamData')

# 11) The pchtxt patches: look at exact patch addresses and their context
# Patch 0x38997D8: in func 0x3899340 - check what instruction is there
notes.append('--- Patch point instructions ---')
for addr in [0x38997D8, 0x32CBF20, 0x32CC030, 0x32CD070]:
    insn = idc.GetDisasm(addr)
    notes.append('PATCH %x: %s' % (addr, insn))
    # Also get a few instructions around it
    for off in [-8, -4, 4, 8]:
        a2 = addr + off
        notes.append('  %x: %s' % (a2, idc.GetDisasm(a2)))

if len(done) > 300:
    notes.append('WARNING: too many functions, stopping early')

with open(os.path.join(OUT, 'team5_notes.txt'), 'w', encoding='utf-8') as o:
    o.write('\n'.join(notes))
with open(os.path.join(OUT, 'team5_funcs.txt'), 'w', encoding='utf-8') as o:
    for ea, (tag, code) in sorted(done.items()):
        o.write('\n\n######## %x  [%s]\n%s\n' % (ea, tag, code))
print('tmt5 done: %d funcs, %d notes' % (len(done), len(notes)))
