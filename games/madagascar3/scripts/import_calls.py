"""Map static direct PPC branches to imported thunks in the decoded image."""
import csv
import hashlib
import json
import struct
from pathlib import Path

root = Path(__file__).resolve().parents[1]
decoded = root / 'analysis/decoded'
image = (decoded / 'image-mapped.bin').read_bytes()
info = json.loads((decoded / 'image-info.json').read_text())
base = int(info['imageBase'], 16)
with (decoded / 'imports.tsv').open() as source:
    imports = {int(row['address'], 16): row['name']
               for row in csv.DictReader(source, delimiter='\t')}
calls = []
with (decoded / 'sections.tsv').open() as source:
    for section in csv.DictReader(source, delimiter='\t'):
        if section['code'] != '1':
            continue
        start, size = int(section['address'], 16), int(section['bytes'])
        for address in range(start, start + size - 3, 4):
            word = struct.unpack_from('>I', image, address - base)[0]
            if word >> 26 != 18:
                continue
            displacement = word & 0x3FFFFFC
            if displacement & 0x2000000:
                displacement -= 0x4000000
            target = (displacement if word & 2 else address + displacement) & 0xFFFFFFFF
            if target in imports:
                calls.append(dict(address=f'0x{address:08X}', target=f'0x{target:08X}',
                                  name=imports[target], linked=bool(word & 1)))
report = dict(imageSha256=hashlib.sha256(image).hexdigest(), importThunks=len(imports),
              directBranchSites=len(calls), referencedImports=len({c['name'] for c in calls}),
              calls=calls, limitations=['Static direct b/bl only; indirect calls are not covered.',
                                        'Derived image has loader-modified import thunks.',
                                        'No runtime execution established.'])
(root / 'analysis/import-call-xrefs.json').write_text(json.dumps(report, indent=2) + '\n')
print({key: value for key, value in report.items() if key != 'calls'})
