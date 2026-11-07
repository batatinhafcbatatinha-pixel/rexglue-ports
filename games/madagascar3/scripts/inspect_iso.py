"""Inventory an Xbox XDVDFS ISO and extract only XEX executables for analysis."""
import argparse
import hashlib
import json
from pathlib import Path
import struct

SECTOR = 2048
MAGIC = b'MICROSOFT*XBOX*MEDIA'
BASES = (0, 0xFD90, 0x30600, 0x2080000, 0xFD90000, 0xFDA0000, 0x18300000)

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('iso', type=Path)
    ap.add_argument('--output', type=Path, required=True)
    ap.add_argument('--inventory-only', action='store_true', help='Do not create another XEX copy')
    args = ap.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    length = args.iso.stat().st_size
    records, seen_dirs = [], set()
    with args.iso.open('rb') as f:
        def read(offset, size):
            if offset < 0 or size < 0 or offset + size > length:
                raise ValueError('Out-of-image file extent')
            f.seek(offset)
            data = f.read(size)
            if len(data) != size:
                raise ValueError('Truncated image')
            return data
        base = next((b for b in BASES if b + 0x10000 + len(MAGIC) <= length and read(b + 0x10000, len(MAGIC)) == MAGIC), None)
        if base is None:
            raise ValueError('No XDVDFS game partition found at known offsets')
        descriptor = read(base + 0x10000, SECTOR)
        def walk(sector, size, prefix='', depth=0):
            if depth > 64 or (sector, size) in seen_dirs or size > 64 * 1024 * 1024:
                raise ValueError('Invalid directory tree')
            seen_dirs.add((sector, size))
            data = read(base + sector * SECTOR, size)
            pending, seen = [0], set()
            while pending:
                offset = pending.pop()
                if offset in seen or offset + 14 > len(data):
                    raise ValueError('Invalid directory node')
                seen.add(offset)
                left, right, start, count, flags, n = struct.unpack_from('<HHIIBB', data, offset)
                if not n or offset + 14 + n > len(data):
                    raise ValueError('Invalid directory name')
                name = data[offset + 14:offset + 14 + n].decode('ascii')
                if name in ('.', '..') or any(c in name for c in '\\/:*?"<>|'):
                    raise ValueError('Unsafe directory name')
                if left:
                    pending.append(left * 4)
                if right:
                    pending.append(right * 4)
                path = prefix + name
                if flags & 0x10:
                    walk(start, count, path + '/', depth + 1)
                else:
                    if base + start * SECTOR + count > length:
                        raise ValueError('Invalid file extent')
                    records.append({'path': path, 'sector': start, 'bytes': count})
        walk(*struct.unpack_from('<II', descriptor, 20))
        executables = []
        for entry in records:
            if entry['path'].lower() not in ('default.xex', 'mpdefault.xex'):
                continue
            blob = read(base + entry['sector'] * SECTOR, entry['bytes'])
            if blob[:4] != b'XEX2' or len(blob) < 24:
                raise ValueError('Unexpected XEX header')
            target = output / 'executables' / entry['path']
            if not args.inventory_only:
                target.parent.mkdir(parents=True, exist_ok=True)
                if target.exists() and target.read_bytes() != blob:
                    raise ValueError('Refusing to replace different executable')
                if not target.exists():
                    target.write_bytes(blob)
            flags, pe_offset, reserved, security, n = struct.unpack_from('>IIIII', blob, 4)
            headers = []
            info = dict(path=entry['path'], bytes=len(blob), sha256=hashlib.sha256(blob).hexdigest(),
                        format='XEX2', moduleFlags=f'0x{flags:08X}', imageDataOffset=pe_offset,
                        securityInfoOffset=security, optionalHeaderCount=n)
            if n > 10000 or 24 + n * 8 > len(blob):
                raise ValueError('Invalid optional header count')
            for i in range(n):
                key, value = struct.unpack_from('>II', blob, 24 + i * 8)
                headers.append({'key': f'0x{key:08X}', 'value': f'0x{value:08X}'})
                if key == 0x00040006:
                    media, version, base_version, title, platform, exe_type, disc, discs, save = struct.unpack_from('>IIIIBBBBI', blob, value)
                    info.update(titleId=f'{title:08X}', mediaId=f'{media:08X}', version=f'0x{version:08X}',
                                baseVersion=f'0x{base_version:08X}', platform=platform, executableType=exe_type,
                                discNumber=disc, discCount=discs, savegameId=f'{save:08X}')
                elif key == 0x00010100:
                    info['entryPoint'] = f'0x{value:08X}'
                elif key == 0x00010201:
                    info['imageBase'] = f'0x{value:08X}'
                elif key == 0x000003FF:
                    size, encryption, compression = struct.unpack_from('>IHH', blob, value)
                    info['fileFormatInfo'] = {'bytes': size, 'encryptionType': encryption, 'compressionType': compression}
            info['optionalHeaders'] = headers
            executables.append(info)
        report = dict(sourceIso=str(args.iso.resolve()), isoBytes=length, gamePartitionOffset=base,
                      fileCount=len(records), resourceBytes=sum(e['bytes'] for e in records),
                      files=sorted(records, key=lambda e: e['path'].lower()), executables=executables,
                      limitations=['Static inventory only; no game execution.', 'Only XEX executables extracted; remaining game assets stay in the ISO.',
                                   'Language filenames are candidates, not verified language selection.'])
        (output / 'iso-inventory.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
        summary = {k:v for k,v in report.items() if k not in ('files', 'executables')}
        summary['executables'] = [{k:v for k,v in e.items() if k != 'optionalHeaders'} for e in executables]
        print(json.dumps(summary, indent=2))
        print('Language-related paths:', [e['path'] for e in records if any(s in e['path'].lower() for s in ('locale', 'language', 'portug', 'brazil', 'eng_', 'por_', 'pt_', 'en_'))])

if __name__ == '__main__':
    main()
