/* Drawing.  One byte per pixel, and the art read straight out of the .CG. */
#include "gfx.h"

/* CHR98N.CG's own grid, measured off the decoded sheet.  Scanning for rows
 * that are neither background nor black gives four bands:
 *
 *     rows  84..115   32 tall     the narrow set, first row
 *     rows 116..147   32 tall     the narrow set, second row
 *     rows 148..187   40 tall     the wide set, first row
 *     rows 188..227   40 tall     the wide set, second row
 *
 * and cutting on a 40- or 32-pixel grid from x = 0 puts one whole sprite in
 * every cell with nothing clipped.  Two more bands at rows 289..318 and
 * 321..358 hold the Kao product packages that stand in for the boxes.
 */
static const int BAND32[2] = { 84, 116 };
static const int BAND40[2] = { 148, 188 };

/* Two more bands hold the fifteen Kao product packages that stand in for the
 * boxes: content at rows 289..318 and 321..358, which is a 32-pixel band from
 * y = 288 and a 40-pixel one from y = 320.  Fifteen of each, from x = 0:
 * 15 * 32 = 480 and 15 * 40 = 600, and the measured extents are exactly 480
 * and 600 wide. */
#define BOX_BAND32 288
#define BOX_BAND40 320

void gfx_tile_at(int px, int t, int *sx, int *sy)
{
    int perRow = GFX_W / px;

    if (t >= T_BOX) {
        int n = (t - T_BOX) % T_BOX_KINDS;
        *sx = n * px;
        *sy = px >= 40 ? BOX_BAND40 : BOX_BAND32;
        return;
    }
    {
        const int *band = px >= 40 ? BAND40 : BAND32;
        int row = t / perRow;
        if (row > 1) row = 1;
        *sx = (t % perRow) * px;
        *sy = band[row];
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

/* Which sprite the man is.  Rendering tiles 0..30 large and numbering them
 * with the game's own digits makes the groups plain:
 *
 *      0 floor   1 wall   2 box on goal   3 goal   4 box
 *   5..7  pushing right      19..21 walking right
 *   8..10 pushing left       22..24 walking left
 *  11..13 pushing up         25..27 walking up    (the back of the cap)
 *  16..18 pushing down       28..30 walking down  (the face towards you)
 *
 * Row one of the sheet only reaches x = 560, so tiles 14 and 15 are padding
 * and the "pushing down" group starts row two.  Three frames each, so a phase
 * of 0..2 picks one.
 */
static const int MAN_WALK[4] = { 25, 19, 28, 22 };   /* up right down left */
static const int MAN_PUSH[4] = { 11, 5, 16, 8 };

int gfx_man(int dir, int pushing, int phase)
{
    const int *g = pushing ? MAN_PUSH : MAN_WALK;
    if (dir < 0 || dir > 3) dir = 0;
    if (phase < 0) phase = 0;
    return g[dir] + phase % 3;
}
