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

void gfx_tile_at(int px, int t, int *sx, int *sy)
{
    const int *band = px >= 40 ? BAND40 : BAND32;
    int perRow = GFX_W / px;
    int row = t / perRow;

    if (row > 1) row = 1;
    *sx = (t % perRow) * px;
    *sy = band[row];
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
