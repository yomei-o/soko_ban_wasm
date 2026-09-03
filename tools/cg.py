"""Decode the .CG / .CGM pictures and write a PNG.

    python tools/cg.py disk/TITLE.CG  out.png --pal 0     the wooden plaque
    python tools/cg.py disk/END1.CG   out.png --pal 2     the ending
    python tools/cg.py disk/CHR98N.CG out.png --pal 1     the tiles
    python tools/cg.py disk/SELECT.CGM out.png            one plane, mono
    python tools/cg.py disk/END1.CG   out.png --planes 3

The codec is the loader at 2406:09xx in SBP98.EXE, read out of the Ghidra
listing in `out/sbp98.asm`.  Written out:

    fread(&skip, 1, 2, fp)              the leading word, thrown away
    planes = ([0x90] == 1) ? 4 : 3      2406:0a0d
    for p in 0..planes-1:
        dst = seg[p]                    from the table at DS:0x0330,
        di = 0                          which is literally 00 a8 00 b0
        while di < 0x7d00:                            00 b8 00 e0
            fread(&c, 1, 1, fp)
            *dst++ = c                  the byte is written before the test
            if c == 0x00 or c == 0xff:  2406:0a68 and 2406:0a6e
                fread(&n, 1, 2, fp)
                while n - 1 > si++:     2406:0aa5
                    *dst++ = c
                di += n - 1
            di++

So a run is the byte itself followed by a 16-bit count that INCLUDES that first
byte, and 0x00 escapes exactly as 0xff does.  Missing the 0x00 case is what
made every picture shear: a mask is mostly zeros, so the stream is full of
`00 NN NN` and every one of them put the decoder two bytes out of step.  Each
plane also stops dead at 32000 bytes, which is why running them together left
the sizes a few hundred short of a whole plane.

Four files are stored flat, with no leading length that matches: `FONT.CG`
(3192, fixed 42-byte records), `LOGO.CG` (8000), `STAFF1.CG` (64000) and
`WINDOWS.CGM` (32000, one plane, the whole window sheet).

The three palettes sit just before that plane-segment table, at DS:0x02a0,
0x02d0 and 0x0300 - sixteen entries of three nibbles each, in R G B order,
scaled by 17.  Palette 0 is the title's wood, 1 the tiles, 2 the ending.
"""
import struct
import sys
import zlib

W, H = 640, 400
PLANE = W * H // 8
LIMIT = 0x7d00                          # 2406:0aaa

MZ_HEADER = 4608                        # SBP98.EXE
DS_AT = 0x19ca0                         # DS = 0x19ca, in the load image
PAL_AT = (0x02a0, 0x02d0, 0x0300)

# The PC-98 boot palette, for when the file's own palette is not known.
BOOT = [(0, 0, 0), (0, 0, 15), (15, 0, 0), (15, 0, 15), (0, 15, 0),
        (0, 15, 15), (15, 15, 0), (15, 15, 15), (0, 0, 0), (0, 0, 7),
        (7, 0, 0), (7, 0, 7), (0, 7, 0), (0, 7, 7), (7, 7, 0), (7, 7, 7)]


def palette(n, exe='disk/SBP98.EXE'):
    """One of the three tables, as sixteen (r, g, b) at 0..255."""
    if n is None:
        nib = BOOT
    else:
        d = open(exe, 'rb').read()
        o = MZ_HEADER + DS_AT + PAL_AT[n]
        nib = [tuple(d[o + i * 3 + j] for j in range(3)) for i in range(16)]
    return [tuple(min(255, c * 17) for c in t) for t in nib]


def unpack(data, planes):
    """The loader above.  Returns a list of `planes` byte strings of 32000."""
    out = []
    i = 0
    n = len(data)
    for _ in range(planes):
        p = bytearray()
        while len(p) < LIMIT and i < n:
            c = data[i]
            i += 1
            run = 1
            if c in (0x00, 0xff) and i + 1 < n:
                run = struct.unpack_from('<H', data, i)[0] or 1
                i += 2
            p += bytes([c]) * min(run, LIMIT - len(p))
        out.append(bytes(p).ljust(LIMIT, b'\x00'))
    return out, i


def load(path, planes=None):
    """Returns (planes, was_compressed).  A flat file is split as it lies."""
    d = open(path, 'rb').read()
    packed = len(d) >= 2 and struct.unpack_from('<H', d, 0)[0] == len(d) - 2
    if planes is None:
        planes = 1 if path.upper().endswith('M') else 4
    if not packed:
        return [d[k * PLANE:(k + 1) * PLANE].ljust(PLANE, b'\x00')
                for k in range(max(1, len(d) // PLANE))], False
    ps, used = unpack(d[2:], planes)
    left = len(d) - 2 - used
    if left:
        print('  note: %d of %d stream bytes unread' % (left, len(d) - 2))
    return ps, True


def png(path, w, h, rgb):
    raw = b''.join(b'\x00' + rgb[y * w * 3:(y + 1) * w * 3] for y in range(h))

    def chunk(tag, body):
        c = tag + body
        return struct.pack('>I', len(body)) + c + \
            struct.pack('>I', zlib.crc32(c) & 0xffffffff)

    open(path, 'wb').write(
        b'\x89PNG\r\n\x1a\n' +
        chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)) +
        chunk(b'IDAT', zlib.compress(raw, 9)) +
        chunk(b'IEND', b''))


def render(ps, pal=None):
    """One plane alone is drawn black and white; more index the palette."""
    if pal is None:
        pal = palette(None)
    rgb = bytearray(W * H * 3)
    mono = len(ps) == 1
    for y in range(H):
        for xb in range(W // 8):
            o = y * (W // 8) + xb
            bits = [p[o] for p in ps]
            for j in range(8):
                v = 0
                for k, b in enumerate(bits):
                    v |= ((b >> (7 - j)) & 1) << k
                c = ((255, 255, 255) if v else (0, 0, 0)) if mono else pal[v]
                i = (y * W + xb * 8 + j) * 3
                rgb[i:i + 3] = bytes(c)
    return bytes(rgb)


def main():
    a = sys.argv[1:]
    src, dst = a[0], a[1]
    pal = int(a[a.index('--pal') + 1]) if '--pal' in a else None
    planes = int(a[a.index('--planes') + 1]) if '--planes' in a else None
    ps, packed = load(src, planes)
    print('%s: %d plane%s %s, palette %s' %
          (src, len(ps), '' if len(ps) == 1 else 's',
           'unpacked' if packed else 'flat',
           'boot' if pal is None else 'DS:%04x' % PAL_AT[pal]))
    png(dst, W, H, render(ps, palette(pal)))
    print('wrote %s' % dst)


if __name__ == '__main__':
    main()
