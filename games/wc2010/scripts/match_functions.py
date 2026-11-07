"""Find functions of another title's image (same XDK library) in this game's image by instruction shape.

usage: match_functions.py <ref decoded dir> <mine decoded dir> <addr> [addr ...]
Branch displacements masked; registers/offsets must match. Prints best candidates (from .pdata).
"""
import json, struct, sys
from pathlib import Path
N = 48

def load(d):
    d = Path(d)
    img = (d / 'image-mapped.bin').read_bytes()
    base = int(json.load(open(d / 'image-info.json'))['imageBase'], 16)
    funcs = [(int(f['address'], 16), f['bytes']) for f in json.load(open(d / 'pdata-functions.json'))]
    return img, base, funcs

def words(img, base, addr, n=N):
    off = addr - base
    n = min(n, (len(img) - off) // 4)
    return list(struct.unpack_from(f'>{n}I', img, off))

def mask(w):
    op = w >> 26
    if op == 18: return w & 0xFC000003
    if op == 16: return w & 0xFFFF0003
    return w

def main():
    rimg, rbase, rfuncs = load(sys.argv[1])
    mimg, mbase, mfuncs = load(sys.argv[2])
    rsize = dict(rfuncs)
    cache = {a: [mask(w) for w in words(mimg, mbase, a, min(N, max(4, s // 4)))] for a, s in mfuncs if s >= 8}
    msize = dict(mfuncs)
    for t in sys.argv[3:]:
        a = int(t, 16)
        n = min(N, max(4, rsize.get(a, N * 4) // 4))
        ref = [mask(w) for w in words(rimg, rbase, a, n)]
        res = sorted(((sum(1 for x, y in zip(ref, c) if x == y) / len(ref), f) for f, c in cache.items()), reverse=True)[:3]
        print(f'{a:08X} ({rsize.get(a,0)}b): ' + '  '.join(f'{f:08X}({msize[f]}b) {s:.2f}' for s, f in res))

if __name__ == '__main__':
    main()
