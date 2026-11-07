"""Extract and LZMA-decompress every file of data.pak into D:/CodexBuilds/doritos/pak (local only)."""
import lzma, re, struct
from pathlib import Path
d = Path(r'D:\CodexBuilds\doritos\game_files\data.pak').read_bytes()
out = Path(r'D:\CodexBuilds\doritos\pak')
n = bad = 0
for m in re.finditer(rb'data/[ -~]+?\.[A-Za-z0-9_]+', d[:0x200000]):
    off, size, flag, ln = struct.unpack_from('<IIII', d, m.start() - 16)
    name = d[m.start():m.start() + ln].decode('latin1')
    b = d[off:off + size]
    try:
        data = lzma.decompress(b, format=lzma.FORMAT_ALONE) if flag == 1 else b
    except Exception:
        data, bad = b, bad + 1
    p = out / name
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_bytes(data)
    n += 1
print(n, 'files,', bad, 'not decompressed')
