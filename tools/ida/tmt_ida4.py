# tmt: IDA script 4 - private match team handling: functions around the "either team" patch points, their callers, and team-related strings
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
PATCH = [0x38997D8, 0x32CBF20, 0x32CC030, 0x32CD070]
for p in PATCH:
    fs = dec(p, 'PATCH %x' % p)
    if fs is None: continue
    callers = [x.frm for x in idautils.XrefsTo(fs)]
    notes.append('%x in func %x  callers: %s' % (p, fs, ' '.join('%x' % c for c in callers[:20])))
    for c in callers[:12]: dec(c, 'CALLER of %x' % fs)
KEYS = re.compile(r'(Team|Alpha|Bravo|Charlie|Spectat|Kansen|Member|Slot|Entry).*(Private|Room|Lobby|Match)|(Private|Room|Lobby).*(Team|Member|Slot|Spectat)|PrivateMatch', re.I)
for s in idautils.Strings():
    t = str(s)
    if len(t) > 120 or not KEYS.search(t): continue
    refs = [x.frm for x in idautils.XrefsTo(s.ea)]
    notes.append('STR %x %s <- %s' % (s.ea, t, ' '.join('%x' % r for r in refs[:10])))
    for r in refs[:4]: dec(r, 'STR ' + t)
    if len(done) > 220: break
with open(os.path.join(OUT, 'team_notes.txt'), 'w', encoding='utf-8') as o: o.write('\n'.join(notes))
with open(os.path.join(OUT, 'team_funcs.txt'), 'w', encoding='utf-8') as o:
    for ea, (tag, code) in sorted(done.items()): o.write('\n\n######## %x  [%s]\n%s\n' % (ea, tag, code))
print('tmt4 done', len(done))
