"""Decode the .CG / .CGM pictures and write a PNG.

    python tools/cg.py disk/END1.CG    out.png        four planes, 16 colours
    python tools/cg.py disk/SELECT.CGM out.png        one plane
    python tools/cg.py disk/END1.CG    out.png 3      three planes

The codec is `FUN_2406_09xx` in SBP98.EXE, read out of the Ghidra listing at
`out/sbp98.asm`.  Written out:

    fread(&skip, 1, 2, fp)              the leading word, thrown away
    planes = ([0x90] == 1) ? 4 : 3      2406:0a0d
    for p in 0..planes-1:
        dst = seg[p]                    a8000 / b0000 / b8000 / e0000
        di = 0
        while di < 0x7d00:              32000 = one PC-98 plane, 2406:0aaa
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
`00 NN NN` and every one of them put the decoder two bytes out of step.

Each plane stops dead at 32000 bytes, which is why the sizes never quite added
up when the planes were run together.

Four files are stored flat, with no leading length that matches: `FONT.CG`
(3192, fixed 42-byte records), `LOGO.CG` (8000), `STAFF1.CG` (64000) and
`WINDOWS.CGM` (32000, one plane, the whole window sheet).
"""
import struct
import sys
import zlib

W, H = 640, 400
PLANE = W * H // 8
LIMIT = 0x7d00                          # 2406:0aaa

# The PC-98 boot palette; entry n is (r, g, b) at 3 bits each.
BOOT = [(0, 0, 0), (0, 0, 7), (7, 0, 0), (7, 0, 7), (0, 7, 0), (0, 7, 7),
        (7, 7, 0), (7, 7, 7), (0, 0, 0), (0, 0, 3), (3, 0, 0), (3, 0, 3),
        (0, 3, 0), (0, 3, 3), (3, 3, 0), (3, 3, 3)]


def unpack(data, planes):
    """The loader above.  Returns a list of `planes` byte strings of 32000."""
    out = []
    i = 0
    n = len(data)
    for _ in range(planes):
        p = bytearray()
        while len(p) < LIMIT:
            if i >= n:
                break
            c = data[i]
            i += 1
            run = 1
            if c in (0x00, 0xff) and i + 1 < n:
                run = struct.unpack_from('<H', data, i)[0] or 1
                i += 2
            p += bytes([c]) * min(run, LIMIT - len(p))
        out.append(bytes(p) + b'\x00' * (LIMIT - len(p)))
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
    if used < len(d) - 2:
        print('  note: %d of %d stream bytes left over' % (len(d) - 2 - used,
                                                           len(d) - 2))
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


def render(ps):
    """One plane is black and white; more than one indexes the palette."""
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
                if mono:
                    c = (255, 255, 255) if v else (0, 0, 0)
                else:
                    r, g, b = BOOT[v]
                    c = (r * 36, g * 36, b * 36)
                i = (y * W + xb * 8 + j) * 3
                rgb[i:i + 3] = bytes(c)
    return bytes(rgb)


def main():
    src, dst = sys.argv[1], sys.argv[2]
    want = int(sys.argv[3]) if len(sys.argv) > 3 else None
    ps, packed = load(src, want)
    print('%s: %d plane%s %s' %
          (src, len(ps), '' if len(ps) == 1 else 's',
           'unpacked' if packed else 'flat'))
    png(dst, W, H, render(ps))
    print('wrote %s' % dst)


if __name__ == '__main__':
    main()
