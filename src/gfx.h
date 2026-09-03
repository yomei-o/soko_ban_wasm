/* A 640x400 indexed framebuffer, and tiles cut out of CHR98N.CG.
 *
 * The original draws straight into the PC-98's four bit planes through the
 * GRCG; there is nothing to gain from imitating that here, so the port keeps
 * one byte per pixel and only the plane layout of the source art. */
#ifndef SOKO_GFX_H
#define SOKO_GFX_H

#include "cg.h"

#define GFX_W CG_W
#define GFX_H CG_H

typedef struct {
    unsigned char px[GFX_H][GFX_W];
    const unsigned char (*pal)[3];       /* 16 entries */
} Gfx;

void gfx_clear(Gfx *g, int colour);
void gfx_palette(Gfx *g, int which);     /* CG_PAL_* */

/* Copy a tile out of a sheet.  `t` is the tile's number along the two rows of
 * the sheet's own grid: CHR98N.CG carries the set twice, the 40-pixel band at
 * y = 148 and 188 and the 32-pixel band at y = 84 and 116.  Colour `clear` in
 * the source is left alone, which is how the floor shows through. */
void gfx_tile(Gfx *g, const Cg *sheet, int px, int t, int x, int y, int clear);

/* The whole sheet or part of it, with no transparency. */
void gfx_blit(Gfx *g, const Cg *src, int sx, int sy, int w, int h,
              int dx, int dy);

/* Where tile `t` sits on the sheet, for the sizes the game uses. */
void gfx_tile_at(int px, int t, int *sx, int *sy);

/* The tiles, in the order they lie on the sheet. */
enum {
    T_FLOOR = 0,
    T_WALL = 1,
    T_BOX_ON_GOAL = 2,
    T_GOAL = 3,
    T_BOX = 4,
    T_MAN = 5                            /* the first of the walking frames */
};

#endif
