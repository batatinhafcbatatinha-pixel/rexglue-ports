"""Extracts the microcode the D3D library uploads with IM_LOAD_IMM (internal clear/copy
shaders) from the decoded image, as offline test input for the shader translator."""
import struct, sys, os
from s4img import *
out = sys.argv[1]
os.makedirs(out, exist_ok=True)
found = {}
for a in range(0x826FC000, 0x82726000, 4):
    w = word(a)
    if (w >> 26) == 24 and (w & 0xFFFF) == 0x2B00:  # ori rX, rY, 0x2B00 (IM_LOAD_IMM low half)
        regs = {}
        typ = None
        for k in range(1, 30):
            b = a + 4 * k
            x = word(b)
            op = x >> 26
            rd = (x >> 21) & 31
            ra = (x >> 16) & 31
            imm = x & 0xFFFF
            simm = imm - 0x10000 if imm & 0x8000 else imm
            if op == 15 and ra == 0:
                regs[rd] = (simm << 16) & 0xFFFFFFFF
            elif op == 14 and ra == 0 and rd == 10 and typ is None:
                typ = simm
            elif op == 14 and rd == 4 and ra in regs:
                addr = (regs[ra] + simm) & 0xFFFFFFFF
                # size: li r5, n (bytes)
                for j in range(1, 6):
                    y = word(b + 4 * j)
                    if (y >> 26) == 14 and ((y >> 21) & 31) == 5 and ((y >> 16) & 31) == 0:
                        size = y & 0xFFFF
                        found[addr] = (size, typ)
                        break
                break
            elif op == 14 and ra in regs and rd != 4:
                regs[rd] = (regs[ra] + simm) & 0xFFFFFFFF
for addr, (size, typ) in sorted(found.items()):
    if not (BASE <= addr < BASE + len(IMG)) or size == 0 or size > 4096:
        continue
    data = IMG[addr - BASE: addr - BASE + size]
    stage = 'ps' if typ == 1 else 'vs'
    with open(os.path.join(out, '%s_%08X.bin' % (stage, addr)), 'wb') as f:
        f.write(data)
    print(stage, hex(addr), size)
