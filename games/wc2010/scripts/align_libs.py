"""Align the function sequences of the same static library in two images (Needleman-Wunsch on
masked instruction similarity). usage: align_libs.py <ref dir> <ref lo> <ref hi> <mine dir> <lo> <hi> [addr...]"""
import json, struct, sys
from pathlib import Path
N = 40
def load(d, lo, hi):
    d = Path(d); img = (d/'image-mapped.bin').read_bytes(); base = int(json.load(open(d/'image-info.json'))['imageBase'],16)
    fs = sorted((int(f['address'],16), f['bytes']) for f in json.load(open(d/'pdata-functions.json')))
    fs = [(a, s) for a, s in fs if lo <= a < hi]
    def sig(a, s):
        n = max(1, min(N, s//4)); o = a-base
        ws = struct.unpack_from(f'>{n}I', img, o)
        out = []
        for w in ws:
            op = w >> 26
            out.append(w & 0xFC000003 if op == 18 else w & 0xFFFF0003 if op == 16 else w)
        return out
    return [(a, s, sig(a, s)) for a, s in fs]
def sim(x, y):
    a, b = x[2], y[2]
    m = sum(1 for p, q in zip(a, b) if p == q) / max(len(a), len(b))
    sz = min(x[1], y[1]) / max(x[1], y[1], 1)
    return 0.7*m + 0.3*sz
r = load(sys.argv[1], int(sys.argv[2],16), int(sys.argv[3],16))
m = load(sys.argv[4], int(sys.argv[5],16), int(sys.argv[6],16))
G = -0.05
n, k = len(r), len(m)
S = [[0.0]*(k+1) for _ in range(n+1)]; B = [[0]*(k+1) for _ in range(n+1)]
for i in range(1, n+1): S[i][0] = i*G; B[i][0] = 1
for j in range(1, k+1): S[0][j] = j*G; B[0][j] = 2
for i in range(1, n+1):
    for j in range(1, k+1):
        d = S[i-1][j-1] + sim(r[i-1], m[j-1]) - 0.35
        u = S[i-1][j] + G; l = S[i][j-1] + G
        S[i][j], B[i][j] = max((d,0),(u,1),(l,2))
pairs = {}; i, j = n, k
while i and j:
    if B[i][j] == 0: pairs[r[i-1][0]] = (m[j-1][0], m[j-1][1], sim(r[i-1], m[j-1])); i -= 1; j -= 1
    elif B[i][j] == 1: i -= 1
    else: j -= 1
targets = [int(t,16) for t in sys.argv[7:]] or [a for a,_,_ in r]
rs = {a: s for a, s, _ in r}
for t in targets:
    p = pairs.get(t)
    print(f'{t:08X} ({rs.get(t,0)}b) -> ' + (f'{p[0]:08X} ({p[1]}b) {p[2]:.2f}' if p else 'unmatched'))
