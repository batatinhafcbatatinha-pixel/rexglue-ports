"""callers.py <decoded dir> <addr>... : functions (from pdata) containing a `bl` to each address."""
import json, struct, sys, bisect
from pathlib import Path
d = Path(sys.argv[1]); img = (d/'image-mapped.bin').read_bytes()
base = int(json.load(open(d/'image-info.json'))['imageBase'], 16)
funcs = sorted((int(f['address'],16), f['bytes']) for f in json.load(open(d/'pdata-functions.json')))
starts = [f[0] for f in funcs]
targets = {int(a,16): [] for a in sys.argv[2:]}
for fs, size in funcs:
    for off in range(fs - base, fs - base + size, 4):
        w = struct.unpack_from('>I', img, off)[0]
        if w >> 26 == 18 and (w & 3) == 1:
            disp = w & 0x03FFFFFC
            if disp & 0x02000000: disp -= 0x04000000
            t = base + off + disp
            if t in targets: targets[t].append((fs, base + off))
for t, l in targets.items():
    print(f'{t:08X}: ' + ' '.join(f'{f:08X}@{s:08X}' for f, s in l[:20]))
