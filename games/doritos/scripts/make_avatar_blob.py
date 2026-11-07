"""Build the stand-in Xbox Avatar asset blob from the game's own Cal3D dummy model.

The game asks XAM (XamAvatarGetAssets) for render-ready avatar data and gets two buffers back: a main
buffer (skeleton, part and draw-batch tables) and a physical one (vertices, indices, textures). This script
writes both images, with every pointer stored as an offset plus a relocation entry, into
  D:/CodexBuilds/doritos/game_files/avatar_dummy.bin
which src/avatar_xam.cpp copies into the game's buffers. Input: the dummy model extracted from data.pak by
scripts/pak_extract.py (D:/CodexBuilds/doritos/pak). Layout derived from the game's consumers
(NOTES.md, "Avatar do jogador").

Blob: 'DAVA', u32 version, u32 main_size, u32 gpu_size, u32 n_reloc, main image, gpu image,
      n_reloc x (u32 offset_in_main | 0x80000000 if the value points into the gpu image).

Main buffer:  +0 -> skeleton {u32 count, -> bones (0x60 each)}; +8 part count; +0x10 -> parts (0x34 each)
Bone (0x60):  +0 parent (avatar index, -1 root), +0x30 local translation, +0x40 local rotation, +0x50 scale
Part (0x34):  +0x0C vb size, +0x10 ib size, +0x14 batches, +0x18 textures, +0x24 -> vb, +0x28 -> ib,
              +0x2C -> batches (0x200 each), +0x30 -> textures (0x2C each)
Batch (0x200): +0 shader type, 20 x 24-byte commands at +4 ({type, kind, u16 texture, u16 sampler, ...}),
              +0x1E4 triangles, +0x1F0 stride, +0x1F8 -> vertices, +0x1FC -> 16-bit indices
Texture (0x2C): +0 format, +4 width, +8 height, +0x14 slice pitch, +0x20 variants, +0x24 -> data
"""
import argparse
import math
import struct
from pathlib import Path

PAK = Path(r'D:\CodexBuilds\doritos\pak\data')
OUT = Path(r'D:\CodexBuilds\doritos\game_files\avatar_dummy.bin')
EXE = Path(r'D:\CodexBuilds\doritos\default-pe.bin')
BASE = 0x82000000
LIN_A8R8G8B8 = 0x18280086
STRIDE = 40  # vertex of avatar shader type 0 (body, opaque)
SUBMESH_TEXTURE = {0: 'mouth', 1: 'eye', 2: 'eye', 3: 'brow', 4: 'brow', 5: 'body'}


def avatar_to_cal_table():
    d = EXE.read_bytes()
    return [struct.unpack_from('>i', d, 0x82A71D00 - BASE + 4 * i)[0] for i in range(72)]


def read_csf(path):
    c = path.read_bytes()
    o = 4
    ver, nb = struct.unpack_from('<ii', c, o); o += 8
    bones = []
    for _ in range(nb):
        n = struct.unpack_from('<i', c, o)[0]; o += 4
        name = c[o:o + n].rstrip(b'\0').decode(); o += n
        t = struct.unpack_from('<7f', c, o); o += 28
        bt = struct.unpack_from('<7f', c, o); o += 28
        parent, nch = struct.unpack_from('<ii', c, o); o += 8 + 4 * nch
        bones.append(dict(name=name, t=t[:3], r=t[3:], bt=bt[:3], br=bt[3:], parent=parent))
    return bones


def read_cmf(path):
    d = path.read_bytes()
    o = 4
    ver, _, nsub = struct.unpack_from('<iii', d, o); o += 12
    subs = []
    for _ in range(nsub):
        mat, nv, nf, lod, nspr, nmap = struct.unpack_from('<6i', d, o); o += 24
        verts = []
        for _ in range(nv):
            px, py, pz, nx, ny, nz = struct.unpack_from('<6f', d, o); o += 24
            o += 8  # collapse id, face collapse count
            uvs = [struct.unpack_from('<2f', d, o + 8 * k) for k in range(nmap)]; o += 8 * nmap
            ninf = struct.unpack_from('<i', d, o)[0]; o += 4
            inf = [struct.unpack_from('<if', d, o + 8 * k) for k in range(ninf)]; o += 8 * ninf
            if nspr:
                o += 4
            verts.append(dict(p=(px, py, pz), n=(nx, ny, nz), uv=uvs, inf=inf))
        o += nspr * 24
        faces = [struct.unpack_from('<3i', d, o + 12 * k) for k in range(nf)]; o += 12 * nf
        subs.append(dict(verts=verts, faces=faces, nmap=nmap))
    return subs


