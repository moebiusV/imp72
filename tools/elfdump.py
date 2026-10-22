import struct,sys
def dump(path, want="retguard"):
    d=open(path,'rb').read()
    shoff,=struct.unpack_from('<Q',d,0x28); shentsize,shnum,shstrndx=struct.unpack_from('<HHH',d,0x3A)
    secs=[]
    for i in range(shnum):
        name,typ,flags,addr,off,size,link,info,align,entsize=struct.unpack_from('<IIQQQQIIQQ',d,shoff+i*shentsize)
        secs.append(dict(i=i,name=name,type=typ,flags=flags,off=off,size=size,link=link,info=info,entsize=entsize))
    sstr=secs[shstrndx]
    def nm(strsec,o):
        s=d[strsec['off']+o:]; return s[:s.index(b'\0')].decode()
    for s in secs: s['n']=nm(sstr,s['name'])
    syms=None
    for s in secs:
        if s['type']==2: # SYMTAB
            strsec=secs[s['link']]; syms=[]
            for k in range(s['size']//24):
                n,info,other,shndx,val,sz=struct.unpack_from('<IBBHQQ',d,s['off']+k*24)
                syms.append((k,nm(strsec,n),info>>4,info&15,other&3,shndx,val,sz))
    tn={1:'PROGBITS',2:'SYMTAB',3:'STRTAB',4:'RELA',9:'REL',17:'GROUP',0:'NULL',8:'NOBITS'}
    out=[]
    n=0
    for s in secs:
        if want in s['n'] or s['type']==17:
            if n<6:
                extra=''
                if s['type']==17:
                    mem=struct.unpack_from('<%dI'%(s['size']//4),d,s['off'])
                    sig=syms[s['info']] if syms and s['info']<len(syms) else None
                    extra=' groupflags=%x members=%s signature_sym[%d]=%s'%(mem[0],[secs[m]['n'] for m in mem[1:]],s['info'],sig[1] if sig else None)
                out.append('sec%d %s type=%s flags=0x%x link=%d info=%d%s'%(s['i'],s['n'],tn.get(s['type'],s['type']),s['flags'],s['link'],s['info'],extra))
            n+=1
    if syms:
        m=0
        for k,name,bind,typ,vis,shndx,val,sz in syms:
            if want in name and typ!=3 and m<4:
                out.append('sym[%d] %s bind=%d type=%d vis=%d shndx=%d(%s) val=%d size=%d'%(k,name,bind,typ,vis,shndx,secs[shndx]['n'] if shndx<len(secs) else '?',val,sz)); m+=1
    return '\n'.join(out)
for p in sys.argv[1:]:
    print('== '+p); print(dump(p))
