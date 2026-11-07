"""Extract every file of the XDVDFS game partition using analysis/iso-inventory.json."""
import argparse
import json
import fnmatch
import shutil
from pathlib import Path

SECTOR = 2048
CHUNK = 8 * 1024 * 1024


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('iso', type=Path)
    ap.add_argument('inventory', type=Path)
    ap.add_argument('output', type=Path)
    ap.add_argument('--include', action='append', default=[], help='Only matching paths (repeatable glob)')
    ap.add_argument('--max-mib', type=int, default=512, help='Maximum additional extraction, default 512 MiB')
    args = ap.parse_args()
    inv = json.loads(args.inventory.read_text(encoding='utf-8'))
    base = inv['gamePartitionOffset']
    out = args.output.resolve()
    if args.max_mib <= 0:
        ap.error('Positive extraction budget required')
    selected = [e for e in inv['files'] if not args.include or any(
        fnmatch.fnmatchcase(e['path'].lower(), pattern.lower()) for pattern in args.include)]
    needed = sum(e['bytes'] for e in selected if not (out / e['path']).is_file()
                 or (out / e['path']).stat().st_size != e['bytes'])
    probe = out
    while not probe.exists():
        probe = probe.parent
    if needed > args.max_mib * 1024 ** 2:
        ap.error(f'Extraction needs {needed} bytes; exceeds --max-mib budget. Select --include paths.')
    if shutil.disk_usage(probe).free - needed < 0:
        ap.error('Insufficient disk space for selected files')
    done = 0
    with args.iso.open('rb') as f:
        for e in selected:
            target = (out / e['path']).resolve()
            if out not in target.parents:
                raise ValueError('unsafe path ' + e['path'])
            if target.exists() and target.stat().st_size == e['bytes']:
                continue
            if shutil.disk_usage(probe).free - e['bytes'] < 0:
                raise RuntimeError('Insufficient disk space for ' + e['path'])
            target.parent.mkdir(parents=True, exist_ok=True)
            f.seek(base + e['sector'] * SECTOR)
            left = e['bytes']
            with target.open('wb') as w:
                while left:
                    data = f.read(min(CHUNK, left))
                    if not data:
                        raise ValueError('truncated ' + e['path'])
                    w.write(data)
                    left -= len(data)
            done += 1
    print(f'extracted {done} files, {len(inv["files"])} total')


if __name__ == '__main__':
    main()
