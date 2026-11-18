import sys, subprocess, struct, re
MC='llvm-mc-18' if subprocess.run(['which','llvm-mc-18'],capture_output=True).returncode==0 else 'llvm-mc'
def dis(img, a, n):
    with open(img,'rb') as f: f.seek(a); b=f.read(4*n)
    words=struct.unpack(f'<{n}I',b); r=[]
    src=' '.join(f'0x{x&0xff:02x} 0x{(x>>8)&0xff:02x} 0x{(x>>16)&0xff:02x} 0x{x>>24:02x}\n' for x in words)
    out=subprocess.run([MC,'--disassemble','-triple=aarch64','-mattr=+v8.2a,+crypto,+fullfp16'],input=src,capture_output=True,text=True).stdout
    lines=[l.strip() for l in out.split('\n') if l.strip() and not l.strip().startswith('.text')]
    # llvm-mc prints one line per word (invalid ones produce warnings on stderr only); pad
    return [(a+4*i, lines[i] if i<len(lines) else '?') for i in range(n)], words
if __name__=='__main__':
    (o,ow),(n,nw)=dis(sys.argv[1],int(sys.argv[3],16)-4*int(sys.argv[6]),int(sys.argv[5])),dis(sys.argv[2],int(sys.argv[4],16)-4*int(sys.argv[6]),int(sys.argv[5]))
    for i,((oa,oi),(na,ni)) in enumerate(zip(o,n)):
        if ow[i]!=nw[i]:
            mn=oi.split()[0] if oi.split() else ''
            if mn in ('bl','b','adrp','adr') and '-v' not in sys.argv: continue
            print(f'{oa:#x}: {oi:42s} | {na:#x}: {ni}')
