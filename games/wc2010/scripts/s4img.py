"""(wc2010 copy of street4/scripts/s4img.py) Shared helpers for static analysis of the decoded fifadllzf image (no game execution)."""
import re
import struct
from bisect import bisect_right
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BASE = 0x82300000
IMG = (ROOT / "analysis/decoded-fifadll/image-mapped.bin").read_bytes()
TEXT_LO, TEXT_HI = 0x82650000, 0x82650000 + 19434932
GEN = ROOT / "project/generated/fifadllzf_xex"
STARTS = sorted(int(m, 16) for m in re.findall(
    r"SetFunction\(0x([0-9A-F]+)", (GEN / "wc2010_register.cpp").read_text()))


def word(a):
    return struct.unpack_from(">I", IMG, a - BASE)[0]


def func_of(a):
    i = bisect_right(STARTS, a) - 1
    return STARTS[i] if i >= 0 else None


def func_end(f):
    i = bisect_right(STARTS, f)
    return STARTS[i] if i < len(STARTS) else TEXT_HI


def bl_target(a, w=None):
    w = word(a) if w is None else w
    if (w >> 26) != 18:
        return None
    li = w & 0x03FFFFFC
    if li & 0x02000000:
        li -= 0x04000000
    return (li if (w & 2) else a + li) & 0xFFFFFFFF, bool(w & 1)


_src_index = None


def source_of(f):
    """Return generated C++ text of function f."""
    global _src_index
    if _src_index is None:
        _src_index = {}
        for p in GEN.glob("wc2010_recomp.*.cpp"):
            for m in re.finditer(r"DEFINE_REX_FUNC\(sub_([0-9A-F]{8})\)", p.read_text()):
                _src_index[int(m.group(1), 16)] = p
    p = _src_index.get(f)
    if not p:
        return None
    t = p.read_text()
    s = t.index("DEFINE_REX_FUNC(sub_%08X)" % f)
    e = t.find("\nDEFINE_REX_FUNC(", s + 10)
    return t[s:e if e > 0 else len(t)]


def dis(addr, end=None, count=None):
    """Disassemble [addr,end) (default: whole function). Returns text."""
    import capstone
    md = capstone.Cs(capstone.CS_ARCH_PPC, capstone.CS_MODE_32 | capstone.CS_MODE_BIG_ENDIAN)
    md.skipdata = True
    if end is None:
        end = addr + 4 * count if count else func_end(addr)
    out = []
    for i in md.disasm(IMG[addr - BASE:end - BASE], addr):
        out.append("%08X  %-8s %s" % (i.address, i.mnemonic, i.op_str))
    return "\n".join(out)


def callers(target):
    out = []
    for a in range(TEXT_LO, TEXT_HI, 4):
        r = bl_target(a)
        if r and r[0] == target:
            out.append((a, r[1]))
    return out


def addr_refs(target):
    """Find lis/addi (or lis/ori) pairs that materialize `target`."""
    hi = (target >> 16) & 0xFFFF
    lo = target & 0xFFFF
    hi_adj = ((target + 0x8000) >> 16) & 0xFFFF
    out = []
    for a in range(TEXT_LO, TEXT_HI - 4, 4):
        w = word(a)
        if (w >> 26) != 15 or ((w >> 16) & 31) != 0:
            continue
        imm = w & 0xFFFF
        if imm not in (hi, hi_adj):
            continue
        rd = (w >> 21) & 31
        for k in range(1, 12):
            w2 = word(a + 4 * k)
            op = w2 >> 26
            if op == 14 and ((w2 >> 16) & 31) == rd and imm == hi_adj and (w2 & 0xFFFF) == lo:
                out.append(a + 4 * k); break
            if op == 24 and ((w2 >> 21) & 31) == rd and imm == hi and (w2 & 0xFFFF) == lo:
                out.append(a + 4 * k); break
    # also as data words
    import struct as _s
    b = _s.pack(">I", target)
    i = IMG.find(b)
    data = []
    while i >= 0:
        if i % 4 == 0:
            data.append(i + BASE)
        i = IMG.find(b, i + 1)
    return out, data
