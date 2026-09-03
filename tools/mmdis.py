"""Flow-following 16-bit disassembler, for MMD2.SYS and anything like it.

    python tools/mmdis.py disk/MMD2.SYS map           what is reachable
    python tools/mmdis.py disk/MMD2.SYS fn 0x41       one routine
    python tools/mmdis.py disk/MMD2.SYS dis 0x100 0x80
    python tools/mmdis.py disk/MMD2.SYS ports         every in/out, by port
    python tools/mmdis.py disk/MMD2.SYS xref 0x123    who reaches an address

A DOS character-device driver starts with an 18-byte header:

    +0x00  4  the link to the next device, ffff:ffff for the last
    +0x04  2  attributes
    +0x06  2  the strategy entry
    +0x08  2  the interrupt entry
    +0x0a  8  the name, padded with spaces

MMD2.SYS says strategy 0x38, interrupt 0x41 and calls itself MMD200OR, so the
two entries are where a sweep has to start.  A linear sweep is no good here:
the file has its register tables and note tables between the routines, and a
misread table desynchronises everything after it.

Ghidra will not do this on its own - a raw binary import has no entry point, so
`-loader BinaryLoader` produces zero instructions.
"""
import os
import struct
import sys

import capstone

PORTS = {
    0x00: 'PIC ICW/EOI', 0x02: 'PIC mask', 0x08: 'PIC2', 0x0a: 'PIC2 mask',
    0x31: 'system port', 0x32: 'sound switch',
    0x71: 'PIT ctrl', 0x73: 'PIT ch0', 0x75: 'PIT ch1', 0x77: 'PIT ch2',
    0x188: 'OPN addr', 0x18a: 'OPN data',
    0x18c: 'OPNA addr2', 0x18e: 'OPNA data2',
    0xa460: 'OPNA id', 0xa466: 'OPNA rhythm',
}

COND = ('je', 'jz', 'jne', 'jnz', 'jl', 'jnl', 'jg', 'jng', 'jle', 'jge',
        'jb', 'jnb', 'jbe', 'jae', 'ja', 'js', 'jns', 'jo', 'jno', 'jp',
        'jnp', 'jcxz', 'loop', 'loope', 'loopne')
STOP = ('ret', 'retf', 'iret', 'hlt', 'jmp', 'ljmp')


class Dis:
    def __init__(self, path, entries):
        self.d = open(path, 'rb').read()
        self.md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_16)
        self.md.detail = True
        self.ins = {}
        self.xref = {}
        self.starts = set(entries)
        self.walk(entries)

    def one(self, at):
        if at in self.ins:
            return self.ins[at]
        for i in self.md.disasm(self.d[at:at + 16], at):
            self.ins[at] = i
            return i
        return None

    def walk(self, entries):
        todo = list(entries)
        seen = set()
        while todo:
            at = todo.pop()
            while 0 <= at < len(self.d) and at not in seen:
                seen.add(at)
                i = self.one(at)
                if i is None:
                    break
                m = i.mnemonic
                tgt = None
                if m in COND or m in ('call', 'jmp'):
                    op = i.op_str
                    if op.startswith('0x'):
                        try:
                            tgt = int(op, 16)
                        except ValueError:
                            tgt = None
                if tgt is not None and 0 <= tgt < len(self.d):
                    self.xref.setdefault(tgt, set()).add(at)
                    if m == 'call':
                        self.starts.add(tgt)
                    todo.append(tgt)
                if m in STOP:
                    break
                at += i.size
        self.seen = seen

    def text(self, at):
        i = self.ins.get(at)
        if i is None:
            return '%04x  db %02x' % (at, self.d[at])
        raw = ' '.join('%02x' % b for b in i.bytes)
        return '%04x  %-20s %-7s %s' % (at, raw, i.mnemonic, i.op_str)

    def body(self, at):
        """A routine, following straight through to its ret."""
        out = []
        while at < len(self.d):
            i = self.ins.get(at)
            if i is None:
                break
            out.append(at)
            if i.mnemonic in STOP:
                break
            at += i.size
            if at in self.starts and len(out) > 1:
                break
        return out


def main():
    path = sys.argv[1]
    what = sys.argv[2] if len(sys.argv) > 2 else 'map'
    d = open(path, 'rb').read()

    strategy, interrupt = struct.unpack_from('<HH', d, 6)
    name = d[10:18].decode('ascii', 'replace')
    print('%s: %d bytes, strategy 0x%02x, interrupt 0x%02x, name %r' %
          (os.path.basename(path), len(d), strategy, interrupt, name))

    p = Dis(path, [strategy, interrupt])

    if what == 'map':
        print('%d of %d bytes reachable (%.0f%%), %d routines' %
              (len(p.seen), len(d), 100.0 * len(p.seen) / len(d),
               len(p.starts)))
        for a in sorted(p.starts):
            n = len(p.body(a))
            who = len(p.xref.get(a, ()))
            print('  %04x  %3d ins  %d callers' % (a, n, who))
    elif what == 'fn':
        for a in p.body(int(sys.argv[3], 16)):
            print('  ' + p.text(a))
    elif what == 'dis':
        a = int(sys.argv[3], 16)
        end = a + (int(sys.argv[4], 16) if len(sys.argv) > 4 else 0x60)
        while a < end:
            print('  ' + p.text(a))
            i = p.ins.get(a)
            a += i.size if i else 1
    elif what == 'ports':
        hits = {}
        for a in sorted(p.seen):
            i = p.ins.get(a)
            if i is None or i.mnemonic not in ('in', 'out'):
                continue
            hits.setdefault(i.op_str, []).append(a)
        for k in sorted(hits):
            print('  %-18s %s' % (k, ' '.join('%04x' % a for a in hits[k])))
    elif what == 'xref':
        t = int(sys.argv[3], 16)
        for a in sorted(p.xref.get(t, ())):
            print('  ' + p.text(a))
    else:
        print('unknown command %s' % what)


if __name__ == '__main__':
    main()
