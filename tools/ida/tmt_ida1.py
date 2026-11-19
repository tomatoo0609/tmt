# tmt: IDA script 1 - decompile functions that reference recruit / private-match / notification strings
import idautils, idc, ida_funcs, ida_hexrays, os, re
OUT = r'C:\Users\takah\Music\tmt_out'
os.makedirs(OUT, exist_ok=True)
KEYS = re.compile(r'Recruit|PrivateMatch|Notification|Joinable|Hammer|Boshu|Invite|PlayerNameAlias|NameAlias', re.I)
def fdec(ea):
    f = ida_funcs.get_func(ea)
    if not f: return None, None
    try: return f.start_ea, str(ida_hexrays.decompile(f.start_ea))
    except Exception as e: return f.start_ea, 'DECOMPILE ERROR %s' % e
seen = {}; strhits = []
for s in idautils.Strings():
    t = str(s)
    if not KEYS.search(t) or len(t) > 200: continue
    refs = [x.frm for x in idautils.XrefsTo(s.ea)]
    strhits.append((s.ea, t, refs))
    for r in refs:
        fs, code = fdec(r)
        if fs is None or fs in seen: continue
        seen[fs] = (t, code)
    if len(seen) > 400: break
with open(os.path.join(OUT, 'strings.txt'), 'w', encoding='utf-8') as o:
    for ea, t, refs in strhits: o.write('%x\t%s\t%s\n' % (ea, t, ' '.join('%x' % r for r in refs)))
with open(os.path.join(OUT, 'funcs.txt'), 'w', encoding='utf-8') as o:
    for fs, (t, code) in sorted(seen.items()):
        o.write('\n\n######## %x  (string: %s)\n%s\n' % (fs, t, code))
print('tmt done', len(strhits), 'strings', len(seen), 'funcs')
