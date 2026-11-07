"""Find functions of another ReXGlue title (same XDK D3D library) in Madagascar 3 by instruction shape.

usage: match_functions.py <other image-mapped.bin> <addr> [addr ...]
Branch displacements are masked; everything else (registers, struct offsets) must match.
Prints the best Madagascar candidates (function starts from .pdata) with their score.
"""
import json
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BASE = 0x82000000
N = 48


def words(img, addr, n=N):
    off = addr - BASE
    return list(struct.unpack_from(f'>{n}I', img, off))


def mask(w):
    op = w >> 26
    if op == 18:            # b/bl
        return w & 0xFC000003
    if op == 16:            # bc
        return w & 0xFFFF0003
    return w


def score(a, b):
    return sum(1 for x, y in zip(a, b) if mask(x) == mask(y))


def main():
    other = Path(sys.argv[1]).read_bytes()
    mine = (ROOT / 'analysis/decoded/image-mapped.bin').read_bytes()
    funcs = [int(f['address'], 16) for f in json.load(open(ROOT / 'analysis/decoded/pdata-functions.json'))
             if f['bytes'] >= 16]
    cache = {f: words(mine, f) for f in funcs}
    for spec in sys.argv[2:]:
        a = int(spec, 16)
        ref = words(other, a)
        best = sorted(((score(ref, w), f) for f, w in cache.items()), reverse=True)[:3]
        print(f'{a:08X} -> ' + '  '.join(f'{f:08X}({s}/{N})' for s, f in best))


if __name__ == '__main__':
    main()
