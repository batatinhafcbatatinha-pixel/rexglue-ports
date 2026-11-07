"""Bounded control-flow extent of PPC functions that the codegen did not discover.

usage: function_extent.py <decoded dir> <hex address> [...]
Like recover_indirect_entries.walk, but VMX128 opcodes (primary 4/5/6, which capstone does not
decode) are accepted as straight-line instructions. Prints manifest lines `0xSTART = { end = 0xEND }`.
"""
import bisect
import json
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main():
    d = Path(sys.argv[1])
    image = (d / 'image-mapped.bin').read_bytes()
    BASE = int(json.loads((d / 'image-info.json').read_text())['imageBase'], 16)
    word = lambda a: struct.unpack_from('>I', image, a - BASE)[0]
    pdata = json.loads((d / 'pdata-functions.json').read_text())
    known = {int(p['address'], 16) for p in pdata}
    for spec in sys.argv[2:]:
        known.add(int(spec, 16))
    anchors = sorted(known)
    for spec in sys.argv[2:]:
        start = int(spec, 16)
        i = bisect.bisect_right(anchors, start)
        ceiling = min(start + 0x4000, anchors[i] if i < len(anchors) else start + 0x4000)
        pending, visited, problem = [start], set(), None
        while pending and not problem:
            a = pending.pop()
            if a in visited:
                continue
            if not start <= a < ceiling:
                problem = f'flow leaves span at {a:08X}'
                break
            w = word(a)
            if not w:
                problem = f'padding at {a:08X}'
                break
            visited.add(a)
            op = w >> 26
            if w in (0x4E800020, 0x4E800420):  # blr, bctr
                continue
            if op == 18:
                d = w & 0x3FFFFFC
                d -= 0x4000000 if d & 0x2000000 else 0
                target = (d if w & 2 else a + d) & 0xFFFFFFFF
                if w & 1:
                    pending.append(a + 4)
                elif start <= target < ceiling:
                    pending.append(target)
                elif target not in known:
                    problem = f'tail branch {a:08X} -> unknown {target:08X}'
                continue
            if op == 16:
                d = w & 0xFFFC
                d -= 0x10000 if d & 0x8000 else 0
                target = (d if w & 2 else a + d) & 0xFFFFFFFF
                pending.append(a + 4)
                if not w & 1:
                    if start <= target < ceiling:
                        pending.append(target)
                    elif target not in known:  # a conditional branch to a known entry is a tail call
                        problem = f'conditional {a:08X} -> outside {target:08X}'
                continue
            if op == 19 and ((w >> 1) & 1023) in (16, 528) and not w & 1 and ((w >> 21) & 31) == 20:
                continue
            pending.append(a + 4)
        if problem:
            print(f'# 0x{start:08X}: {problem} (ceiling {ceiling:08X})')
        else:
            print(f'0x{start:08X} = {{ end = 0x{max(visited) + 4:08X} }}')


if __name__ == '__main__':
    main()
