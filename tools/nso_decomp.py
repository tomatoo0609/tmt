import struct, sys, time
def lz4_block(src, out_size):
    out = bytearray(out_size); op=0; i=0; n=len(src)
    while i < n:
        tok=src[i]; i+=1
        lit=tok>>4
        if lit==15:
            while True:
                b=src[i]; i+=1; lit+=b
                if b!=255: break
        out[op:op+lit]=src[i:i+lit]; op+=lit; i+=lit
        if i>=n: break
        off=src[i]|(src[i+1]<<8); i+=2
        ml=tok&15
        if ml==15:
            while True:
                b=src[i]; i+=1; ml+=b
                if b!=255: break
        ml+=4
        s=op-off
        if off>=ml:
            out[op:op+ml]=out[s:s+ml]; op+=ml
        elif off==1:
            out[op:op+ml]=bytes([out[op-1]])*ml; op+=ml
        else:
            for j in range(ml):
                out[op]=out[op-off]; op+=1
    return out, op
def do(fn, outimg):
    d=open(fn,'rb').read()
    flags=struct.unpack_from('<I',d,0x0c)[0]
    def seg(o): return struct.unpack_from('<III',d,o)
    t=seg(0x10); r=seg(0x20); da=seg(0x30)
    tc,rc,dc=[struct.unpack_from('<I',d,0x60+4*k)[0] for k in range(3)]
    end=da[1]+da[2]
    img=bytearray(end)
    for (fo,mo,ds),cs in [(t,tc),(r,rc),(da,dc)]:
        comp=d[fo:fo+cs]
        dec,op=lz4_block(comp, ds)
        assert op==ds, (op,ds)
        img[mo:mo+ds]=dec
    open(outimg,'wb').write(img)
    print("wrote",outimg,"size",end, flush=True)
t0=time.time()
do(sys.argv[1], sys.argv[2])
print("done in %.1fs"%(time.time()-t0), flush=True)
