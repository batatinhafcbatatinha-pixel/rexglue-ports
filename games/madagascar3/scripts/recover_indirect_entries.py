"""Recover data-referenced PPC entries outside pdata, bounded by decoded control flow.

No game execution or instruction patches. Apply only conservative complete spans.
"""
import argparse
import bisect
import csv
import hashlib
import json
import re
import struct
from pathlib import Path
import capstone

ROOT = Path(__file__).resolve().parents[1]
BASE = 0x82000000


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--apply', action='store_true')
    args = ap.parse_args()
    image = (ROOT / 'analysis/decoded/image-mapped.bin').read_bytes()
    sections = list(csv.DictReader((ROOT / 'analysis/decoded/sections.tsv').open(), delimiter='\t'))
    text = next(s for s in sections if s['name'] == '.text')
    lo, hi = int(text['address'], 16), int(text['address'], 16) + int(text['bytes'])
    register = ROOT / 'project/generated/default/madagascar3_register.cpp'
    known = {int(x, 16) for x in re.findall(r'SetFunction\(0x([0-9A-Fa-f]+)', register.read_text())}
    manifest = ROOT / 'project/madagascar3_manifest.toml'
    previous = manifest.read_text()
    if '# BEGIN RECOVERED ENTRIES' in previous:
        old = previous.split('# BEGIN RECOVERED ENTRIES', 1)[1]
        known -= {int(x, 16) for x in re.findall(r'^(0x[0-9A-Fa-f]+)\s*=', old, re.MULTILINE)}
    pdata = json.loads((ROOT / 'analysis/decoded/pdata-functions.json').read_text())
    spans = sorted((int(p['address'], 16), int(p['address'], 16) + p['bytes']) for p in pdata)
    starts = [s for s, e in spans]
    word = lambda a: struct.unpack_from('>I', image, a - BASE)[0]

    def inside_pdata(a):
        i = bisect.bisect_right(starts, a) - 1
        return i >= 0 and a < spans[i][1]

    references = {}
    for section in sections:
        if section['name'] not in ('.rdata', '.data'):
            continue
        start, size = int(section['address'], 16), int(section['bytes'])
        for a in range(start, start + size - 3, 4):
            v = word(a)
            if lo <= v < hi and v % 4 == 0 and v not in known and not inside_pdata(v):
                references.setdefault(v, []).append(a)

    def terminal(w):
        return w in (0, 0x4E800020, 0x4E800420) or (w >> 26 == 18 and not w & 1)

    candidates = {a for a in references if a > lo and terminal(word(a - 4)) and word(a)}
    anchors = sorted(known | candidates | set(starts) | {hi})
    decoder = capstone.Cs(capstone.CS_ARCH_PPC, capstone.CS_MODE_32 | capstone.CS_MODE_BIG_ENDIAN)

    def walk(start):
        ceiling = min(start + 4096, anchors[bisect.bisect_right(anchors, start)])
        pending, visited = [start], set()
        while pending:
            a = pending.pop()
            if a in visited:
                continue
            if not start <= a < ceiling or a % 4:
                return None, 'control flow leaves bounded span'
            w = word(a)
            if not w or len(list(decoder.disasm(image[a-BASE:a-BASE+4], a))) != 1:
                return None, 'padding or unsupported instruction'
            visited.add(a)
            op = w >> 26
            if w in (0x4E800020, 0x4E800420):
                continue
            if op == 18:
                displacement = w & 0x3FFFFFC
                if displacement & 0x2000000:
                    displacement -= 0x4000000
                target = (displacement if w & 2 else a + displacement) & 0xFFFFFFFF
                if w & 1:
                    pending.append(a + 4)
                elif start <= target < ceiling:
                    pending.append(target)
                elif target not in known | candidates:
                    return None, 'tail target is not an independently referenced entry'
                continue
            if op == 16:
                displacement = w & 0xFFFC
                if displacement & 0x8000:
                    displacement -= 0x10000
                target = (displacement if w & 2 else a + displacement) & 0xFFFFFFFF
                if w & 1:
                    pending.append(a + 4)
                else:
                    if not start <= target < ceiling:
                        return None, 'conditional target outside span'
                    pending.extend([a + 4, target])
                continue
            if op == 19 and ((w >> 1) & 1023) in (16, 528):
                # Conditional return/CTR branch also has a fallthrough path.
                if not w & 1 and ((w >> 21) & 31) == 20:
                    continue
            pending.append(a + 4)
        return max(visited) + 4, None

    accepted, rejected = [], []
    for address in sorted(candidates):
        end, reason = walk(address)
        if reason:
            rejected.append(dict(address=f'0x{address:08X}', reason=reason))
        else:
            accepted.append(dict(address=f'0x{address:08X}', end=f'0x{end:08X}',
                                 references=[f'0x{r:08X}' for r in references[address]],
                                 spanSha256=hashlib.sha256(image[address-BASE:end-BASE]).hexdigest()))
    manifest_has_entries = '# BEGIN RECOVERED ENTRIES' in previous and all(
        f'{e["address"]} = {{ end = {e["end"]} }}' in previous for e in accepted)
    report = dict(imageSha256=hashlib.sha256(image).hexdigest(), accepted=accepted, rejected=rejected,
                  appliedThisInvocation=args.apply, manifestContainsEntries=manifest_has_entries or args.apply,
                  limitations=['Data references are inferred callable candidates, not runtime observations.',
                      'Only entries outside pdata with a preceding terminal/padding and complete bounded CFG are admitted.',
                      'Indirect targets, unsupported instructions and larger/ambiguous functions remain unresolved.'])
    (ROOT / 'analysis/indirect-entries.json').write_text(json.dumps(report, indent=2) + '\n')
    lines = ['# Automatically recovered data-referenced entries; analysis/indirect-entries.json']
    lines += [f'{e["address"]} = {{ end = {e["end"]} }}' for e in accepted]
    (ROOT / 'config/indirect-entries.fragment.toml').write_text('\n'.join(lines) + '\n')
    if args.apply:
        manifest = ROOT / 'project/madagascar3_manifest.toml'
        content = manifest.read_text()
        if '# BEGIN RECOVERED ENTRIES' in content:
            content = content.split('# BEGIN RECOVERED ENTRIES')[0].rstrip() + '\n'
        manifest.write_text(content + '\n# BEGIN RECOVERED ENTRIES\n' + '\n'.join(lines) + '\n')
    print(f'{len(accepted)} bounded entries; {len(rejected)} ambiguous candidates; apply={args.apply}')


if __name__ == '__main__':
    main()
