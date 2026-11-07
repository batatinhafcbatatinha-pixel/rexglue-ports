"""List / extract data.pak (tongas_pack_v300). Usage: pak_list.py [filter] [--extract OUTDIR]
Entries: u32 offset, u32 size, u32 flag, u32 namelen, name (little-endian)."""
import re, struct, sys, zlib
from pathlib import Path
PAK = Path(r'D:\CodexBuilds\doritos\game_files\data.pak')
d = PAK.read_bytes()
flt = sys.argv[1] if len(sys.argv) > 1 and not sys.argv[1].startswith('--') else ''
out = Path(sys.argv[sys.argv.index('--extract') + 1]) if '--extract' in sys.argv else None
entries = []
for m in re.finditer(rb'data/[ -~]+?\.[A-Za-z0-9_]+', d[:0x200000]):
    off, size, flag, n = struct.unpack_from('<IIII', d, m.start() - 16)
    if n != len(m.group()) and d[m.start():m.start() + n].startswith(b'data/'):
        pass
    name = d[m.start():m.start() + n].decode('latin1')
    entries.append((name, off, size, flag))
for name, off, size, flag in entries:
    if flt in name:
        blob = d[off:off + size]
        print(f'{off:08X} {size:8d} flag={flag} {name} head={blob[:8].hex()}')
        if out:
            p = out / name
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_bytes(blob)
print(len(entries), 'entries', file=sys.stderr)
