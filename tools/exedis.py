"""Read SBP98.EXE without Ghidra.

Ghidra is not on every machine this moves between, and `out/sbp98.c` /
`out/sbp98.asm` are in .gitignore, so they are not in the clone.  This is the
substitute: capstone in 16-bit mode plus the two address maps the executable
needs, which is enough to answer "what does 1edb:40bc do" and "who calls it".

    python tools/exedis.py fn 1edb:40bc [bytes]   disassemble there
    python tools/exedis.py fn 0x1406c             a file offset works too
    python tools/exedis.py calls 1edb:40bc        every far call to it
    python tools/exedis.py data 0x40e [bytes]     hex and text at DS:offset
    python tools/exedis.py str 0x40e              the string at DS:offset
    python tools/exedis.py find 9a 1d 00 d7 14    a byte pattern, hex

THE TWO MAPS

* Code.  A Ghidra address `SSSS:OOOO` is a file offset of
  `0x1200 + ((SSSS - 0x1000) << 4) + OOOO`.  0x1200 is the header (18 paragraphs
  at +0x08) and the 0x1000 is Ghidra's image base, which is why the segments in
  the listing are 0x1000 above the ones in the file's own far calls.
* Data.  DS sits at file 0x1aea0.  The three palettes at DS:0x02a0, 0x02d0 and
  0x0300 land on 0x1b140, 0x1b170 and 0x1b1a0, and the plane segment table that
  follows them at DS:0x0330 reads `00 a8 00 b0 00 b8 00 e0` - which is how the
  base was pinned down.

MMD2.SYS is a different animal - a flat device driver - and tools/mmdis.py
already reads it.
"""
import os
import struct
import sys

import capstone

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXE = os.path.join(ROOT, 'disk', 'SBP98.EXE')
CODE_BASE = 0x1200                      # the MZ header is 18 paragraphs
GHIDRA_BASE = 0x1000                    # Ghidra's image base, in paragraphs
DS = 0x1aea0                            # where the data segment lands


def load():
    with open(EXE, 'rb') as f:
        return f.read()


def to_file(where):
    """`1edb:40bc`, `0x1406c` or `1406c` -> a file offset."""
    if ':' in where:
        seg, off = where.split(':')
        seg = int(seg, 16) - GHIDRA_BASE
        return CODE_BASE + (seg << 4) + int(off, 16)
    return int(where, 16)


def to_ghidra(at, seg):
    """A file offset back to SSSS:OOOO inside the segment it was reached in."""
    return '%04x:%04x' % (seg + GHIDRA_BASE,
                          at - CODE_BASE - (seg << 4))


def disasm(d, at, n, seg=None):
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_16)
    for ins in md.disasm(d[at:at + n], at):
        tag = ''
        if seg is not None:
            tag = ' ' + to_ghidra(ins.address, seg)
        print('  %06x%s  %-18s %s %s'
              % (ins.address, tag, ins.bytes.hex(), ins.mnemonic, ins.op_str))


def far_calls(d, target):
    """Every `lcall seg:off` in the image that lands on `target`."""
    out = []
    for i in range(len(d) - 5):
        if d[i] != 0x9a:
            continue
        off, seg = struct.unpack_from('<HH', d, i + 1)
        if CODE_BASE + (seg << 4) + off == target:
            out.append(i)
    return out


def main():
    what = sys.argv[1] if len(sys.argv) > 1 else 'help'
    d = load()

    if what == 'fn':
        at = to_file(sys.argv[2])
        n = int(sys.argv[3], 0) if len(sys.argv) > 3 else 0x80
        seg = None
        if ':' in sys.argv[2]:
            seg = int(sys.argv[2].split(':')[0], 16) - GHIDRA_BASE
        disasm(d, at, n, seg)
    elif what == 'calls':
        target = to_file(sys.argv[2])
        hits = far_calls(d, target)
        print('%d far calls to %06x' % (len(hits), target))
        for h in hits:
            print('== %06x' % h)
            disasm(d, max(0, h - 0x18), 0x18 + 5)
    elif what == 'data':
        off = int(sys.argv[2], 0)
        n = int(sys.argv[3], 0) if len(sys.argv) > 3 else 0x40
        for a in range(DS + off, DS + off + n, 16):
            row = d[a:a + 16]
            text = ''.join(chr(b) if 32 <= b < 127 else '.' for b in row)
            print('%04x  %-47s  %s'
                  % (a - DS, ' '.join('%02x' % b for b in row), text))
    elif what == 'str':
        a = DS + int(sys.argv[2], 0)
        print(repr(bytes(d[a:a + 64]).split(b'\x00')[0]))
    elif what == 'find':
        pat = bytes(int(x, 16) for x in sys.argv[2:])
        hits, i = [], -1
        while True:
            i = d.find(pat, i + 1)
            if i < 0:
                break
            hits.append(i)
        print('%d hits: %s' % (len(hits), ' '.join('%06x' % h for h in hits)))
    else:
        print(__doc__)


if __name__ == '__main__':
    main()
