"""Decode the .CG / .CGM pictures and write a PNG.

    python tools/cg.py disk/SELECT.CGM out.png
    python tools/cg.py disk/SELECT.CG  out.png          four planes, 16 colours
    python tools/cg.py disk/CHR98N.CG  out.png --raw    no plane guess, 1bpp

A file starts with the compressed byte count as a 16-bit little endian word -
`SELECT.CGM` is 9070 bytes and opens with 9068 - and the four files whose first
word is not `size - 2` (`FONT.CG`, `LOGO.CG`, `STAFF1.CG`, `WINDOWS.CGM`) are
stored flat, at 3192, 8000, 64000 and 32000 bytes.

The codec is a zero-run escape: 0xff then a 16-bit little endian count, then
that many zero bytes.  All 851 escapes in SELECT.CGM have a zero high byte,
which is why an 8-bit count looks right there, but END1.CGM needs the full word
- read as bytes it decodes to 26245 and as words to 32133, and one plane is
32000.

Sizes land on the PC-98 screen: one plane of 640x400 is 32000 bytes, and every
mask decodes to about that (SELECT.CGM 31907, END1.CGM 32133, TITLE.CGM 32340),
while END1.CG and END2.CG come to about 128000 = four planes.  Short output is
padded with zeros and long output is kept, so the picture can be looked at
either way while the last few percent is still unexplained.
"""
import struct
import sys
import zlib

W, H = 640, 400
PLANE = W * H // 8

# The PC-98 boot palette, which is what the game leaves in place at these
# screens; entry n is (r, g, b) at 4 bits each scaled to 8.
BOOT = [(0, 0, 0), (0, 0, 7), (7, 0, 0), (7, 0, 7), (0, 7, 0), (0, 7, 7),
        (7, 7, 0), (7, 7, 7), (0, 0, 0), (0, 0, 3), (3, 0, 0), (3, 0, 3),
        (0, 3, 0), (0, 3, 3), (3, 3, 0), (3, 3, 3)]


def unpack(data):
    """`ff NN 00` is NN zero bytes; anything else is itself."""
    out = bytearray()
    i = 0
    n = len(data)
    while i < n:
        if data[i] == 0xff and i + 2 < n:
            out += b'\x00' * struct.unpack_from('<H', data, i + 1)[0]
            i += 3
        else:
            out.append(data[i])
            i += 1
    return bytes(out)


def load(path):
    """Returns (bytes, was_compressed)."""
    d = open(path, 'rb').read()
    if len(d) >= 2 and struct.unpack_from('<H', d, 0)[0] == len(d) - 2:
        return unpack(d[2:]), True
    return d, False


def png(path, w, h, rgb):
    """rgb is a bytes of 3*w*h."""
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


def render(d, planes):
    """Four planes give 16 colours; one gives black and white."""
    rgb = bytearray(W * H * 3)
    for y in range(H):
        for xb in range(W // 8):
            o = y * (W // 8) + xb
            bits = []
            for p in range(planes):
                k = p * PLANE + o
                bits.append(d[k] if k < len(d) else 0)
            for j in range(8):
                v = 0
                for p in range(planes):
                    v |= ((bits[p] >> (7 - j)) & 1) << p
                if planes == 1:
                    c = (255, 255, 255) if v else (0, 0, 0)
                else:
                    r, g, b = BOOT[v]
                    c = (r * 36, g * 36, b * 36)
                i = (y * W + xb * 8 + j) * 3
                rgb[i:i + 3] = bytes(c)
    return bytes(rgb)


def main():
    src, dst = sys.argv[1], sys.argv[2]
    d, packed = load(src)
    planes = 1 if ('--raw' in sys.argv or src.upper().endswith('M') or
                   len(d) < 2 * PLANE) else 4
    print('%s: %d bytes %s, %.4f planes -> %d plane%s' %
          (src, len(d), 'unpacked' if packed else 'flat', len(d) / PLANE,
           planes, '' if planes == 1 else 's'))
    png(dst, W, H, render(d, planes))
    print('wrote %s' % dst)


if __name__ == '__main__':
    main()
