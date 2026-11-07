"""Disassemble a function of a decoded image. usage: dis.py <decoded dir> <addr> [count] ; or
dis.py --cmp <ref dir> <ref addr> <mine dir> <mine addr> [count]  (side by side)"""
import json, sys
from pathlib import Path
from capstone import Cs, CS_ARCH_PPC, CS_MODE_64, CS_MODE_BIG_ENDIAN
md = Cs(CS_ARCH_PPC, CS_MODE_64 | CS_MODE_BIG_ENDIAN)
def img(d):
    d = Path(d); return (d/'image-mapped.bin').read_bytes(), int(json.load(open(d/'image-info.json'))['imageBase'],16)
def size(d, a):
    for f in json.load(open(Path(d)/'pdata-functions.json')):
        if int(f['address'],16) == a: return f['bytes']
    return 0
def lines(d, a, n):
    b, base = img(d)
    if not n: n = size(d, a)//4 or 64
    out=[]
    for k in range(n):
        o=a-base+4*k; w=b[o:o+4]
        ins=list(md.disasm(w, a+4*k))
        out.append(f'{a+4*k:08X} {w.hex()} ' + (f'{ins[0].mnemonic} {ins[0].op_str}' if ins else '.long'))
    return out
if sys.argv[1]=='--cmp':
    r=lines(sys.argv[2],int(sys.argv[3],16),int(sys.argv[6]) if len(sys.argv)>6 else 0)
    m=lines(sys.argv[4],int(sys.argv[5],16),len(r))
    for x,y in zip(r,m):
        flag = ' ' if x.split(' ',2)[2].split('0x')[0]==y.split(' ',2)[2].split('0x')[0] and x[18:]==y[18:] else '*'
        print(f'{flag} {x:<60} | {y}')
else:
    print('\n'.join(lines(sys.argv[1],int(sys.argv[2],16),int(sys.argv[3]) if len(sys.argv)>3 else 0)))