def cal_qmul(a, q):
    """Cal3D CalQuaternion a *= q."""
    qx, qy, qz, qw = a
    return (qw * q[0] + qx * q[3] + qy * q[2] - qz * q[1], qw * q[1] - qx * q[2] + qy * q[3] + qz * q[0],
            qw * q[2] + qx * q[1] - qy * q[0] + qz * q[3], qw * q[3] - qx * q[0] - qy * q[1] - qz * q[2])


def cal_vrot(v, q):
    """Cal3D CalVector v *= q."""
    qx, qy, qz, qw = -q[0], -q[1], -q[2], q[3]
    t = (qw * v[0] + qy * v[2] - qz * v[1], qw * v[1] - qx * v[2] + qz * v[0], qw * v[2] + qx * v[1] - qy * v[0],
         -qx * v[0] - qy * v[1] - qz * v[2])
    return cal_qmul(t, q)[:3]


def cal_absolute(csf):
    """Bind pose (absolute translation/rotation) of every Cal3D bone, as CalBone::calculateState does."""
    T, R = [None] * len(csf), [None] * len(csf)
    for i, b in enumerate(csf):
        p = b['parent']
        if p < 0:
            T[i], R[i] = tuple(b['t']), tuple(b['r'])
        else:
            t = cal_vrot(b['t'], R[p])
            T[i] = tuple(t[k] + T[p][k] for k in range(3))
            R[i] = cal_qmul(b['r'], R[p])
    return T, R


def half(f):
    return struct.unpack('<H', struct.pack('<e', f))[0]


def hend3n(x, y, z):
    """Normal as the game declares it (sub_82395EC8: offset 0xC, type 0x2A2190 = k_10_11_11 signed
    normalized): x 11 bits at bit 0, y 11 bits at bit 11, z 10 bits at bit 22."""
    def c(v, bits):
        m = (1 << (bits - 1)) - 1
        return int(round(max(-1.0, min(1.0, v)) * m)) & ((1 << bits) - 1)
    return c(x, 11) | (c(y, 11) << 11) | (c(z, 10) << 22)


def texture_argb(img):
    """PIL image -> big-endian ARGB texels, linear rows (width a multiple of 64)."""
    img = img.convert('RGBA')
    out = bytearray()
    for (r, g, b, a) in img.getdata():
        out += bytes((a, r, g, b))
    return img.size[0], img.size[1], bytes(out)


def face_textures():
    from PIL import Image as PImage, ImageDraw
    eye = PImage.new('RGBA', (64, 64), (245, 245, 240, 255))
    d = ImageDraw.Draw(eye)
    d.ellipse((18, 18, 46, 46), fill=(70, 45, 25, 255))
    d.ellipse((26, 26, 38, 38), fill=(10, 10, 10, 255))
    d.ellipse((34, 22, 39, 27), fill=(255, 255, 255, 255))
    brow = PImage.new('RGBA', (64, 64), (60, 35, 20, 255))
    mouth = PImage.new('RGBA', (64, 64), (150, 60, 60, 255))
    clear = PImage.new('RGBA', (64, 64), (0, 0, 0, 0))
    white = PImage.new('RGBA', (64, 64), (255, 255, 255, 255))
    return dict(eye=eye, brow=brow, mouth=mouth, clear=clear, white=white)


class Image:
    def __init__(self):
        self.b = bytearray()

    def align(self, a):
        while len(self.b) % a:
            self.b.append(0)

    def alloc(self, n, a=16):
        self.align(a)
        o = len(self.b)
        self.b += bytes(n)
        return o


