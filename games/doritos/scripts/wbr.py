"""Minimal reader/writer for the game's .wbr template tables (local experiments only).
Layout: u16 2, u16 1, u16 class_id, str class, u16 ?, u16 nfields, nfields x (str name, u8 type),
u16 nentries, entries: u16 class_id, str name, nfields x ('1' value | '0'), '0' terminator."""
import struct, sys

def rstr(d, o):
    n = struct.unpack_from('<I', d, o)[0]
    return d[o + 4:o + 4 + n].decode('latin1'), o + 4 + n

SIZES = {0: 4, 1: 4, 2: 1, 3: 4, 4: 8, 5: 12, 9: 12}

def parse(d):
    o = 4
    cid = struct.unpack_from('<H', d, o)[0]; o += 2
    cls, o = rstr(d, o)
    o += 2
    nf = struct.unpack_from('<H', d, o)[0]; o += 2
    fields = []
    for _ in range(nf):
        name, o = rstr(d, o)
        fields.append((name, d[o])); o += 1
    head_end = o
    ne = struct.unpack_from('<H', d, o)[0]; o += 2
    return cls, fields, head_end, ne, o

if __name__ == '__main__':
    d = open(sys.argv[1], 'rb').read()
    cls, fields, he, ne, o = parse(d)
    print(cls, ne, 'entries'); print(fields)
