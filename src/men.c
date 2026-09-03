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
            unsigned long *dst = p == 0 ? s->box : (p == 1 ? s->wall : s->goal);
            for (k = 0; k < MEN_ROWS; k++)
                dst[k] = ((unsigned long)a[k * 4] << 24) |
                         ((unsigned long)a[k * 4 + 1] << 16) |
                         ((unsigned long)a[k * 4 + 2] << 8) |
                         (unsigned long)a[k * 4 + 3];
        }

        s->w = 0;
        s->h = 0;
        for (k = 0; k < MEN_ROWS; k++) {
            unsigned long any = s->box[k] | s->wall[k] | s->goal[k];
            int x;
            if (!any) continue;
            s->h = k + 1;
            for (x = MEN_COLS - 1; x >= 0; x--)
                if (any & (1UL << (MEN_COLS - 1 - x))) {
                    if (x + 1 > s->w) s->w = x + 1;
                    break;
                }
        }
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

/* CHR98N.CG carries the tiles twice.  The wider set sits at y = 148 and 188
 * on a 40-pixel grid and the narrower at y = 84 and 116 on a 32-pixel one,
 * measured off the sheet: both bands are exactly two rows tall, 40 and 32. */
int men_tile_px(const Stage *s)
{
    return s->tile >= 3 ? 40 : 32;
}
