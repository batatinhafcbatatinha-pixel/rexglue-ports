"""offset_pairs.py <ref dir> <mine dir> refA:mineA ... : align same-shape functions word by word and report
D-form displacement differences (same opcode/registers): ref offset -> mine offset, with counts."""
import json, struct, sys, collections
from pathlib import Path
def load(d):
    d = Path(d); img = (d/'image-mapped.bin').read_bytes()
    base = int(json.load(open(d/'image-info.json'))['imageBase'], 16)
    sizes = {int(f['address'],16): f['bytes'] for f in json.load(open(d/'pdata-functions.json'))}
    return img, base, sizes
R = load(sys.argv[1]); M = load(sys.argv[2])
pairs = collections.Counter(); same = collections.Counter()
DFORM = set(range(32, 56)) | {14, 58, 62}   # loads/stores, addi(14), ld/std
for spec in sys.argv[3:]:
    a, b = (int(x, 16) for x in spec.split(':'))
    n = min(R[2].get(a, 0x200), M[2].get(b, 0x200)) // 4
    for i in range(n):
        w1 = struct.unpack_from('>I', R[0], a - R[1] + 4*i)[0]; w2 = struct.unpack_from('>I', M[0], b - M[1] + 4*i)[0]
        if w1 >> 16 != w2 >> 16 or (w1 >> 26) not in DFORM: continue
        d1, d2 = w1 & 0xFFFF, w2 & 0xFFFF
        if d1 == d2: same[d1] += 1
        else: pairs[(d1, d2)] += 1
for (d1, d2), c in sorted(pairs.items()):
    print(f'{d1:04X} -> {d2:04X}  x{c}')
print('identical offsets:', ' '.join(f'{d:04X}' for d in sorted(same) if d > 0x100))
