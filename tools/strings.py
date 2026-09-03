"""Printable runs out of a flat binary, with the ones that look like real text
kept and the ones that are code misread as text thrown away.

    python tools/strings.py <binary> [minlen]

A 16-bit code segment is full of bytes that happen to be in 0x20..0x7e, so a
plain strings(1) over SBP98.BIN gives thousands of lines of noise.  The filter
here is the proportion of characters that belong to a filename or a sentence:
letters, digits, space and a handful of punctuation.  Shift-JIS runs are picked
up separately, because the game's own messages are in it and a lead byte pair
looks like nothing in ASCII.
"""
import re
import sys

KEEP = set(" ._-:/" + chr(92) + "()[]<>!?,;'\"+*=%#&@$")


def ascii_runs(d, minlen):
    for m in re.finditer(rb"[\x20-\x7e]{%d,}" % minlen, d):
        t = m.group().decode("ascii")
        good = sum(1 for c in t if c.isalnum() or c in KEEP)
        if good >= len(t) * 0.85:
            yield m.start(), t


def sjis_runs(d, minlen):
    """Runs of Shift-JIS wide characters, which is where the messages are."""
    i = 0
    while i < len(d) - 1:
        j = i
        n = 0
        while j < len(d) - 1:
            lead = d[j]
            trail = d[j + 1]
            wide = ((0x81 <= lead <= 0x9f or 0xe0 <= lead <= 0xef) and
                    (0x40 <= trail <= 0x7e or 0x80 <= trail <= 0xfc))
            half = 0xa1 <= lead <= 0xdf
            if wide:
                j += 2
                n += 1
            elif half:
                j += 1
                n += 1
            else:
                break
        if n >= minlen:
            try:
                yield i, d[i:j].decode("cp932")
            except Exception:
                pass
            i = j
        else:
            i += 1


def main():
    path = sys.argv[1]
    minlen = int(sys.argv[2]) if len(sys.argv) > 2 else 6
    d = open(path, "rb").read()
    for at, t in ascii_runs(d, minlen):
        print("%06x  A  %s" % (at, t))
    for at, t in sjis_runs(d, 3):
        print("%06x  J  %s" % (at, t))


if __name__ == "__main__":
    main()