def build_parts(main, gpu, ptr, hdr, c2a, args):
    from PIL import Image as PImage
    subs = read_cmf(PAK / 'art/_avatar/dummymodel.cmf')
    imgs = face_textures()
    imgs['body'] = PImage.open(args.body_texture or (PAK / 'art/_avatar/avatar_body_texture.jpg'))
    tex_order = ['body', 'eye', 'brow', 'mouth', 'clear', 'white']
    tex_desc = main.alloc(0x2C * len(tex_order), 16)
    for i, name in enumerate(tex_order):
        w, h, data = texture_argb(imgs[name])
        gpu.align(0x1000)
        at = len(gpu.b)
        gpu.b += data
        rec = tex_desc + 0x2C * i
        struct.pack_into('>III', main.b, rec, LIN_A8R8G8B8, w, h)
        struct.pack_into('>I', main.b, rec + 0x14, len(data))
        struct.pack_into('>I', main.b, rec + 0x20, 1)
        ptr(rec + 0x24, at, True)

    gpu.align(0x1000)
    vb_start = len(gpu.b)
    batches = []
    for si, sub in enumerate(subs):
        vb = len(gpu.b)
        for v in sub['verts']:
            inf = sorted(v['inf'], key=lambda t: -t[1])[:4]
            tot = sum(wt for _, wt in inf) or 1.0
            ws = [int(round(255 * wt / tot)) for _, wt in inf]
            if ws:
                ws[0] += 255 - sum(ws)
            idx = [c2a[b] for b, _ in inf]
            ws += [0] * (4 - len(ws))
            idx += [0] * (4 - len(idx))
            uv0 = v['uv'][0] if v['uv'] else (0.0, 0.0)
            uv1 = v['uv'][1] if len(v['uv']) > 1 else uv0
            rec = struct.pack('>3f', *v['p'])
            rec += struct.pack('>I', hend3n(*v['n']))
            rec += struct.pack('>I', ws[0] | ws[1] << 8 | ws[2] << 16 | ws[3] << 24)
            rec += struct.pack('>I', idx[0] | idx[1] << 8 | idx[2] << 16 | idx[3] << 24)
            rec += struct.pack('>I', 0xFFFFFFFF)
            for uv in (uv0, uv1, uv0):
                # FLOAT16_2 is fetched from a big-endian dword with x in the high half (seen with a quadrant test texture)
                rec += struct.pack('>I', half(uv[0]) << 16 | half(uv[1]))
            assert len(rec) == STRIDE
            gpu.b += rec
        batches.append(dict(vb=vb, nv=len(sub['verts']), sub=si))
    vb_size = len(gpu.b) - vb_start

    gpu.align(0x1000)
    ib_start = len(gpu.b)
    for b in batches:
        sub = subs[b['sub']]
        b['ib'] = len(gpu.b)
        b['nf'] = len(sub['faces'])
        for f in sub['faces']:
            gpu.b += struct.pack('>3H', *f)
        gpu.align(4)
    ib_size = len(gpu.b) - ib_start

    brecs = main.alloc(0x200 * len(batches), 16)
    clear = tex_order.index('clear')
    for i, b in enumerate(batches):
        rec = brecs + 0x200 * i
        struct.pack_into('>i', main.b, rec, 0)
        tex = tex_order.index(SUBMESH_TEXTURE[b['sub']])
        # pixel shader (precache_xenon.bsh, avatar/avatar_ps): s0 s_colourTex, s1 s_intensityTex, s2 s_decalTex;
        # CustomColor1..3 = c210..c212 = type-3 command kinds 0x16..0x18 (table in sub_82395EC8)
        intensity = tex_order.index(args.intensity)
        cmds = [(1, 0, tex, 0), (1, 0, intensity, 1), (1, 0, clear, 2)]
        for k, (typ, kind, t, sampler) in enumerate(cmds):
            struct.pack_into('>iiHHiii', main.b, rec + 4 + 24 * k, typ, kind, t, sampler, 0, 0, 0)
        # type-3 kinds -> PS registers (sub_82395EC8): 0x16-0x18 CustomColor1-3 (c210-c212), 0x1B rimLightColour
        # (c5, w = rim exponent). Real avatar batches set c5 themselves; without it c5 keeps whatever the last
        # draw left there (a stale green with a negative exponent flooded the whole body).
        consts = [(kind, args.custom_color) for kind in (0x16, 0x17, 0x18)] + [(0x1B, args.rim)]
        for k, (kind, value) in enumerate(consts):
            struct.pack_into('>ii4f', main.b, rec + 4 + 24 * (len(cmds) + k), 3, kind, *value)
        struct.pack_into('>I', main.b, rec + 0x1E4, b['nf'])
        struct.pack_into('>I', main.b, rec + 0x1EC, b['nv'])
        struct.pack_into('>I', main.b, rec + 0x1F0, STRIDE)
        struct.pack_into('>I', main.b, rec + 0x1F4, b['nv'])
        ptr(rec + 0x1F8, b['vb'], True)
        ptr(rec + 0x1FC, b['ib'], True)

    part = main.alloc(0x34, 16)
    struct.pack_into('>I', main.b, part + 0x0C, vb_size)
    struct.pack_into('>I', main.b, part + 0x10, ib_size)
    struct.pack_into('>I', main.b, part + 0x14, len(batches))
    struct.pack_into('>I', main.b, part + 0x18, len(tex_order))
    ptr(part + 0x24, vb_start, True)
    ptr(part + 0x28, ib_start, True)
    ptr(part + 0x2C, brecs)
    ptr(part + 0x30, tex_desc)
    struct.pack_into('>I', main.b, hdr + 8, 1)
    ptr(hdr + 0x10, part)


