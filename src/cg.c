/* Decoding the game's .CG / .CGM pictures.
 *
 * The loader is at 2406:09xx in SBP98.EXE; out/sbp98.asm has it in full.
 * Written out:
 *
 *     fread(&skip, 1, 2, fp)              the leading word, thrown away
 *     planes = ([0x90] == 1) ? 4 : 3      2406:0a0d
 *     for p in 0..planes-1:
 *         dst = seg[p]                    from the table at DS:0x0330, which
 *         di = 0                          is literally 00 a8 00 b0 00 b8 00 e0
 *         while di < 0x7d00:              32000 = one plane, 2406:0aaa
 *             fread(&c, 1, 1, fp)
 *             *dst++ = c                  written BEFORE the test
 *             if c == 0x00 or c == 0xff:  2406:0a68 and 2406:0a6e
 *                 fread(&n, 1, 2, fp)
 *                 while n - 1 > si++:     2406:0aa5
 *                     *dst++ = c
 *                 di += n - 1
 *             di++
 *
 * So a run is the byte itself followed by a 16-bit count that INCLUDES that
 * first byte, and 0x00 escapes exactly as 0xff does.  Missing the 0x00 case is
 * what made every picture shear when this was being guessed from the data: a
 * mask is mostly zeros, so the stream is full of `00 NN NN` and each one put
 * the decoder two bytes out of step.  Each plane also stops dead at 32000,
 * which is why running the planes together left every size a few hundred
 * bytes short of a whole plane.
 */
#include "cg.h"

/* The three palettes sit just before the plane-segment table, sixteen entries
 * of three nibbles in R G B order.  Scaled by 17 here so a nibble of 0xf
 * becomes 0xff. */
const unsigned char cgPalette[3][16][3] = {
    {   /* DS:0x02a0 - TITLE */
        { 0x00, 0x00, 0x00 },
        { 0xcc, 0x88, 0x44 },
        { 0xee, 0x99, 0x33 },
        { 0xff, 0xaa, 0x44 },
        { 0xaa, 0x77, 0x33 },
        { 0xcc, 0x88, 0x44 },
        { 0xff, 0xaa, 0x66 },
        { 0xff, 0xcc, 0x66 },
        { 0xbb, 0x99, 0x66 },
        { 0xff, 0xdd, 0x99 },
        { 0xff, 0xff, 0x33 },
        { 0x00, 0xcc, 0xff },
        { 0xff, 0x00, 0x00 },
        { 0xff, 0x00, 0xff },
        { 0x00, 0xaa, 0x00 },
        { 0xff, 0xff, 0xff }
    },
    {   /* DS:0x02d0 - TILES */
        { 0x00, 0x00, 0x00 },
        { 0x00, 0x00, 0x00 },
        { 0x99, 0x44, 0x00 },
        { 0xff, 0xdd, 0xbb },
        { 0xdd, 0x33, 0x22 },
        { 0xff, 0xbb, 0x33 },
        { 0xdd, 0xbb, 0xaa },
        { 0x00, 0x11, 0xff },
        { 0x00, 0xcc, 0x99 },
        { 0x99, 0x77, 0x44 },
        { 0x00, 0x99, 0x66 },
        { 0x88, 0xff, 0x00 },
        { 0x66, 0x55, 0x11 },
        { 0x00, 0x22, 0xaa },
        { 0xff, 0xff, 0x00 },
        { 0xff, 0xff, 0xff }
    },
    {   /* DS:0x0300 - END */
        { 0x00, 0x00, 0x00 },
        { 0x00, 0x00, 0xaa },
        { 0xff, 0xaa, 0xaa },
        { 0xff, 0xcc, 0xaa },
        { 0xdd, 0x00, 0x00 },
        { 0xee, 0xbb, 0x77 },
        { 0xcc, 0xcc, 0x00 },
        { 0x88, 0x55, 0x11 },
        { 0xbb, 0xbb, 0xbb },
        { 0x00, 0x00, 0xff },
        { 0x00, 0x88, 0x00 },
        { 0x88, 0xff, 0xff },
        { 0xff, 0x00, 0x00 },
        { 0xff, 0x00, 0xff },
        { 0xff, 0xff, 0x00 },
        { 0xff, 0xff, 0xff }
    }
};

