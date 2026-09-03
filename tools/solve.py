"""Solve a stage, so a run can be checked against the port's own engine.

    python tools/solve.py disk/SBPMEN.DAT 1

Prints the move string in the letters tmp/soko_shot.exe --press takes, so the
answer can be handed straight to the real engine:

    ./tmp/soko_shot.exe board 1 tmp/out.png --press <the string>

Plain breadth-first search over (man, boxes).  The boards here are small - the
biggest is 18x11 with sixteen boxes - so the only stages this finishes on
quickly are the early ones; that is enough for its purpose, which is checking
the port rather than playing the game.
"""
import sys
from collections import deque

sys.path.insert(0, 'tools')
import men as M

DIRS = [(0, -1, 'u'), (1, 0, 'r'), (0, 1, 'd'), (-1, 0, 'l')]


def load(path, n):
    data = open(path, 'rb').read()
    for k, s in M.stages(data):
        if k == n:
            return s
    raise SystemExit('no stage %d' % n)


def solve(s, limit=4000000):
    walls = set()
    goals = set()
    boxes = set()
    for y in range(M.ROWS):
        for x in range(M.COLS):
            wall, box, goal = s.at(x, y)
            if wall:
                walls.add((x, y))
            if goal:
                goals.add((x, y))
            if box:
                boxes.add((x, y))

    start = ((s.sx, s.sy), frozenset(boxes))
    if boxes == goals:
        return ''

    # A box in a corner that is not a goal can never move again.
    dead = set()
    for y in range(M.ROWS):
        for x in range(M.COLS):
            if (x, y) in walls or (x, y) in goals:
                continue
            up = (x, y - 1) in walls
            dn = (x, y + 1) in walls
            lf = (x - 1, y) in walls
            rt = (x + 1, y) in walls
            if (up or dn) and (lf or rt):
                dead.add((x, y))

    seen = {start: None}
    q = deque([start])
    n = 0
    while q:
        n += 1
        if n > limit:
            return None
        state = q.popleft()
        (mx, my), bs = state
        for dx, dy, ch in DIRS:
            nx, ny = mx + dx, my + dy
            if (nx, ny) in walls:
                continue
            nb = bs
            if (nx, ny) in bs:
                bx, by = nx + dx, ny + dy
                if (bx, by) in walls or (bx, by) in bs or (bx, by) in dead:
                    continue
                nb = frozenset((bs - {(nx, ny)}) | {(bx, by)})
            nxt = ((nx, ny), nb)
            if nxt in seen:
                continue
            seen[nxt] = (state, ch)
            if nb == goals:
                out = []
                cur = nxt
                while seen[cur] is not None:
                    prev, c = seen[cur]
                    out.append(c)
                    cur = prev
                return ''.join(reversed(out))
            q.append(nxt)
    return None


def main():
    path = sys.argv[1]
    n = int(sys.argv[2])
    s = load(path, n)
    print('stage %d  %dx%d  %d boxes  limit %d moves' %
          (n, s.w, s.h, s.count(M.BOXES), s.moves))
    for line in s.art():
        print('    |%s|' % line)
    ans = solve(s)
    if ans is None:
        print('not solved within the search limit')
        return 1
    print('%d moves: %s' % (len(ans), ans))
    print('the stage asks for %d, so this is %s' %
          (s.moves, 'inside the limit' if len(ans) <= s.moves else 'over it'))
    return 0


if __name__ == '__main__':
    sys.exit(main())
