import sys, struct, subprocess
def load(img):
    b=open(img,'rb').read()
    rel={}
    for l in subprocess.run([sys.executable,'-I','/home/claude/tools/dynsym.py',img,'plt'],capture_output=True,text=True).stdout.split('\n'):
        if l: o,n=l.split(' ',1); rel[int(o,16)]=n
    return b,rel
def stub(b,rel,a):
    w0,w1=struct.unpack_from('<II',b,a)
    if (w0&0x9F000000)!=0x90000000: return None
    imm=((w0>>29)&3)|(((w0>>5)&0x7FFFF)<<2)
    if imm&(1<<20): imm-=1<<21
    page=(a&~0xFFF)+(imm<<12)
    got=page+(((w1>>10)&0xFFF)<<3)
    return rel.get(got)
ob,orel=load(sys.argv[1]); nb,nrel=load(sys.argv[2])
# index new stubs by name: scan region near end of text
tl=struct.unpack_from('<I',open(sys.argv[4],'rb').read(0x20),0x18)[0]
byname={}
for a in range(tl-0x8000,tl,4):
    n=stub(nb,nrel,a)
    if n and n not in byname: byname[n]=a
for s in sys.argv[5:]:
    a=int(s,16); n=stub(ob,orel,a); print(hex(a),n,'->',hex(byname[n]) if n in byname else None)
