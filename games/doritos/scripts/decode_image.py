"""Writes analysis/decoded/{sections.tsv,pdata-functions.json} from the image-mapped base PE
(D:/CodexBuilds/doritos/default-pe.bin, from XexTool). image-mapped.bin stays outside the repo."""
import json, struct
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
PE = Path(r'D:\CodexBuilds\doritos\default-pe.bin')
BASE = 0x82000000
d = PE.read_bytes()
pe = struct.unpack_from('<I', d, 0x3C)[0]
nsec = struct.unpack_from('<H', d, pe + 6)[0]
optsz = struct.unpack_from('<H', d, pe + 20)[0]
sec = pe + 24 + optsz
rows, pdata = ['name\taddress\tbytes\tcode'], None
for i in range(nsec):
    o = sec + 40 * i
    name = d[o:o + 8].rstrip(b'\0').decode()
    vsize, va = struct.unpack_from('<II', d, o + 8)
    chars = struct.unpack_from('<I', d, o + 36)[0]
    rows.append(f'{name}\t0x{BASE + va:08X}\t{vsize}\t{1 if chars & 0x20 else 0}')
    if name == '.pdata':
        pdata = (va, vsize)
out = ROOT / 'analysis/decoded'
out.mkdir(parents=True, exist_ok=True)
(out / 'sections.tsv').write_text('\n'.join(rows) + '\n')
funcs = []
for o in range(pdata[0], pdata[0] + pdata[1] - 7, 8):
    begin, packed = struct.unpack_from('>II', d, o)
    if not begin:
        continue
    funcs.append(dict(address=f'0x{begin:08X}', bytes=((packed >> 8) & 0x3FFFFF) * 4,
                      prologBytes=(packed & 0xFF) * 4, exceptionFlag=bool(packed >> 31)))
(out / 'pdata-functions.json').write_text(json.dumps(funcs, indent=1) + '\n')
print(len(funcs), 'pdata entries;', rows[1:])
