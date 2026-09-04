"""Decode a .CG the way src/cg.c does, for measuring colours from python.

    import cgpy
    cg = cgpy.load('disk/CHR98N.CG')
    cg.px(x, y)          -> the 0..15 palette index
    cgpy.PAL[1]          -> the tile palette, 16 (r, g, b)

Nothing here is used by the game; it exists so that a claim about a colour can
be checked against the file rather than against a screenshot.
"""
import io

W, H = 640, 400
PLANE = W * H // 8


class Cg(object):
    def __init__(self, planes):
        self.plane = planes

    def px(self, x, y):
        o = y * (W // 8) + x // 8
        bit = 7 - (x & 7)
        v = 0
        for p in range(len(self.plane)):
            v |= ((self.plane[p][o] >> bit) & 1) << p
        return v


def load(path, planes=4):
    d = io.open(path, 'rb').read()
    out = [bytearray(PLANE) for _ in range(planes)]
    if (d[0] | (d[1] << 8)) != len(d) - 2:          # not packed
        for p in range(planes):
            chunk = d[p * PLANE:(p + 1) * PLANE]
            out[p][:len(chunk)] = chunk
        return Cg(out)
    i = 2
    for p in range(planes):
        at = 0
        while at < PLANE and i < len(d):
            c = d[i]
            i += 1
            run = 1
            if (c == 0x00 or c == 0xff) and i + 1 < len(d):
                run = d[i] | (d[i + 1] << 8)
                i += 2
                if run < 1:
                    run = 1
            if run > PLANE - at:
                run = PLANE - at
            for _ in range(run):
                out[p][at] = c
                at += 1
    return Cg(out)


def _pal(at):
    """The sixteen entries of one palette, straight out of SBP98.EXE."""
    d = io.open('disk/SBP98.EXE', 'rb').read()
    return [tuple(((d[at + i * 3 + k] & 0xf) * 17) for k in range(3))
            for i in range(16)]


PAL = {0: _pal(0x1b140), 1: _pal(0x1b170), 2: _pal(0x1b1a0)}
