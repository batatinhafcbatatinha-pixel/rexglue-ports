"""Code-built pointers (lis rX,hi ; addi/ori rY,rX,lo) to .text addresses that are not functions.

Prints candidate entries outside every .pdata span whose previous word is a terminal instruction
(blr/bctr/b/padding), i.e. the start of an undiscovered function. Feed them to function_extent.py.
"""
import bisect
import json
import re
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BASE, LO, HI = 0x82000000, 0x820A0000, 0x8248BCF4


def main():
    image = (ROOT / 'analysis/decoded/image-mapped.bin').read_bytes()
    word = lambda a: struct.unpack_from('>I', image, a - BASE)[0]
    register = (ROOT / 'project/generated/default/madagascar3_register.cpp').read_text()
    known = {int(x, 16) for x in re.findall(r'SetFunction\(0x([0-9A-Fa-f]+)', register)}
    pdata = json.loads((ROOT / 'analysis/decoded/pdata-functions.json').read_text())
    spans = sorted((int(p['address'], 16), int(p['address'], 16) + p['bytes']) for p in pdata)
    starts = [s for s, _ in spans]

    def inside_pdata(a):
        i = bisect.bisect_right(starts, a) - 1
        return i >= 0 and a < spans[i][1]

    def terminal(w):
        return w in (0, 0x4E800020, 0x4E800420) or (w >> 26 == 18 and not w & 1)

    found = {}
    for a in range(LO, HI - 4, 4):
        w = word(a)
        if w >> 26 != 15 or (w >> 16) & 31:  # lis rD,imm (addis rD,0,imm)
            continue
        rd, hi = (w >> 21) & 31, (w & 0xFFFF) << 16
        for k in range(1, 12):
            w2 = word(a + 4 * k)
            op = w2 >> 26
            if op in (14, 24) and (w2 >> 16) & 31 == rd:  # addi / ori with rA = rD
                lo = w2 & 0xFFFF
                target = (hi + (lo - 0x10000 if op == 14 and lo & 0x8000 else lo)) & 0xFFFFFFFF
                if LO <= target < HI and target % 4 == 0 and target not in known \
                        and not inside_pdata(target) and terminal(word(target - 4)) and word(target):
                    found.setdefault(target, []).append(a)
                break
            if (w2 >> 21) & 31 == rd and op not in (36, 37, 38, 44, 47, 54):  # rD overwritten
                break
    for t in sorted(found):
        print(f'{t:08X}', ' '.join(f'{r:08X}' for r in found[t][:4]))


if __name__ == '__main__':
    main()
