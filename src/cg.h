/* The game's picture files, and the palettes that go with them. */
#ifndef SOKO_CG_H
#define SOKO_CG_H

#define CG_W 640
#define CG_H 400
#define CG_PLANE (CG_W * CG_H / 8)      /* 32000, and the loader's own limit */
#define CG_PLANES 4

/* Four planes of 32000 bytes, bit 7 of a byte being the leftmost pixel. */
typedef struct {
    unsigned char plane[CG_PLANES][CG_PLANE];
    int planes;                          /* how many were actually filled */
} Cg;

/* Decode one .CG / .CGM image.  `planes` is 4 for a picture and 1 for a mask;
 * the original picks 4 or 3 from [DS:0x90], a flag for the display it found.
 * A file whose leading word is not `size - 2` is stored flat and is split as
 * it lies.  Returns 0 on success. */
int cg_load(Cg *out, const unsigned char *data, long len, int planes);

/* Which bit of the palette index each plane carries: 0, 2, 1, 3, and not the
 * identity the plane order suggests.  The game's own colour-to-GRCG table says
 * so; cg.c has it. */
extern const int CG_PLANE_BIT[CG_PLANES];

/* The colour index of one pixel. */
int cg_pixel(const Cg *cg, int x, int y);

/* The three tables at DS:0x02a0, 0x02d0 and 0x0300, as r,g,b at 0..255.
 * 0 is the title's wood, 1 the tiles, 2 the ending. */
#define CG_PAL_TITLE 0
#define CG_PAL_TILES 1
#define CG_PAL_END 2
extern const unsigned char cgPalette[3][16][3];

#endif
