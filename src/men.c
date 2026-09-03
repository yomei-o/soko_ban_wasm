/* SBPMEN.DAT, the thirty stages.
 *
 * The file is 7798 bytes and only 869 of them are non-zero, which is why it
 * looked like a score table at first: floor is a clear bit, so most of the
 * file is floor.  The non-zero bytes fall into a strict 250-byte period and
 * inside one period they sit at +0x0e, +0x5a and +0xb2 - four bytes into an
 * array at +0x0a, none into one at +0x5a and eight into one at +0xaa.  Three
 * arrays of 20 uint32 after a 10-byte header comes to exactly 250.
 *
 * Record 0 is all zeros, records 1..30 are the stages, and 48 bytes are left
 * on the end.  All thirty pass: box count equals goal count, nothing sits on
 * a wall, and the man starts on floor.
 *
 * WHICH PLANE IS WHICH comes out of FUN_1edb_338f, the original's own loader:
 *
 *     read 10 bytes of header;  man x = +1, man y = +2
 *     read three blocks of 0x50 bytes
 *     for row 0..0x13, col 0..0x1f:
 *         code = 4 * plane0_bit + 2 * plane1_bit + 1 * plane2_bit
 *         board[col + row * 0x20] = DS:0xba[code]
 *
 * and DS:0xba holds `00 02 01 09 03 04 09 09`, so
 *
 *     code 0  nothing        -> 0   floor
 *     code 1  plane2 only    -> 2   and FUN_1edb_2bb9 redraws a 2 as one of
 *                                   fifteen sprites, 0x1d..0x2b - the Kao
 *                                   product packages.  So plane2 is the BOXES
 *     code 2  plane1 only    -> 1   wall
 *     code 4  plane0 only    -> 3   which draws the blue dot: the GOALS
 *     code 5  plane0+plane2  -> 4   a box standing on a goal
 *     codes 3, 6, 7          -> 9   impossible, so filler
 *
 * The first pass at this had boxes and goals the other way round, which no
 * amount of staring at the boards could catch: the counts are equal and both
 * readings give a legal puzzle.  It is the sprite that settles it - the thing
 * drawn as a branded package is the box.
 *
 * `usermen.dat` is the sibling name in the executable's filename table, so
 * MEN is 面 and this is the built-in set.
 */
#include "men.h"

int men_load(Stage *out, const unsigned char *data, long len)
{
    int n = 0;

    if (!out || !data) return 0;
    for (n = 0; n < MEN_STAGES; n++) {
        const unsigned char *r = data + (long)(n + 1) * MEN_REC;
        Stage *s = &out[n];
        int k, p;

        if ((long)(n + 2) * MEN_REC > len) break;

        s->tile = r[0];
        s->sx = r[1];
        s->sy = r[2];
        s->moves = (unsigned short)(r[3] | (r[4] << 8));
        for (p = 0; p < 3; p++) {
            const unsigned char *a = r + 10 + p * 80;
            unsigned long *dst = p == 0 ? s->goal : (p == 1 ? s->wall : s->box);
            for (k = 0; k < MEN_ROWS; k++)
                dst[k] = ((unsigned long)a[k * 4] << 24) |
                         ((unsigned long)a[k * 4 + 1] << 16) |
                         ((unsigned long)a[k * 4 + 2] << 8) |
                         (unsigned long)a[k * 4 + 3];
        }

        s->w = 0;
        s->h = 0;
        for (k = 0; k < MEN_ROWS; k++) {
            unsigned long any = s->goal[k] | s->wall[k] | s->box[k];
            int x;
            if (!any) continue;
            s->h = k + 1;
            for (x = MEN_COLS - 1; x >= 0; x--)
                if (any & (1UL << (MEN_COLS - 1 - x))) {
                    if (x + 1 > s->w) s->w = x + 1;
                    break;
                }
        }
        men_fit(s);
    }
    return n;
}

/* Bit 31 of a row is column 0 - the PC-98 order, the same as the tiles. */
static int bit(const unsigned long *rows, int x, int y)
{
    if (x < 0 || y < 0 || x >= MEN_COLS || y >= MEN_ROWS) return 0;
    return (rows[y] >> (MEN_COLS - 1 - x)) & 1UL ? 1 : 0;
}

int men_wall(const Stage *s, int x, int y) { return bit(s->wall, x, y); }
int men_goal(const Stage *s, int x, int y) { return bit(s->goal, x, y); }
int men_box(const Stage *s, int x, int y) { return bit(s->box, x, y); }

/* The four display sizes, from DS:0x00a0, 0x00a8 and 0x00b0:
 *
 *     tile pixels   20  24  32  40
 *     grid width    31  25  19  15      as a last index, so 32/26/20/16 cells
 *     grid height   19  15  11   9      as a last index, so 20/16/12/10 cells
 *
 * 640 / 20 = 32 and 400 / 20 = 20, and so on down the row, so these are just
 * the screen divided by the tile. */
const int menTilePx[4] = { 20, 24, 32, 40 };
const int menGridW[4] = { 31, 25, 19, 15 };
const int menGridH[4] = { 19, 15, 11, 9 };

/* FUN_1edb_31c5: take the largest tile the board fits in, then centre the
 * board in that grid in whole cells.  It walks the index down from 3 and stops
 * at the first fit, comparing the board's LAST used row and column against the
 * tables - so a 15-wide board has a last index of 14.
 *
 * Every one of the thirty agrees with the header's first byte, which is why
 * that byte is the size index and not something else. */
void men_fit(Stage *s)
{
    int i;

    s->size = 0;
    for (i = 3; i >= 0; i--)
        if (s->w - 1 <= menGridW[i] && s->h - 1 <= menGridH[i]) {
            s->size = i;
            break;
        }
    s->tilePx = menTilePx[s->size];
    s->shiftX = (menGridW[s->size] - (s->w - 1)) / 2;
    s->shiftY = (menGridH[s->size] - (s->h - 1)) / 2;
}

int men_tile_px(const Stage *s)
{
    return s->tilePx ? s->tilePx : (s->tile >= 3 ? 40 : 32);
}
