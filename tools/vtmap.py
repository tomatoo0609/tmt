import sys; sys.path.insert(0,'/home/claude/tools')
from porter import *
O=Img(sys.argv[1],sys.argv[3]); N=Img(sys.argv[2],sys.argv[4])
D0,Dl=N.h['data']; R0=N.h['ro'][0]
nd=N.b[R0:N.dend//8*8].view(np.uint64)   # ro+data as u64 (aligned since R0 page aligned)
for s in sys.argv[5:]:
    a=int(s,16); ents=[O.u64(a+8*i) for i in range(12)]
    mapped=[]
    for i,e in enumerate(ents):
        if 0x1000<e<O.tl:
            na,sc,L,s2=text_map(O,N,e); mapped.append((i,na if sc>=L*0.8 else None,e))
        else: mapped.append((i,('raw',e),e))
    # anchor on first good text pointer
    anc=[(i,na) for i,na,e in mapped if isinstance(na,int)]
    if not anc: print(s,'no text ptrs',[hex(e) for e in ents[:6]]); continue
    i0,v0=anc[0]
    hits=np.nonzero(nd==v0)[0]
    res=[]
    for h in hits.tolist():
        cand=R0+h*8-8*i0; ok=0; tot=0
        for i,na,e in mapped:
            nv=N.u64(cand+8*i)
            if isinstance(na,int): tot+=1; ok+= nv==na
        res.append((ok,tot,cand))
    res.sort(reverse=True)
    print(s,'->',', '.join(f'{c:#x}({ok}/{t})' for ok,t,c in res[:3]), 'hits',len(hits))
