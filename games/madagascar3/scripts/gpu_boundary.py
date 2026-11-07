"""Locate this image's graphics boundary for subsequent native draw/resource hooks.

Static candidate map only: no guessed SDK device layout and no hooks enabled.
"""
import bisect
import csv
import hashlib
import json
import re
import struct
from pathlib import Path

root = Path(__file__).resolve().parents[1]
image = (root / 'analysis/decoded/image-mapped.bin').read_bytes()
sections = list(csv.DictReader((root / 'analysis/decoded/sections.tsv').open(), delimiter='\t'))
text = next(s for s in sections if s['name'] == '.text')
low, high = int(text['address'], 16), int(text['address'], 16) + int(text['bytes'])
known = sorted({int(s, 16) for s in re.findall(r'SetFunction\(0x([0-9A-Fa-f]+)',
    (root / 'project/generated/default/madagascar3_register.cpp').read_text())})
word = lambda a: struct.unpack_from('>I', image, a - 0x82000000)[0]
owner = lambda a: known[bisect.bisect_right(known, a) - 1]
calls = json.loads((root / 'analysis/import-call-xrefs.json').read_text())['calls']
anchors = [dict(**c, inferredOwner=f'0x{owner(int(c["address"],16)):08X}')
           for c in calls if c['name'].startswith('__imp__Vd')]
operations = {0x22: 'DRAW_INDX', 0x36: 'DRAW_INDX_2', 0x27: 'IM_LOAD',
              0x2B: 'IM_LOAD_IMMEDIATE', 0x2D: 'SET_CONSTANT', 0x46: 'EVENT_WRITE',
              0x59: 'EVENT_WRITE_EXT', 0x64: 'XE_SWAP'}
packets = []
for a in range(low, high - 4, 4):
    w = word(a)
    if w >> 26 != 15 or (w >> 16) & 31:
        continue
    rd = (w >> 21) & 31
    upper = (w & 0xFFFF) << 16
    # Adjacent lis/ori-or-addi only; larger live ranges need register dataflow.
    w2 = word(a + 4)
    if w2 >> 26 == 24 and (w2 >> 21) & 31 == rd and (w2 >> 16) & 31 == rd:
        value = upper | (w2 & 0xFFFF)
    elif w2 >> 26 == 14 and (w2 >> 16) & 31 == rd and (w2 >> 21) & 31 == rd:
        value = (upper + struct.unpack('>h', struct.pack('>H', w2 & 0xFFFF))[0]) & 0xFFFFFFFF
    else:
        continue
    opcode = (value >> 8) & 0x7F
    if value >> 30 == 3 and value & 0xFF in (0, 1) and opcode in operations:
        packets.append(dict(address=f'0x{a:08X}', owner=f'0x{owner(a):08X}',
            words=[f'{w:08X}', f'{w2:08X}'], value=f'0x{value:08X}', operation=operations[opcode]))
report = dict(imageSha256=hashlib.sha256(image).hexdigest(), importedGraphicsCalls=anchors,
    packetConstantCandidates=packets, hooksEnabled=False,
    limitations=['Nearest registered owner is an inference, not a recovered original symbol.',
                 'Packet constants suggest writers; they do not establish API ABI or device layout.',
                 'No renderer hook is enabled from this candidate map and no game was executed.'])
(root / 'analysis/native-gpu-boundary.json').write_text(json.dumps(report, indent=2) + '\n')
print(f'{len(anchors)} graphics import sites; {len(packets)} adjacent packet constants; no hooks applied')
