/* FONT.CG.  See font.h for where the layout came from. */
#include "font.h"

int font_load(Font *f, const unsigned char *data, long len)
{
    int style, g, r;

    f->ok = 0;
    if (!f || !data) return -1;
    if (len < (long)FONT_REC * FONT_STYLES * FONT_GLYPHS) return -2;

    for (style = 0; style < FONT_STYLES; style++)
        for (g = 0; g < FONT_GLYPHS; g++) {
            const unsigned char *rec =
                data + (long)(style * FONT_GLYPHS + g) * FONT_REC;
            /* The two constants are in every record, so they are a cheap
             * check that the stride is right. */
            if (rec[0] != 0x06 || rec[2] != 0x08) return -3;
            if (rec[40] != 0xfe || rec[41] != 0x23) return -4;
            for (r = 0; r < FONT_H; r++)
                f->row[style][g][r] = rec[4 + r * 4];
        }
    f->ok = 1;
    return 0;
}

int font_ink(const Font *f, int style, int glyph, int gx, int gy)
{
    if (!f->ok) return 0;
    if (style < 0 || style >= FONT_STYLES) return 0;
    if (glyph < 0 || glyph >= FONT_GLYPHS) return 0;
    if (gx < 0 || gx >= FONT_W || gy < 0 || gy >= FONT_H) return 0;
    /* Bit 7 is column 0, and ink is the CLEAR bit. */
    return ((f->row[style][glyph][gy] >> (7 - gx)) & 1) ? 0 : 1;
}

int font_glyph(int ch)
{
    if (ch >= 'A' && ch <= 'Z') return ch - 'A';
    if (ch >= 'a' && ch <= 'z') return ch - 'a';
    if (ch >= '0' && ch <= '9') return FONT_DIGIT0 + (ch - '0');
    return -1;
}
