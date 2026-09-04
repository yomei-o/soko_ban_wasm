/* The ending, checked without a screen.
 *
 *     tmp/end_check.exe
 *
 * Run from the repo root; it reads disk/.
 *
 * FUN_1edb_40bc is an endless loop of five pictures and three kinds of
 * scatter, and the whole of it is in app.h.  What is checked here is the shape
 * of that loop rather than the pixels of any one frame:
 *
 *   * the grid starts it by itself once every stage is cleared
 *   * the screen is scattered away to black before the first picture
 *   * END1 lands exactly, pixel for pixel
 *   * the credits go ON TOP of it - colour 0 of STAFF1 is transparent, so the
 *     picture still shows through around the letters
 *   * END2 replaces END1, black and all
 *   * the last scatter leaves the staff4 strip on an otherwise black screen
 *   * and it comes round again
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"

static int fails;

static void ok(int cond, const char *what)
{
    if (!cond) { printf("FAIL %s\n", what); fails++; }
}

static App app;
static Cg pic;

static unsigned char *slurp(const char *name, long *len)
{
    char path[128];
    FILE *f;
    unsigned char *b;
    long n;

    snprintf(path, sizeof path, "disk/%s", name);
    f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    b = (unsigned char *)malloc((size_t)n);
    if (!b || fread(b, 1, (size_t)n, f) != (size_t)n) { free(b); fclose(f); return NULL; }
    fclose(f);
    *len = n;
    return b;
}

/* How many pixels of the screen are not the picture's own. */
static long differs(const Cg *cg)
{
    long n = 0;
    int y, x;
    for (y = 0; y < GFX_H; y++)
        for (x = 0; x < GFX_W; x++)
            if (app.gfx.px[y][x] != (unsigned char)cg_pixel(cg, x, y)) n++;
    return n;
}

static long count(int colour)
{
    long n = 0;
    int y, x;
    for (y = 0; y < GFX_H; y++)
        for (x = 0; x < GFX_W; x++)
            if (app.gfx.px[y][x] == colour) n++;
    return n;
}

/* Run the ending until it reaches a step.  Counting frames would work too,
 * but then the test would break every time OVER_FRAMES or a wait is tuned,
 * and what is being checked here is the order, not the pacing. */
static void reach(int step, const char *what)
{
    int guard = 0;
    while (app.endStep != step && app.screen == SCR_END && guard++ < 40000)
        app_tick(&app);
    ok(app.endStep == step, what);
}

static void load(const char *name, int planes)
{
    long len;
    unsigned char *d = slurp(name, &len);
    if (!d) { printf("FAIL cannot read disk/%s\n", name); exit(1); }
    cg_load(&pic, d, len, planes);
    free(d);
}

int main(void)
{
    long len;
    unsigned char *d;
    int n;

    if (app_init(&app, "disk")) { printf("FAIL app_init\n"); return 1; }
    d = slurp("SBPMEN.DAT", &len);
    if (!d) { printf("FAIL cannot read disk/SBPMEN.DAT\n"); return 1; }
    free(d);

    /* 1edb:053d asks the question every turn of the grid's loop. */
    app.screen = SCR_SELECT;
    app_tick(&app);
    ok(app.screen == SCR_SELECT, "an unfinished game stays on the grid");

    for (n = 0; n < MEN_STAGES; n++) app.record[n] = 1;
    app_render(&app);                    /* the grid is what gets erased */
    app_tick(&app);
    ok(app.screen == SCR_END, "all thirty cleared starts the ending");
    ok(app.song == BGM_END, "and it plays SBPBGM1");

    /* 00f9 first: the screen goes away before anything is drawn. */
    reach(END_IN1, "the erase runs first");
    ok(count(0) == (long)GFX_W * GFX_H, "the screen is scattered away to black");

    /* then 01f5 puts END1 on, black and all. */
    reach(END_HOLD1, "then END1 is scattered on");
    load("END1.CG", 4);
    ok(differs(&pic) == 0, "END1 lands exactly");

    /* The credits go on top with colour 0 transparent, so both are on the
     * screen at once. */
    reach(END_HOLD2, "the credits follow");
    load("STAFF1.CG", 2);
    {
        long over = 0, under = 0;
        int y, x;
        for (y = 0; y < GFX_H; y++)
            for (x = 0; x < GFX_W; x++) {
                int v = cg_pixel(&pic, x, y);
                if (v) { if (app.gfx.px[y][x] == v) over++; }
                else under++;
            }
        ok(over > 5000, "the credits are on the screen");
        ok(under > 100000, "and most of it is still the picture underneath");
    }
    load("END1.CG", 4);
    ok(differs(&pic) > 5000, "so the screen is no longer just END1");

    /* Then END1 comes back, wiping the credits, and END2 replaces it. */
    reach(END_IN2, "the buffer comes back");
    ok(differs(&pic) == 0, "which wipes the credits off again");
    reach(END_STAFF3, "END2 follows");
    load("END2.CG", 4);
    ok(differs(&pic) == 0, "END2 replaces it");

    /* staff3, then staff4 after its wait, and both sit on top of END2. */
    reach(END_STAFF4, "SPECIAL THANKS goes on top");
    ok(differs(&pic) > 2000, "and it is on the screen");
    reach(END_HOLD3, "the copyright line follows");
    ok(differs(&pic) > 2000, "and it is on the screen too");

    /* 0019: the hidden page - black plus that strip - replaces the screen. */
    reach(END_HOLD4, "the last scatter runs");
    {
        long lit = 0, outside = 0;
        int y, x;
        for (y = 0; y < GFX_H; y++)
            for (x = 0; x < GFX_W; x++) {
                if (!app.gfx.px[y][x]) continue;
                lit++;
                if (y < 200 || y >= 200 + 0x1d) outside++;
            }
        ok(lit > 1000, "the copyright line is still lit");
        ok(outside == 0, "and nothing else is");
    }

    /* And round again. */
    reach(END_HOLD1, "the loop comes round");
    load("END1.CG", 4);
    ok(differs(&pic) == 0, "back to END1");

    if (fails) printf("%d checks failed\n", fails);
    else printf("all ending checks passed\n");
    return fails ? 1 : 0;
}