def build(args):
    a2c = avatar_to_cal_table()
    csf = read_csf(PAK / 'art/_avatar/dummymodel.csf')
    nbones = 71
    main, gpu = Image(), Image()
    relocs = []

    def ptr(at, value, to_gpu=False):
        struct.pack_into('>I', main.b, at, value)
        relocs.append(at | (0x80000000 if to_gpu else 0))

    hdr = main.alloc(0x20)
    skel = main.alloc(0x10)
    bones = main.alloc(0x60 * nbones)
    ptr(hdr + 0, skel)
    struct.pack_into('>I', main.b, skel + 0, nbones)
    ptr(skel + 4, bones)

    c2a = {c: a for a, c in enumerate(a2c[:nbones])}
    abs_t, abs_r = cal_absolute(csf)
    for a in range(nbones):
        cb = csf[a2c[a]]
        rec = bones + 0x60 * a
        parent = c2a[cb['parent']] if cb['parent'] >= 0 else -1
        struct.pack_into('>iii', main.b, rec, parent, -1, -1)
        t, r = cb['t'], cb['r']
        if parent < 0:
            # the game rotates the root by +90 deg about X (avatar Y-up -> Z-up); undo it
            t = (t[0], t[2], -t[1])
            s = math.sqrt(0.5)
            r = (-s, 0.0, 0.0, s)
        if parent >= 0 and args.swap_yz:
            t = (t[0], t[2], t[1])  # sub_8239F8B0 reads local translations as (+0x30, +0x38, +0x34)
        if parent >= 0 and math.sqrt(sum(x * x for x in t)) < 1e-4:
            t = (0.0, 0.0, 0.002)  # no zero-length bones: the game's physics normalizes bone directions
        # bind pose: the game builds a matrix from +0x20 (rotation) and +0x10 (translation, w = 1) and inverts
        # it (sub_8239F8B0 -> sub_8229E9C8); a zero w there made every node NaN
        struct.pack_into('>4f', main.b, rec + 0x10, *abs_t[a2c[a]], 1.0)
        struct.pack_into('>4f', main.b, rec + 0x20, *abs_r[a2c[a]])
        struct.pack_into('>4f', main.b, rec + 0x30, *t, 1.0)  # w = 1, as in the game's own extra bone
        struct.pack_into('>4f', main.b, rec + 0x40, *r)
        struct.pack_into('>3f', main.b, rec + 0x50, 1.0, 1.0, 1.0)

    if args.parts:
        build_parts(main, gpu, ptr, hdr, c2a, args)
    main.align(16)
    gpu.align(0x1000)
    if not gpu.b:
        gpu.b += bytes(0x1000)
    blob = b'DAVA' + struct.pack('<IIII', 1, len(main.b), len(gpu.b), len(relocs)) + bytes(main.b) + \
        bytes(gpu.b) + b''.join(struct.pack('<I', r) for r in relocs)
    out = Path(args.out)
    out.write_bytes(blob)
    print(f'{out}: main {len(main.b)} bytes, gpu {len(gpu.b)} bytes, {len(relocs)} relocations')


if __name__ == '__main__':
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--no-parts', dest='parts', action='store_false')
    ap.add_argument('--out', default=str(OUT))
    ap.add_argument('--custom-color', type=float, nargs=4, default=(0.0, 0.0, 0.0, 0.0))
    ap.add_argument('--intensity', default='clear', choices=('white', 'clear'))
    ap.add_argument('--swap-yz', action='store_true')
    ap.add_argument('--rim', type=float, nargs=4, default=(0.15, 0.15, 0.15, 3.0), help='rimLightColour rgb + exponent')
    ap.add_argument('--body-texture', default='', help='replace the body colour texture (diagnostics)')
    build(ap.parse_args())
