"""Vtable call thunks `lwz r12,0(rA); lwz r11,d(r12); mtctr r11; bctr` (and the r3-based variant
with any registers) not yet known to the codegen. Prints manifest lines."""
import re, struct
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
BASE, LO, HI = 0x82000000, 0x821A0000, 0x829F768C
img = Path('D:/CodexBuilds/doritos/default-pe.bin').read_bytes()
w = lambda a: struct.unpack_from('>I', img, a - BASE)[0]
known = {int(x, 16) for x in re.findall(r'SetFunction\(0x([0-9A-Fa-f]+)',
         (ROOT / 'project/generated/default/doritoscrashcourse_register.cpp').read_text())}
def lwz(x): return x >> 26 == 32
for a in range(LO, HI - 12, 4):
    w0, w1, w2, w3 = w(a), w(a + 4), w(a + 8), w(a + 12)
    if w3 != 0x4E800420 or not (lwz(w0) and lwz(w1)) or w0 & 0xFFFF:
        continue
    r12 = (w0 >> 21) & 31
    r11 = (w1 >> 21) & 31
    if (w1 >> 16) & 31 != r12 or w2 != (0x7C0903A6 | (r11 << 21)):
        continue
    prev = w(a - 4)
    if a in known or not (prev in (0, 0x4E800020, 0x4E800420) or (prev >> 26 == 18 and not prev & 1)):
        continue
    print(f'0x{a:08X} = {{ end = 0x{a + 16:08X} }}')
