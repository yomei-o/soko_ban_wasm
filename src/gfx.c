/* Drawing.  One byte per pixel, and the art read straight out of the .CG. */
#include "gfx.h"

/* CHR98N.CG's own grid.
 *
 * Scanning the decoded sheet for rows that are neither background nor black
 * gives four bands for the characters and two for the boxes:
 *
 *     rows  84..115   32 tall     the narrow set, first row,  448 wide
 *     rows 116..147   32 tall     the narrow set, second row, 480 wide
 *     rows 148..187   40 tall     the wide set, first row,    560 wide
 *     rows 188..227   40 tall     the wide set, second row,   600 wide
 *     rows 289..318   32 tall     the boxes, narrow, 480 wide
 *     rows 321..358   40 tall     the boxes, wide,   600 wide
 *
 * so the FIRST row holds fourteen sprites and the second fifteen, and the box
 * band fifteen.  14 + 15 + 15 = 44, which is exactly the size of the image
 * table FUN_1edb_2bb9 indexes: `DAT_29ca_1287 * 0xb0 + 0x4024 + n * 4`, with
 * 0xb0 = 176 bytes = 44 far pointers a size.
 *
 * Reading the first row as sixteen wide - the obvious guess from 640 / 40 -
 * puts everything from sprite 14 on in the wrong place.
 */
#define BAND1_32 84
#define BAND2_32 116
#define BAND1_40 148
#define BAND2_40 188
#define BOXES_32 288
#define BOXES_40 320
#define ROW1_COUNT 14
#define ROW2_COUNT 15

void gfx_tile_at(int px, int t, int *sx, int *sy)
{
    int wide = px >= 40;

    if (t >= T_BOX) {
        *sx = ((t - T_BOX) % T_BOX_KINDS) * px;
        *sy = wide ? BOXES_40 : BOXES_32;
    } else if (t < ROW1_COUNT) {
        *sx = t * px;
        *sy = wide ? BAND1_40 : BAND1_32;
    } else {
        int n = t - ROW1_COUNT;
        if (n >= ROW2_COUNT) n = ROW2_COUNT - 1;
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
