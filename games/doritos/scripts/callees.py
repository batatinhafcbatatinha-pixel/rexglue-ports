"""callees.py <decoded dir> <func>... : ordered `bl` targets inside each function (pdata extent)."""
import json, struct, sys
from pathlib import Path
d = Path(sys.argv[1]); img = (d/'image-mapped.bin').read_bytes()
base = int(json.load(open(d/'image-info.json'))['imageBase'], 16)
sizes = {int(f['address'],16): f['bytes'] for f in json.load(open(d/'pdata-functions.json'))}
for a in sys.argv[2:]:
    fs = int(a,16); size = sizes.get(fs, 0x400); out = []
    for off in range(fs - base, fs - base + size, 4):
        w = struct.unpack_from('>I', img, off)[0]
        if w >> 26 == 18 and (w & 3) == 1:
            disp = w & 0x03FFFFFC
            if disp & 0x02000000: disp -= 0x04000000
            out.append(f'{base+off+disp:08X}')
    print(f'{fs:08X} ({size}b): ' + ' '.join(out))
