"""Find code addresses referenced as data (vtables, init tables, lis/addi pairs) that
ReXGlue did not register as function entries. Prints manifest lines.

usage: find_missing_functions.py <generated/default dir> [--manifest out.toml]
"""
import re
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import struct
import sys
from bisect import bisect_right
from pathlib import Path

from s4img import IMG, BASE, TEXT_LO, TEXT_HI, word
# .rdata and .data (skip .pdata and .reloc)
DATA_RANGES = [(0x82300400, 0x82300400 + 2972564), (0x838E0000, 0x838E0000 + 2149088)]




def is_terminator(w):
    if w == 0x4E800020 or w == 0x4E800420:  # blr, bctr
        return True
    if (w >> 26) == 18 and not (w & 1):  # b (no link)
        return True
    if w == 0:  # padding
        return True
    return False


def plausible_start(a):
    w = word(a)
    if w == 0 or (w >> 26) == 0:
        return False
    return is_terminator(word(a - 4))


def main():
    gen = Path(sys.argv[1])
    known = set(int(m, 16) for m in re.findall(r"SetFunction\(0x([0-9A-F]+)", (gen / "wc2010_register.cpp").read_text()))
    known_sorted = sorted(known)
    refs = {}
    for lo, hi in DATA_RANGES:
        for a in range(lo, hi - 3, 4):
            v = word(a)
            if TEXT_LO <= v < TEXT_HI and not (v & 3):
                refs.setdefault(v, []).append(("data", a))
    # lis rX,hi ; addi rY,rX,lo  (within next 8 instructions)
    for a in range(TEXT_LO, TEXT_HI - 4, 4):
        w = word(a)
        if (w >> 26) == 15 and ((w >> 16) & 31) == 0:  # lis
            rd = (w >> 21) & 31
            hi = (w & 0xFFFF) << 16
            for k in range(1, 9):
                w2 = word(a + 4 * k)
                op = w2 >> 26
                if op == 14 and ((w2 >> 16) & 31) == rd:  # addi rY, rd, lo
                    v = (hi + struct.unpack(">h", struct.pack(">H", w2 & 0xFFFF))[0]) & 0xFFFFFFFF
                    if TEXT_LO <= v < TEXT_HI and not (v & 3):
                        refs.setdefault(v, []).append(("code", a))
                    break
                if ((w2 >> 21) & 31) == rd and op not in (36, 37, 38, 44, 45, 62, 54, 52, 55, 53):
                    break  # rd overwritten (not a store using rd as source)
    def owner(a):
        i = bisect_right(known_sorted, a) - 1
        return known_sorted[i] if i >= 0 else None

    def branched_internally(v):
        lo = owner(v)
        if lo is None:
            return False
        i = bisect_right(known_sorted, v)
        hi = known_sorted[i] if i < len(known_sorted) else TEXT_HI
        for a in range(lo, hi, 4):
            w = word(a)
            op = w >> 26
            if op == 18 and not (w & 3):
                li = w & 0x03FFFFFC
                if li & 0x02000000:
                    li -= 0x04000000
                if a + li == v:
                    return True
            elif op == 16 and not (w & 3):
                bd = w & 0xFFFC
                if bd & 0x8000:
                    bd -= 0x10000
                if a + bd == v:
                    return True
        return False

    def scope_target(x):
        b, e, h = word(x - 12), word(x - 8), word(x - 4)
        return (all(TEXT_LO <= t < TEXT_HI for t in (b, e, h)) and b < e
                and b not in known and e not in known)

    def in_fn_table(v):
        # referenced from a table whose neighbours are known function entries
        for k, r in refs[v]:
            if k == "data" and any(word(r + d) in known for d in (-4, 4)):
                return True
        return False

    def keep(v):
        if v in known:
            return False
        if not plausible_start(v) and not (word(v) and in_fn_table(v)):
            return False
        if all(k == "data" and scope_target(r) for k, r in refs[v]):
            return False  # SEH scope table __except/__finally target
        if all(k == "code" and owner(r) == owner(v) for k, r in refs[v]):
            return False  # jump-table base / local label
        return not branched_internally(v)

    missing = sorted(v for v in refs if keep(v))
    for v in missing:
        i = bisect_right(known_sorted, v)
        nxt = known_sorted[i] if i < len(known_sorted) else TEXT_HI
        reach, a = v, v
        while a < TEXT_HI:
            w = word(a)
            op = w >> 26
            t = None
            if op == 18 and not (w & 3):
                li = w & 0x03FFFFFC
                t = a + (li - 0x04000000 if li & 0x02000000 else li)
            elif op == 16 and not (w & 3):
                bd = w & 0xFFFC
                t = a + (bd - 0x10000 if bd & 0x8000 else bd)
            if t is not None and v < t < v + 0x10000:
                reach = max(reach, t)
            if is_terminator(w) and w != 0 and a + 4 > reach:
                break
            a += 4
        flow_end = a + 4
        kinds = ",".join(sorted(set(k for k, _ in refs[v])))
        print("0x%08X  next=0x%08X  flowend=0x%08X  refs=%d (%s) first=0x%08X" % (v, nxt, flow_end, len(refs[v]), kinds, refs[v][0][1]))
    print("total missing:", len(missing), file=sys.stderr)


if __name__ == "__main__":
    main()
