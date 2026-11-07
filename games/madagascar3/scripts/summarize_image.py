"""Validate the exported image and inventory compiler function records and entry code."""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import struct
from capstone import Cs, CS_ARCH_PPC, CS_MODE_64, CS_MODE_BIG_ENDIAN

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('directory', type=Path)
    args = ap.parse_args()
    root = args.directory
    image = (root / 'image-mapped.bin').read_bytes()
    info = json.loads((root / 'image-info.json').read_text())
    if len(image) != info['imageBytes'] or image[:2] != b'MZ':
        raise ValueError('Image export incomplete')
    base, entry = int(info['imageBase'], 16), int(info['entryPoint'], 16)
    pe = struct.unpack_from('<I', image, 60)[0]
    if image[pe:pe+4] != b'PE\0\0':
        raise ValueError('PE signature missing')
    machine, nsections = struct.unpack_from('<HH', image, pe + 4)
    if machine != 0x1f2:
        raise ValueError(f'Unexpected architecture: {machine:X}')
    optional = struct.unpack_from('<H', image, pe + 20)[0]
    sections = []
    for i in range(nsections):
        offset = pe + 24 + optional + 40 * i
        name = image[offset:offset+8].split(b'\0')[0].decode('ascii')
        size, rva, rawsize, rawoffset = struct.unpack_from('<IIII', image, offset + 8)
        flags = struct.unpack_from('<I', image, offset + 36)[0]
        sections.append({'name':name, 'address':base+rva, 'declaredBytes':size,
                         'availableBytes':max(0,min(size,len(image)-rva)), 'rawOffset':rawoffset,
                         'rawBytes':rawsize, 'code':bool(flags & 0x20)})
    pdata = next(s for s in sections if s['name'] == '.pdata')
    code_sections = [s for s in sections if s['code']]
    functions, rejected = [], []
    for offset in range(pdata['address']-base, pdata['address']-base+pdata['availableBytes'], 8):
        address, packed = struct.unpack_from('>II',image,offset)
        size = ((packed >> 8) & 0x3fffff) * 4
        valid = size and any(s['address'] <= address and address + size <= s['address'] + s['availableBytes'] for s in code_sections)
        row = {'address':f'0x{address:08X}', 'bytes':size, 'prologBytes':(packed & 255)*4,
               'exceptionFlag':bool(packed & 0x80000000)}
        (functions if valid else rejected).append(row)
    (root / 'pdata-functions.json').write_text(json.dumps(functions,indent=2)+'\n',encoding='utf-8')
    imports = list(csv.DictReader((root/'imports.tsv').open(encoding='utf-8'),delimiter='\t'))
    disasm = Cs(CS_ARCH_PPC,CS_MODE_64|CS_MODE_BIG_ENDIAN)
    entry_code = [{'address':f'0x{i.address:08X}', 'mnemonic':i.mnemonic, 'operands':i.op_str}
                  for i in disasm.disasm(image[entry-base:entry-base+96],entry)]
    report = {'imageSha256':hashlib.sha256(image).hexdigest(), 'imageBytes':len(image),
              'imageBase':info['imageBase'], 'entryPoint':info['entryPoint'], 'machine':'PowerPC big-endian (PE 0x01F2)',
              'sections':sections, 'pdataValidRecords':len(functions), 'pdataRejectedRecords':len(rejected),
              'namedImportThunks':len(imports), 'entryInstructions':entry_code,
              'limitations':['Compiler function records do not cover every leaf/indirect function.',
                             'Import thunks were modified by XenonUtils; this is a derived image, not original disc bytes.',
                             'Declared PE section extent may exceed the XEX image; only available mapped bytes are admitted.',
                             'No game instructions executed; this does not establish boot or gameplay.']}
    (root/'analysis-summary.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({k:v for k,v in report.items() if k not in ('sections','entryInstructions')},indent=2))

if __name__ == '__main__':
    main()
