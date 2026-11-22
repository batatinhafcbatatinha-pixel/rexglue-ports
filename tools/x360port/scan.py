"""Entry points the ReXGlue analysis missed, found in the loaded image (x360port compile runs it once per port).

The analysis registers functions it reaches by calls and its vtable scan. A title also reaches code through pointers
in its data (function tables, callbacks) and through addresses its code builds with lis/addi pairs; a run that takes
one of those stops with "Call to invalid or unregistered function". This looks for both in the image the runtime
loaded (X360PORT_DUMP_IMAGE, see x360port.py generic_project): every code address that starts right after the end of
other code (blr, bctr, an unconditional branch or padding) and is not a registered function yet.
"""
import re
import struct
from pathlib import Path


def load(dump):
    data = Path(dump).read_bytes()
    image_base, image_size, code_base, code_size = struct.unpack_from('<4I', data, 0)
    return image_base, code_base, code_size, data[16:16 + image_size]


def known_functions(generated_dir, manifest):
    known = set()
    for f in Path(generated_dir).glob('*_register.cpp'):
        known.update(int(m, 16) for m in re.findall(r'SetFunction\(0x([0-9A-Fa-f]+)', f.read_text(encoding='utf-8')))
    if Path(manifest).is_file():
        known.update(int(m, 16) for m in re.findall(r'^(0x[0-9A-Fa-f]+)\s*=', Path(manifest).read_text(encoding='utf-8'), re.M))
    return known


def internal_labels(generated_dir):
    """Branch targets inside the translated functions (loc_XXXXXXXX:). Declaring one as a function would cut the
    function holding it in two, and its own branches to that label would then fail at run time."""
    labels = set()
    for f in Path(generated_dir).glob('*.cpp'):
        labels.update(int(m, 16) for m in re.findall(r'^loc_([0-9A-Fa-f]{8}):', f.read_text(encoding='utf-8', errors='replace'), re.M))
    return labels


def scan(dump, generated_dir, manifest):
    image_base, code_base, code_size, img = load(dump)
    code_end = code_base + code_size

    def word(a):
        o = a - image_base
        return struct.unpack_from('>I', img, o)[0] if 0 <= o <= len(img) - 4 else 0

    def is_terminator(w):
        # Not bctr: a switch's bctr is followed by its first case, and its jump table (in the data, where this scan
        # looks) lists every case as a code address; declared as functions they cut the switch apart.
        return (w == 0x4E800020                            # blr
                or ((w >> 26) == 18 and not (w & 1))       # b (no link)
                or w == 0)                                 # padding

    def plausible(a):
        if a % 4 or not code_base <= a < code_end:
            return False
        w = word(a)
        return w != 0 and (w >> 26) != 0 and is_terminator(word(a - 4))

    known = known_functions(generated_dir, manifest) | internal_labels(generated_dir)
    found = set()
    # Pointers in the data: every aligned word outside the code section.
    for o in range(0, len(img) - 3, 4):
        a = image_base + o
        if code_base <= a < code_end:
            continue
        v = struct.unpack_from('>I', img, o)[0]
        if v not in known and plausible(v):
            found.add(v)
    # lis rD,hi ... addi rX,rD,lo within the next 8 instructions of the code.
    for a in range(code_base, code_end - 4, 4):
        w = word(a)
        if (w >> 26) != 15 or (w >> 16) & 0x1F:          # lis = addis rD,0,imm
            continue
        rd, hi = (w >> 21) & 0x1F, w & 0xFFFF
        for b in range(a + 4, min(a + 36, code_end), 4):
            x = word(b)
            if (x >> 26) == 14 and ((x >> 16) & 0x1F) == rd:  # addi rX,rD,lo
                lo = x & 0xFFFF
                v = ((hi << 16) + (lo - 0x10000 if lo & 0x8000 else lo)) & 0xFFFFFFFF
                if v not in known and plausible(v):
                    found.add(v)
                break
            if ((x >> 21) & 0x1F) == rd and (x >> 26) not in (36, 37, 38, 44, 45, 47, 54, 55):
                break                                      # rD written by something else (stores do not write it)
    return sorted(found)
