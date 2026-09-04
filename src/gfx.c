/* Drawing.  One byte per pixel, and the art read straight out of the .CG. */
#include "gfx.h"

/* CHR98N.CG's own grid.
 *
 * Four bands for the characters and two for the boxes:
 *
 *     rows  84..115   32 tall     the narrow set, first row
 *     rows 116..147   32 tall     the narrow set, second row
 *     rows 148..187   40 tall     the wide set, first row
 *     rows 188..227   40 tall     the wide set, second row
 *     rows 289..318   32 tall     the boxes, narrow
 *     rows 321..358   40 tall     the boxes, wide
 *
 * Each size has 29 sprites and 15 boxes, which is exactly the size of the
 * image table FUN_1edb_2bb9 indexes: `DAT_29ca_1287 * 0xb0 + 0x4024 + n * 4`,
 * with 0xb0 = 176 bytes = 44 far pointers a size, and 29 + 15 = 44.
 *
 * BUT THE TWO SIZES BREAK THEIR ROWS IN DIFFERENT PLACES.  The wide set is
 * 14 then 15; the narrow set is **17 then 12**.  An unused slot is a solid
 * square of the floor colour, so the slots can be counted by looking for the
 * tiles with no colour 0 in them at all - every sprite has black in its
 * outline.  Non-zero pixels a tile, with the bar where the sprites stop:
 *
 *     narrow row 1   1024 1024 1024 1024 900 811 ... 843 | 1024 1024 1024
 *     narrow row 2   826 869 ... 869 | 1024 x8
 *     wide row 1     1600 1600 1600 1600 1444 ... 1378 | 1600 1600
 *     wide row 2     1378 ... 1404 | 1600
 *
 * Reading the narrow set as 14 + 15 puts sprite 14 - the first frame of the
 * man pushing UPWARDS - on sprite 17, so on the seven boards drawn small an
 * upward push showed a man walking sideways instead.  The rows were measured
 * by scanning for "not background and not black", and the three white-armed
 * pushing frames slip through that.
 */
#define BAND1_32 84
#define BAND2_32 116
#define BAND1_40 148
#define BAND2_40 188
#define BOXES_32 288
#define BOXES_40 320
#define ROW1_32 17
#define ROW1_40 14
#define SPRITES 29

void gfx_tile_at(int px, int t, int *sx, int *sy)
{
    int wide = px >= 40;
    int row1 = wide ? ROW1_40 : ROW1_32;

    if (t >= T_BOX) {
        *sx = ((t - T_BOX) % T_BOX_KINDS) * px;
        *sy = wide ? BOXES_40 : BOXES_32;
    } else if (t < row1) {
        *sx = t * px;
        *sy = wide ? BAND1_40 : BAND1_32;
    } else {
        int n = t - row1;
        if (n >= SPRITES - row1) n = SPRITES - row1 - 1;
        *sx = n * px;
        *sy = wide ? BAND2_40 : BAND2_32;
    }
}

void gfx_clear(Gfx *g, int colour)
{
    int y, x;
    for (y = 0; y < GFX_H; y++)
        for (x = 0; x < GFX_W; x++)
            g->px[y][x] = (unsigned char)colour;
}

void gfx_palette(Gfx *g, int which)
{
    g->pal = cgPalette[which];
}

void gfx_blit(Gfx *g, const Cg *src, int sx, int sy, int w, int h,
              int dx, int dy)
{
    int y, x;
    for (y = 0; y < h; y++) {
        int ty = dy + y;
        if (ty < 0 || ty >= GFX_H) continue;
        for (x = 0; x < w; x++) {
            int tx = dx + x;
            if (tx < 0 || tx >= GFX_W) continue;
            g->px[ty][tx] = (unsigned char)cg_pixel(src, sx + x, sy + y);
        }
    }
}

void gfx_tile(Gfx *g, const Cg *sheet, int px, int t, int x, int y, int clear)
{
    int sx, sy, dy, dx;

    gfx_tile_at(px, t, &sx, &sy);
    for (dy = 0; dy < px; dy++) {
        int ty = y + dy;
        if (ty < 0 || ty >= GFX_H) continue;
        for (dx = 0; dx < px; dx++) {
            int tx = x + dx;
            int v;
            if (tx < 0 || tx >= GFX_W) continue;
            v = cg_pixel(sheet, sx + dx, sy + dy);
            if (v == clear) continue;
            g->px[ty][tx] = (unsigned char)v;
        }
    }
}

/* Which sprite the man is.  FUN_1edb_2c10 works it out at 1edb:2c7e:
 *
 *     AX = [0x11bf] * 3                  the facing
 *     AX += (1 - kind) * 0xc             kind 1 means a box is coming along
 *     DX = AX + [0x9e] + 5               [0x9e] is the walk frame, 0..2
 *
 * so the table is
 *
 *      0..4    floor, wall, the crossed crate, goal, box on goal
 *      5..16   pushing, four facings of three frames
 *     17..28   walking, the same again
 *     29..43   the fifteen Kao packages a plain box is drawn as
 *
 * and the facing is 0 left, 1 right, 2 down, 3 up - the order the trace
 * replay decodes at 1edb:1d65..1daa.
 */
int gfx_man(int dir, int pushing, int phase)
{
    if (dir < 0 || dir > 3) dir = 0;
    if (phase < 0) phase = 0;
    return 5 + dir * 3 + (pushing ? 0 : 12) + phase % 3;
}
