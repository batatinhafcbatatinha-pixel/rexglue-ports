"""Map struct displacements between two builds of the same library: aligns matched function pairs
instruction by instruction (difflib on mnemonic+base reg) and collects displacement pairs.
usage: offset_map.py <ref dir> <mine dir> <pairs file: 'refaddr mineaddr' per line> [min_disp]"""
import json, struct, sys, re, difflib
from collections import Counter, defaultdict
from pathlib import Path
from capstone import Cs, CS_ARCH_PPC, CS_MODE_64, CS_MODE_BIG_ENDIAN
md = Cs(CS_ARCH_PPC, CS_MODE_64 | CS_MODE_BIG_ENDIAN)
def load(d):
    d = Path(d); img = (d/'image-mapped.bin').read_bytes(); base = int(json.load(open(d/'image-info.json'))['imageBase'],16)
    sizes = {int(f['address'],16): f['bytes'] for f in json.load(open(d/'pdata-functions.json'))}
    return img, base, sizes
def insns(img, base, sizes, a):
    n = sizes.get(a, 0) or 0x400
    out = []
    for i in md.disasm(img[a-base:a-base+n], a):
        m = re.match(r'(.*?)(-?0x[0-9a-f]+|-?\d+)\((r\d+)\)$', i.op_str)
        if m:
            disp = int(m.group(2), 0)
            out.append((f'{i.mnemonic} {m.group(1)}@{m.group(3)}', disp))
        elif i.mnemonic in ('addi', 'li', 'cmplwi', 'cmpwi'):
            parts = i.op_str.split(', ')
            try: v = int(parts[-1], 0)
            except: v = None
            out.append((f'{i.mnemonic} {",".join(parts[:-1])}', v))
        else:
            out.append((f'{i.mnemonic} {i.op_str if i.mnemonic[0] != "b" else ""}', None))
    return out
R = load(sys.argv[1]); M = load(sys.argv[2])
mind = int(sys.argv[4], 0) if len(sys.argv) > 4 else 0x40
votes = defaultdict(Counter)
for line in open(sys.argv[3]):
    p = line.split()
    if len(p) < 2: continue
    ra, ma = int(p[0], 16), int(p[1], 16)
    a = insns(*R, ra); b = insns(*M, ma)
    sm = difflib.SequenceMatcher(None, [x[0] for x in a], [x[0] for x in b], autojunk=False)
    for blk in sm.get_matching_blocks():
        for k in range(blk.size):
            x, y = a[blk.a+k], b[blk.b+k]
            if x[1] is not None and y[1] is not None and abs(x[1]) >= mind and '@r1' not in x[0]:
                votes[x[1]][y[1]] += 1
for d in sorted(votes):
    c = votes[d].most_common(3)
    print(f'{d:#06x} -> ' + '  '.join(f'{v:#06x}x{n}' for v, n in c))
