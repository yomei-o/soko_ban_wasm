/* Everything that must hold about the thirty stages and the rules.
 *
 *     tmp/game_check.exe
 *
 * Run from the repo root; it reads disk/SBPMEN.DAT.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cg.h"
#include "game.h"
#include "gfx.h"
#include "men.h"

static int fails;

static void ok(int cond, const char *what, ...)
{
    if (!cond) {
        printf("FAIL %s\n", what);
        fails++;
    }
}

static void okn(int cond, const char *what, int n)
{
    if (!cond) {
        printf("FAIL %s (stage %d)\n", what, n);
        fails++;
    }
}

static unsigned char *slurp(const char *path, long *len)
{
    FILE *f = fopen(path, "rb");
    unsigned char *b;
    long n;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    b = malloc((size_t)n);
    if (!b || fread(b, 1, (size_t)n, f) != (size_t)n) { free(b); fclose(f); return NULL; }
    fclose(f);
    *len = n;
    return b;
}

static int count(const unsigned long *rows)
{
    int k, n = 0;
    for (k = 0; k < MEN_ROWS; k++) {
        unsigned long v = rows[k];
        while (v) { n += (int)(v & 1); v >>= 1; }
    }
    return n;
}

int main(void)
{
    static Stage st[MEN_STAGES];
    unsigned char *d;
    long len;
    int n, i, boxTotal = 0;

    d = slurp("disk/SBPMEN.DAT", &len);
    if (!d) { printf("FAIL cannot read disk/SBPMEN.DAT\n"); return 1; }
    ok(len == 7798, "SBPMEN.DAT is 7798 bytes");
    n = men_load(st, d, len);
    ok(n == MEN_STAGES, "thirty stages parse");

    for (i = 0; i < n; i++) {
        const Stage *s = &st[i];
        int x, y, nb = count(s->box), ng = count(s->goal);

        /* If any of these fail the record layout is being read wrongly, which
         * is exactly how the 250-byte / three-plane shape was confirmed. */
        okn(nb == ng, "box count equals goal count", i + 1);
        okn(nb > 0, "the stage has boxes", i + 1);
        okn(!men_wall(s, s->sx, s->sy), "the man starts on floor", i + 1);
        okn(s->tile == 2 || s->tile == 3, "tile size is 2 or 3", i + 1);
        okn(s->moves > 0, "the target move count is set", i + 1);
        okn(s->w > 0 && s->w <= MEN_COLS, "width in range", i + 1);
        okn(s->h > 0 && s->h <= MEN_ROWS, "height in range", i + 1);
        for (y = 0; y < MEN_ROWS; y++)
            for (x = 0; x < MEN_COLS; x++) {
                if (men_wall(s, x, y) && men_box(s, x, y))
                    okn(0, "no box on a wall", i + 1);
                if (men_wall(s, x, y) && men_goal(s, x, y))
                    okn(0, "no goal on a wall", i + 1);
            }
        boxTotal += nb;
    }
    ok(boxTotal == 243, "243 boxes over the thirty stages");

    /* The seven widest boards are the ones drawn small. */
    {
        int small = 0;
        for (i = 0; i < n; i++) if (st[i].tile == 2) small++;
        ok(small == 7, "seven stages use the small tiles");
    }

    /* Stage 1's own numbers, so a change in the reader shows up here. */
    ok(st[0].w == 8 && st[0].h == 6, "stage 1 is 8x6");
    ok(st[0].sx == 6 && st[0].sy == 3, "stage 1 starts at (6,3)");
    ok(st[0].moves == 71, "stage 1 asks for 71 moves");
    ok(st[10].moves == 1389, "stage 11 asks for 1389 moves");
    ok(st[25].moves == 2001, "stage 26 asks for 2001 moves");
    ok(men_tile_px(&st[0]) == 40, "tile 3 is 40 pixels");
    ok(men_tile_px(&st[10]) == 32, "tile 2 is 32 pixels");

    /* The rules. */
    {
        Game g;
        game_start(&g, &st[0], 1);
        ok(g.x == 6 && g.y == 3, "the man is placed");
        ok(g.boxes == 4 && g.done == 0, "four boxes, none home");
        ok(!game_won(&g), "not won at the start");

        /* Stage 1 row 3 is #......# with the man at 6 and a box at 1, so a
         * step left is free and a step right is into the wall. */
        ok(game_step(&g, DIR_LEFT) == 1, "left is free");
        ok(g.x == 5 && g.moves == 1, "the step counted");
        ok(game_step(&g, DIR_RIGHT) == 1, "back right");
        ok(g.x == 6 && g.moves == 2, "and counted again");
        ok(game_step(&g, DIR_RIGHT) == 0, "the wall refuses");
        ok(g.moves == 2, "a refused step is not counted");
        ok(g.facing == DIR_RIGHT, "but the man still turns");

        ok(game_undo(&g) == 1, "undo");
        ok(g.x == 5 && g.moves == 1, "undo puts the man back");
        ok(game_undo(&g) == 1 && g.x == 6 && g.moves == 0, "undo to the start");
        ok(game_undo(&g) == 0, "nothing left to undo");
    }

    /* A push and its undo.  Stage 24 is the symmetric one: row 3 reads
     * `# *@* #`, so the man starts between two boxes that are already home
     * and can shove either of them off its goal and back on again. */
    {
        Game g;
        int before;
        game_start(&g, &st[23], 24);
        before = g.done;
        ok(g.boxes == 8, "stage 24 has eight boxes");
        /* stage 24 row 3 is # *@* #, so the man pushes either way */
        ok(game_step(&g, DIR_RIGHT) == 1, "stage 24: push right");
        ok(g.pushes == 1, "the push counted");
        ok(game_undo(&g) == 1, "undo the push");
        ok(g.pushes == 0 && g.done == before, "the box came back");
        ok(g.moves == 0, "and the step went with it");
    }

    /* Undo has to restore the board exactly, whatever the run. */
    {
        Game g, ref;
        const int walk[] = { DIR_UP, DIR_RIGHT, DIR_DOWN, DIR_LEFT,
                             DIR_RIGHT, DIR_RIGHT, DIR_UP, DIR_DOWN };
        int k, taken = 0;
        game_start(&g, &st[5], 6);
        ref = g;
        for (k = 0; k < (int)(sizeof walk / sizeof *walk); k++)
            taken += game_step(&g, walk[k]);
        while (game_undo(&g)) ;
        ok(taken > 0, "the walk moved at least once");
        ok(g.x == ref.x && g.y == ref.y, "undo returns the man");
        ok(memcmp(g.box, ref.box, sizeof g.box) == 0, "undo returns the boxes");
        ok(g.moves == 0 && g.pushes == 0, "and the counters");
    }

    free(d);

    /* THE COLOURS OF THE TILE SHEET.
     *
     * cg.c reads plane p as bit CG_PLANE_BIT[p] of the palette index, which is
     * 0, 2, 1, 3 and not the identity, because the game's own colour-to-GRCG
     * table says so.  Read it as the identity and every one of these fails:
     * the floor stops matching the page it is drawn on, the NIVEA tin turns
     * yellow-green, and the 花王 mark on a crate that has reached its goal
     * turns olive.  That last one is what a player notices. */
    {
        static Cg sheet;
        int x, y, hit[16], i;

        ok(CG_PLANE_BIT[0] == 0 && CG_PLANE_BIT[1] == 2 &&
           CG_PLANE_BIT[2] == 1 && CG_PLANE_BIT[3] == 3,
           "the planes carry bits 0, 2, 1, 3");

        d = slurp("disk/CHR98N.CG", &len);
        if (!d) { printf("FAIL cannot read disk/CHR98N.CG\n"); return 1; }
        cg_load(&sheet, d, len, 4);
        free(d);

        for (i = 0; i < 16; i++) hit[i] = 0;
        for (y = 0; y < 40; y++)
            for (x = 0; x < 40; x++) hit[cg_pixel(&sheet, x, 148 + y)]++;
        ok(hit[3] == 40 * 40, "the floor tile is colour 3, the play ground");

        for (i = 0; i < 16; i++) hit[i] = 0;
        for (y = 0; y < 40; y++)
            for (x = 0; x < 40; x++)
                hit[cg_pixel(&sheet, T_BOX_ON_GOAL * 40 + x, 148 + y)]++;
        ok(hit[10] > 200 && hit[12] == 0,
           "the 花王 mark on a box that is home is green, not olive");
        ok(hit[14] > 800, "and it stands on yellow");

        for (i = 0; i < 16; i++) hit[i] = 0;
        for (y = 0; y < 40; y++)
            for (x = 0; x < 40; x++) hit[cg_pixel(&sheet, 7 * 40 + x, 320 + y)]++;
        ok(hit[13] > 1000 && hit[11] == 0, "the NIVEA tin is blue");
    }

    if (fails) printf("%d checks failed\n", fails);
    else printf("all checks passed (%d stages)\n", n);
    return fails ? 1 : 0;
}
