import struct
def vint_dummy():pass
class W:
    def __init__(s):pass
def build(root):
    # collect keys and strings
    keys=set();strs=set()
    def walk(n):
        if isinstance(n,dict):
            for k,v in n.items():keys.add(k);walk(v)
        elif isinstance(n,list):
            for v in n:walk(v)
        elif isinstance(n,str):strs.add(n)
    walk(root)
    K=sorted(keys);S=sorted(strs)
    out=bytearray(16)
    def table(L):
        n=len(L);pos=4+4*(n+1);b=bytearray(b'\xc2'+n.to_bytes(3,'little'));offs=[]
        body=b''
        for x in L:
            offs.append(pos+len(body));body+=x.encode()+b'\0'
        offs.append(pos+len(body))
        b+=struct.pack('<%dI'%(n+1),*offs)+body
        while len(b)%4:b+=b'\0'
        return b
    kt=len(out);out+=table(K);st=len(out);out+=table(S)
    cache={}
    def put(n):
        # returns (type,val)
        if isinstance(n,bool):return 0xd0,int(n)
        if isinstance(n,int):return (0xd1,n&0xffffffff)
        if isinstance(n,float):return 0xd2,struct.unpack('<I',struct.pack('<f',n))[0]
        if isinstance(n,str):return 0xa0,S.index(n)
        if isinstance(n,list):
            ch=[put(x) for x in n]
            key=('a',tuple(ch))
            if key in cache:return 0xc0,cache[key]
            while len(out)%4:out.append(0)
            off=len(out);m=len(n);out.extend(b'\xc0'+m.to_bytes(3,'little'))
            out.extend(bytes(t for t,v in ch));
            while len(out)%4:out.append(0)
            for t,v in ch:out.extend(struct.pack('<I',v))
            cache[key]=off;return 0xc0,off
        if isinstance(n,dict):
            items=sorted(n.items(),key=lambda kv:K.index(kv[0]))
            ch=[(K.index(k),)+put(v) for k,v in items]
            key=('h',tuple(ch))
            if key in cache:return 0xc1,cache[key]
            while len(out)%4:out.append(0)
            off=len(out);out.extend(b'\xc1'+len(ch).to_bytes(3,'little'))
            for k,t,v in ch:out.extend(k.to_bytes(3,'little')+bytes([t])+struct.pack('<I',v))
            cache[key]=off;return 0xc1,off
        raise Exception(type(n))
    t,r=put(root)
    out[0:2]=b'YB';out[2:4]=struct.pack('<H',1);out[4:16]=struct.pack('<III',kt,st,r)
    return bytes(out)
def parse(b):
    kt,st,ro=struct.unpack_from('<III',b,4)
    def tab(o):
        n=int.from_bytes(b[o+1:o+4],'little');offs=struct.unpack_from('<%dI'%(n+1),b,o+4)
        return [b[o+offs[i]:b.index(b'\0',o+offs[i])].decode() for i in range(n)]
    K=tab(kt);S=tab(st)
    assert K==sorted(K),'key table not sorted'; assert S==sorted(S),'string table not sorted'
    def node(t,v):
        if t==0xa0:return S[v]
        if t==0xd0:return bool(v)
        if t==0xd1:return struct.unpack('<i',struct.pack('<I',v))[0]
        if t==0xd3:return v
        if t==0xd2:return round(struct.unpack('<f',struct.pack('<I',v))[0],4)
        if t==0xff:return None
        if t==0xc0:
            n=int.from_bytes(b[v+1:v+4],'little');ts=b[v+4:v+4+n];p=v+4+((n+3)&~3)
            return [node(ts[i],struct.unpack_from('<I',b,p+4*i)[0]) for i in range(n)]
        if t==0xc1:
            n=int.from_bytes(b[v+1:v+4],'little');o={};last=-1
            for i in range(n):
                q=v+4+8*i;k=int.from_bytes(b[q:q+3],'little');assert k>last,'hash unsorted';last=k
                o[K[k]]=node(b[q+3],struct.unpack_from('<I',b,q+4)[0])
            return o
        raise Exception(hex(t))
    return node(0xc1,ro),K,S
