/* FONT.CG, the game's own lettering.
 *
 * The file is 76 records of exactly 42 bytes:
 *
 *     +0x00  06 00 08 00     the same in every record
 *     +0x04  36 bytes        nine rows of four identical bytes - the four
 *                            planes, so the glyphs are one colour
 *     +0x28  fe 23           the same in every record
 *
 * INK IS A CLEAR BIT, the same way round as WINDOWS.CGM, and only columns
 * 0..6 belong to the glyph: bit 0 of a row byte is padding, and dropping it
 * makes FONT.CG's digits identical to the ones sitting in WINDOWS.CGM.
 *
 * 76 = 2 x 38, and FUN_2329_0506 picks a glyph as
 *
 *     sprite = digit + style * 0x26 + 0x1b
 *
 * so a style is 38 glyphs and the digits are at 27..36 inside one.  Reading
 * those out gives 0..9, and 0..25 are A..Z.  Two styles, drawn in whatever
 * colour the caller wants: the stage grid uses one on white and the other on
 * red.
 *
 * FUN_2329_0506 also steps 9 pixels a digit, which is the pitch the digits in
 * WINDOWS.CGM sit at.
 */
#ifndef SOKO_FONT_H
#define SOKO_FONT_H

#define FONT_REC 42
#define FONT_GLYPHS 38                  /* per style */
#define FONT_STYLES 2
#define FONT_W 7                         /* columns that belong to the glyph */
#define FONT_H 9
#define FONT_PITCH 9                     /* FUN_2329_0506's step */

#define FONT_DIGIT0 27                   /* 0x1b */

typedef struct {
    unsigned char row[FONT_STYLES][FONT_GLYPHS][FONT_H];
    int ok;
} Font;

int font_load(Font *f, const unsigned char *data, long len);

/* Is (gx, gy) ink?  gx 0..6, gy 0..8. */
int font_ink(const Font *f, int style, int glyph, int gx, int gy);

/* 'A'..'Z' and '0'..'9' to a glyph index, or -1. */
int font_glyph(int ch);

#endif
