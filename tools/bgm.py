"""Decode a .BGM into readable MML, using the grammar in docs/sound.md.

    python tools/bgm.py disk/SBPBGM0.BGM          all six tracks
    python tools/bgm.py disk/SBPBGM0.BGM 0        one track
    python tools/bgm.py disk/SBPBGM0.BGM 0 --bars  with a running tick count

Printing the stream is the only way to tell a wrong reading of the ranges from
a right one without a speaker: a right reading gives note lengths that add up
to whole bars and notes that stay inside a scale, and a wrong one gives
nonsense lengths and notes that jump about.
"""
import sys

NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B']


def tracks(d):
    """A track runs to a 0x00; 0xfc carries two argument bytes and anything
    above it one (MMD2.SYS 0x0460)."""
    out = []
    p = 0
    while p < len(d) and len(out) < 6:
        s = p
        while p < len(d):
            c = d[p]
            p += 1
            if c == 0:
                break
            if c < 0xfc:
                continue
            p += 1
            if c == 0xfc:
                p += 1
        out.append((s, p - 1))
    return out


def note_name(n):
    """The table at 0xd14 is twelve semitones in three fine steps, so the
    semitone is n // 3 and the remainder is a few cents of detune."""
    return NAMES[(n // 3) % 12] + ('' if n % 3 == 0 else '%+d' % (n % 3))


def decode(d, s, e):
    """(tick, text) for every code in one track."""
    out = []
    p = s
    tick = 0
    length = 12
    octave = 4
    while p <= e:
        code = d[p]
        p += 1
        if code == 0:
            out.append((tick, 'end'))
            break
        n = code - 1
        if n < 36:
            out.append((tick, '%-4s o%d l%d' % (note_name(n), octave, length)))
            tick += length
            continue
        n -= 36
        if n < 1:
            out.append((tick, 'rest l%d' % length))
            tick += length
            continue
        n -= 1
        if n < 2:
            octave += 1 if n else -1
            out.append((tick, 'octave %s -> %d' % ('+' if n else '-', octave)))
            continue
        n -= 2
        if n < 8:
            octave = n
            out.append((tick, 'octave = %d' % n))
            continue
        n -= 8
        if n < 2:
            out.append((tick, 'volume %s' % ('+' if n else '-')))
            continue
        n -= 2
        if n < 16:
            out.append((tick, 'volume = %d' % n))
            continue
        n -= 16
        if n < 32:
            out.append((tick, 'voice/noise = %d' % n))
            continue
        n -= 32
        if n < 16:
            out.append((tick, 'LOOP start x%s' % (n if n else 'inf')))
            continue
        n -= 16
        if n < 1:
            out.append((tick, 'LOOP end'))
            continue
        n -= 1
        if n < 16:
            out.append((tick, 'vibrato %d' % n))
            continue
        n -= 16
        if n < 16:
            out.append((tick, 'slide %d' % n))
            continue
        n -= 16
        if n < 4:
            length += -(2 - n) if n < 2 else (n - 1)
            out.append((tick, 'length %+d -> %d' % (-(2 - n) if n < 2 else n - 1,
                                                    length)))
            continue
        n -= 4
        if n < 91:
            length = n
            out.append((tick, 'length = %d' % n))
            continue
        n -= 91
        if n < 2:
            out.append((tick, 'tie %s' % ('on' if n else 'off')))
            continue
        n -= 2
        if n < 8:
            out.append((tick, 'q = %d' % n))
            continue
        n -= 8
        if n < 1:
            lo, hi = d[p], d[p + 1]
            p += 2
            out.append((tick, 'jump 0x%04x' % (lo | (hi << 8))))
            continue
        n -= 1
        arg = d[p]
        p += 1
        out.append((tick, ('mode = %d' if n < 1 else
                           'length = %d' if n < 2 else 'detune = %d') % arg))
        if n == 1:
            length = arg
    return out, tick


def main():
    path = sys.argv[1]
    want = int(sys.argv[2]) if len(sys.argv) > 2 and sys.argv[2].isdigit() else None
    bars = '--bars' in sys.argv
    d = open(path, 'rb').read()
    ts = tracks(d)
    print('%s: %d bytes, %d tracks (the first three FM, the last three SSG)'
          % (path, len(d), len(ts)))
    for k, (s, e) in enumerate(ts):
        if want is not None and k != want:
            continue
        rows, total = decode(d, s, e)
        print('--- track %d  bytes %d..%d (%d), %d ticks = %.2f s'
              % (k, s, e, e - s + 1, total, total / 54.1667))
        line = []
        for tick, text in rows:
            line.append(('%4d ' % tick if bars else '') + text)
            if len(line) == 4:
                print('    ' + ' | '.join('%-18s' % x for x in line))
                line = []
        if line:
            print('    ' + ' | '.join('%-18s' % x for x in line))


if __name__ == '__main__':
    main()