/* WHICH PLANE IS WHICH BIT of the palette index.
 *
 * The planes are loaded in the file's own order - A800, B000, B800, E000, the
 * segment table at DS:0x0330 - and the obvious reading is that plane p is bit
 * p, which is what the PC-98's documented layout (A800 blue, B000 red, B800
 * green, E000 intensity; code = I G R B) comes to.  It is wrong for this game,
 * and the game says so itself.
 *
 * To draw in colour c the graphics library loads the GRCG's four tile
 * registers - one per plane, in plane order - from a 16 x 4 table of 00 / ff
 * at CS:0x1c2b, file offset 0xb18b:
 *
 *     colour  1   ff 00 00 00      bit 0 -> plane 0   A800
 *     colour  2   00 00 ff 00      bit 1 -> plane 2   B800
 *     colour  4   00 ff 00 00      bit 2 -> plane 1   B000
 *     colour  8   00 00 00 ff      bit 3 -> plane 3   E000
 *
 * so bits 1 and 2 sit on the other plane from the documented layout, and the
 * pictures have to be read the same way round as the game draws them.
 *
 * Three things go right the moment they are:
 *
 *   * FUN_1edb_109b fills the play screen with colour 3 of the tile palette,
 *     #ffddbb, and the floor tile then decodes as 3 as well.  Read plane p as
 *     bit p and the floor is 5, #ffbb33, an orange square on a cream page.
 *   * The NIVEA tin, sprite 0x24, is one flat colour: 13, #0022aa, instead of
 *     11, #88ff00.  It is a blue tin.
 *   * A box standing on its goal is the 花王 crate, and its moon and 花王 come
 *     out 10, #009966 - the green mark - instead of 12, #665511.
 */
const int CG_PLANE_BIT[CG_PLANES] = { 0, 2, 1, 3 };

int cg_load(Cg *out, const unsigned char *data, long len, int planes)
{
    long i = 0;
    int p;

    if (!out || !data || len < 2) return -1;
    if (planes < 1) planes = 1;
    if (planes > CG_PLANES) planes = CG_PLANES;

    for (p = 0; p < CG_PLANES; p++) {
        int k;
        for (k = 0; k < CG_PLANE; k++) out->plane[p][k] = 0;
    }
    out->planes = planes;

    /* A file whose leading word is not `size - 2` was never packed - FONT.CG,
     * LOGO.CG, STAFF1.CG and WINDOWS.CGM - so it is split as it lies. */
    if ((long)(data[0] | (data[1] << 8)) != len - 2) {
        long left = len;
        for (p = 0; p < planes && left > 0; p++) {
            long n = left < CG_PLANE ? left : CG_PLANE;
            long k;
            for (k = 0; k < n; k++) out->plane[p][k] = data[p * CG_PLANE + k];
            left -= n;
        }
        out->planes = p ? p : 1;
        return 0;
    }

    i = 2;                              /* the length word is read and dropped */
    for (p = 0; p < planes; p++) {
        long at = 0;
        while (at < CG_PLANE && i < len) {
            unsigned char c = data[i++];
            long run = 1;
            if ((c == 0x00 || c == 0xff) && i + 1 < len) {
                run = (long)(data[i] | (data[i + 1] << 8));
                i += 2;
                if (run < 1) run = 1;   /* a count of 0 still writes the byte */
            }
            if (run > CG_PLANE - at) run = CG_PLANE - at;
            while (run--) out->plane[p][at++] = c;
        }
    }
    return 0;
}

int cg_pixel(const Cg *cg, int x, int y)
{
    long o;
    int bit, v = 0, p;

    if (!cg || x < 0 || y < 0 || x >= CG_W || y >= CG_H) return 0;
    o = (long)y * (CG_W / 8) + x / 8;
    bit = 7 - (x & 7);
    for (p = 0; p < cg->planes; p++)
        v |= ((cg->plane[p][o] >> bit) & 1) << CG_PLANE_BIT[p];
    return v;
}
