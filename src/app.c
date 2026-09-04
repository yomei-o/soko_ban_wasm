/* The screens.
 *
 * Everything with an address in a comment came out of out/sbp98.c or
 * out/sbp98.asm; anything else is this port's own choice and says so.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"

/* FUN_1edb_109b does setfillstyle(SOLID, 3) then bar(0, 0, 0x27f, 399) before
 * anything else, so the board sits on colour 3 of the tile palette. */
#define PLAY_GROUND 3

/* FUN_2329_000d's colours: the panel and an uncleared cell are 0x0f, a cleared
 * one is 0x0c, and the border round each cell is 0. */
#define SEL_PANEL 15
#define SEL_DONE 12
#define SEL_EDGE 0

/* The loading screen has a palette of its own, four entries of it, straight
 * out of FUN_1edb_000e:
 *
 *     setrgbpalette(0,  15, 15, 15)     the ground: white
 *     setrgbpalette(9,  15, 15, 15)
 *     setrgbpalette(13,  0, 11,  0)     the logo: green
 *     setrgbpalette(4,  15, 15, 15)
 *
 * then, once the files check out, it walks i from 0 to 15 doing
 *
 *     setrgbpalette(13, i, i / 4 + 12, i)
 *
 * which takes 13 from green up to white so the logo dissolves into the
 * ground, and finishes with setrgbpalette(13, 0, 0, 0) - the logo coming
 * back in black.  Then the title loads.
 *
 * The dwell at each stage is this port's; the original's is however long the
 * disk takes. */
#define BOOT_GROUND 0
#define BOOT_INK 13
#define BOOT_HOLD_TICKS 40               /* the original waits on the disk */
#define BOOT_FADE_TICKS 4                /* per palette step */

/* The order in FUN_1edb_000e, after the file check passes:
 *
 *     FUN_23b0_03a9()                          the Kao crescent rises
 *     wait
 *     for i in 0..15:                          colour 13 green -> white,
 *         setrgbpalette(13, i, i/4+12, i)      so it dissolves into the
 *         wait                                 white ground
 *     FUN_23b0_03a9()                          and scrolls on out of sight
 *     setrgbpalette(13, 0, 0, 0)               13 turns black
 *     FUN_23b0_03ef()                          THINKING RABBIT rises, in black
 *
 * so the two logos are never up together: the first is green and fades away,
 * the second is black.
 */
enum {
    BOOT_RISE_A, BOOT_WAIT, BOOT_FADE, BOOT_RISE_A2, BOOT_RISE_B, BOOT_DONE
};

/* One pass of FUN_23b0_03a9 / FUN_23b0_03ef: the band moves up a row. */
static void logo_scroll(App *a, int row, int rows)
{
    int r;
    for (r = row; r < row + rows && r < GFX_H; r++) {
        unsigned char *src = a->logoPlane + r * LOGO_STRIDE + LOGO_BAND_BYTE;
        unsigned char *dst = src - LOGO_STRIDE;
        int k;
        if (r == 0) continue;
        for (k = 0; k < LOGO_BAND_WORDS * 2; k++) dst[k] = src[k];
    }
}

/* FUN_23b0_03ef's `rep movsw`: 8000 bytes back by 27, which slides the second
 * logo out of its staging half and into the band. */
static void logo_bring_second(App *a)
{
    long from = (long)LOGO_Y * LOGO_STRIDE + LOGO_B_FROM;
    long to = (long)LOGO_Y * LOGO_STRIDE + LOGO_BAND_BYTE;
    long n = LOGO_ROWS * LOGO_STRIDE;
    long k;
    for (k = 0; k < n; k++) {
        if (from + k >= LOGO_PLANE || to + k >= LOGO_PLANE) break;
        a->logoPlane[to + k] = a->logoPlane[from + k];
    }
}

static void boot_palette(App *a, int r, int g, int b)
{
    int i, j;
    for (i = 0; i < 16; i++)
        for (j = 0; j < 3; j++) a->bootPal[i][j] = 0;
    for (i = 0; i < 3; i++) {
        a->bootPal[0][i] = 255;
        a->bootPal[9][i] = 255;
        a->bootPal[4][i] = 255;
    }
    a->bootPal[BOOT_INK][0] = (unsigned char)(r * 17);
    a->bootPal[BOOT_INK][1] = (unsigned char)(g * 17);
    a->bootPal[BOOT_INK][2] = (unsigned char)(b * 17);
    a->gfx.pal = a->bootPal;
}

/* FUN_1edb_2c10 divides a square into [0x11e8] / cc_step sub-steps.  cc_step
 * lives in [0x121b], written by code Ghidra could not reach, and the game
 * bails out with "cc_step=0 でした!!" if it is ever zero - so it is a speed
 * setting.  Four pixels gives ten sub-steps on a 40-pixel tile and eight on a
 * 32-pixel one, which is this port's choice of that setting. */
#define CC_STEP 4

/* FUN_1edb_2c10 advances the walk frame every third sub-step. */
#define PHASE_EVERY 3

static unsigned char *slurp(const char *dir, const char *name, long *len)
{
    char path[512];
    FILE *f;
    unsigned char *b;
    long n;

    snprintf(path, sizeof path, "%s/%s", dir, name);
    f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    b = malloc((size_t)n);
    if (!b || fread(b, 1, (size_t)n, f) != (size_t)n) {
        free(b);
        fclose(f);
        return NULL;
    }
    fclose(f);
    *len = n;
    return b;
}

static int load_cg(Cg *out, const char *dir, const char *name, int planes)
{
    long len;
    unsigned char *d = slurp(dir, name, &len);
    int rc;

    if (!d) return -1;
    rc = cg_load(out, d, len, planes);
    free(d);
    return rc;
}

int app_init(App *a, const char *dir)
{
    long len;
    unsigned char *d;
    int k;

    memset(a, 0, sizeof *a);
    /* The ending loads its five pictures as it needs them, so where they came
     * from has to outlive app_init. */
    snprintf(a->dir, sizeof a->dir, "%s", dir ? dir : ".");
    if (load_cg(&a->title, dir, "TITLE.CG", 4)) return -1;
    if (load_cg(&a->select, dir, "SELECT.CG", 4)) return -2;
    if (load_cg(&a->chr, dir, "CHR98N.CG", 4)) return -3;
    if (load_cg(&a->windows, dir, "WINDOWS.CGM", 1)) return -4;
    if (load_cg(&a->logo, dir, "LOGO.CG", 1)) return -7;
    /* The two overlays and their masks.  CLEARM.CG is a cloud-shaped hole and
     * CLEAR.CG the picture that shows through it; PEKE is the same pair for
     * the failure. */
    if (load_cg(&a->clear, dir, "CLEAR.CG", 4)) return -12;
    if (load_cg(&a->clearMask, dir, "CLEARM.CG", 1)) return -13;
    if (load_cg(&a->peke, dir, "PEKE.CG", 4)) return -14;
    if (load_cg(&a->pekeMask, dir, "PEKEM.CG", 1)) return -15;

    d = slurp(dir, "FONT.CG", &len);
    if (!d) return -8;
    if (font_load(&a->font, d, len)) { free(d); return -9; }
    free(d);

    d = slurp(dir, "SBPVOICE.VOI", &len);
    if (!d) return -10;
    a->voiLen = len < (long)sizeof a->voi ? len : (long)sizeof a->voi;
    memcpy(a->voi, d, (size_t)a->voiLen);
    free(d);
    for (k = 0; k < BGM_COUNT; k++) {
        char name[32];
        snprintf(name, sizeof name, "SBPBGM%d.BGM", k);
        d = slurp(dir, name, &len);
        if (!d) return -11;
        a->bgmLen[k] = len < (long)sizeof a->bgm[k] ? len
                                                   : (long)sizeof a->bgm[k];
        memcpy(a->bgm[k], d, (size_t)a->bgmLen[k]);
        free(d);
    }
    mmd2_reset(&a->mmd);
    a->song = -1;
    a->music = 1;
    a->songNext = -1;

    d = slurp(dir, "SBPMEN.DAT", &len);
    if (!d) return -5;
    a->stageCount = men_load(a->stages, d, len);
    free(d);
    if (a->stageCount < MEN_STAGES) return -6;

    a->screen = SCR_BOOT;
    a->step = CC_STEP;
    a->pick = 0;
    a->dirty = 1;
    /* LOGO.CG straight into the plane at row 245, the way FUN_2406_01b7
     * leaves it. */
    {
        int y, xb;
        memset(a->logoPlane, 0, sizeof a->logoPlane);
        for (y = 0; y < LOGO_ROWS; y++)
            for (xb = 0; xb < LOGO_STRIDE; xb++)
                a->logoPlane[(LOGO_Y + y) * LOGO_STRIDE + xb] =
                    a->logo.plane[0][y * LOGO_STRIDE + xb];
    }
    a->bootPhase = BOOT_RISE_A;
    a->bootStep = 0;
    boot_palette(a, 0, 11, 0);
    /* The original's first FUN_24d7_001d is at 1edb:03a7, right before the
     * stage grid, so the logo and the title are silent. */
    a->song = -1;
    return 0;
}

/* DIR_UP/RIGHT/DOWN/LEFT to the original's [0x11bf].  The encoding is at
 * 1edb:1d65..1daa, where the trace replay turns its two recorded bits into a
 * facing: not-horizontal and not-positive is 3 with dy = -1, not-horizontal
 * and positive is 2 with dy = +1, horizontal and not-positive is 0 with
 * dx = -1, horizontal and positive is 1 with dx = +1. */
int app_facing(int dir)
{
    switch (dir) {
    case DIR_LEFT: return 0;
    case DIR_RIGHT: return 1;
    case DIR_DOWN: return 2;
    default: return 3;                   /* DIR_UP */
    }
}

static void song_start(App *a, int n)
{
    mmd2_play(&a->mmd, a->bgm[n], a->bgmLen[n], a->voi, a->voiLen);
    a->song = n;
    a->audioAcc = 0;
}

/* [0x128d] guards every FUN_24d7_001d call, so turning the music off just
 * means never starting one.
 *
 * Every bgm(n) in the game reaches FUN_24d7_00a8 with its flag zero, which is
 * the fading arm: AH=6 at speed [0x04a6], then `while (AH=8) ;`, then the new
 * song.  Even asking for the song that is already playing goes the long way
 * round - 1edb:1554 does exactly that when a stage starts again. */
void app_music(App *a, int n)
{
    if (!a->music || n < 0 || n >= BGM_COUNT) {
        mmd2_stop(&a->mmd);
        a->song = -1;
        a->songNext = -1;
        a->waitWhat = WAIT_NOTHING;
        return;
    }
    if (a->mmd.playing) {
        mmd2_fade(&a->mmd, BGM_FADE_SPEED);
        a->songNext = n;
        a->songWait = 0;
        return;
    }
    song_start(a, n);
}

/* Is FUN_24d7_00a8 still spinning? */
static int music_waiting(const App *a)
{
    return a->songNext >= 0;
}

void app_audio(App *a, short *out, int frames, int rate)
{
    mmd2_run(&a->mmd, out, frames, rate, &a->audioAcc);
    /* Every track opens with an endless loop, so a song that stops has really
     * run out; restarting keeps the screen from going quiet.  A song that has
     * been faded out has also stopped, and that one must be left alone: the
     * wait in app_tick is watching for exactly this. */
    if (!a->mmd.playing && a->song >= 0 && !music_waiting(a)) {
        int n = a->song;
        a->song = -1;
        app_music(a, n);
    }
}

int app_cell(int x, int y)
{
    int col, row;

    if (x <= 0x1f || x >= 0x260 || y <= 0x27 || y >= 0x17c) return 0;
    col = (x - SEL_X0) / SEL_CW;
    row = (y - SEL_Y0) / SEL_CH;
    if (col < 0 || col >= SEL_COLS || row < 0 || row >= SEL_ROWS) return 0;
    return row * SEL_COLS + col + 1;
}

int app_busy(const App *a)
{
    return a->animLeft > 0 || music_waiting(a);
}

/* Tick until the slide is over.  Scripted play and the checks need this; a
 * host with a frame loop never does. */
void app_settle(App *a)
{
    int guard = 0;
    while ((a->animLeft > 0 || music_waiting(a)) && guard++ < 8192) app_tick(a);
}

/* The board, once the music is settled. */
static void play_now(App *a, int stage)
{
    const Stage *s;

    s = &a->stages[stage - 1];
    game_start(&a->game, s, stage);
    /* FUN_1edb_31c5 picks the size from the board's extent and shifts the
     * board into the middle of that grid in whole cells, so the origin is
     * always a multiple of the tile. */
    a->px = s->tilePx;
    a->ox = s->shiftX * a->px;
    a->oy = s->shiftY * a->px;
    /* FUN_1edb_2bb9 turns a box into 0x1d + [0x3ed7] % 15 and [0x3ed7] comes
     * from the clock once a stage, so every box in a stage is the same
     * product.  Keying it to the stage keeps a screenshot repeatable. */
    a->boxKind = (stage - 1) % T_BOX_KINDS;
    a->animLeft = 0;
    a->animTick = 0;
    a->animDx = 0;
    a->animDy = 0;
    a->phase = 0;
    a->facing = app_facing(DIR_DOWN);
    a->lastSprite = gfx_man(a->facing, 0, 0);
    a->result = RESULT_PLAYING;
    a->overStep = 0;
    a->overTick = 0;
    a->screen = SCR_PLAY;
    gfx_palette(&a->gfx, CG_PAL_TILES);
    a->dirty = 1;
}

/* 1edb:1053: the stage's music is asked for at the top of the function, so
 * the grid stays on the screen until the fade is over. */
void app_play(App *a, int stage)
{
    if (stage < 1 || stage > a->stageCount) return;
    /* FUN_1edb_042c only enters a stage whose record is under 0x3a99. */
    if (a->record[stage - 1] > SEL_LOCKED) return;

    app_music(a, BGM_PLAY);
    if (music_waiting(a)) {
        a->waitWhat = WAIT_PLAY;
        a->waitArg = stage;
        return;
    }
    play_now(a, stage);
}

/* And the same for the way back to the grid. */
static void select_now(App *a)
{
    a->screen = SCR_SELECT;
    gfx_palette(&a->gfx, CG_PAL_TITLE);
    a->dirty = 1;
}

static void go_select(App *a)
{
    app_music(a, BGM_SELECT);
    if (music_waiting(a)) {
        a->waitWhat = WAIT_SELECT;
        return;
    }
    select_now(a);
}

/* --- the ending, FUN_1edb_40bc -------------------------------------------
 *
 * app.h has the whole loop written out.  What follows is the three effects it
 * scatters with and the state machine that walks them.
 */

/* 2406:042f: a staff strip.  Eighty bytes a row and 0x1d = 29 rows, read into
 * VRAM offset 16000 - row 200, the middle of the screen.  The file holds two
 * planes' worth and the seek between the reads is absolute (0x910), so the
 * SECOND half is read three times over, into b000, b800 and e000: the strip
 * has four colours, 0, 1, 14 and 15. */
#define STAFF_ROW 200
#define STAFF_ROWS 0x1d
#define STAFF_BYTES ((GFX_W / 8) * STAFF_ROWS)           /* 0x910 */

static int load_staff_strip(Cg *out, const char *dir, const char *name)
{
    long len, k;
    unsigned char *d = slurp(dir, name, &len);
    int p;

    if (!d) return -1;
    memset(out, 0, sizeof *out);
    out->planes = CG_PLANES;
    for (p = 0; p < CG_PLANES; p++) {
        long src = p == 0 ? 0 : STAFF_BYTES;
        for (k = 0; k < STAFF_BYTES; k++) {
            long at = (long)STAFF_ROW * (GFX_W / 8) + k;
            if (src + k < len) out->plane[p][at] = d[src + k];
        }
    }
    free(d);
    return 0;
}

enum { SCAT_OVER, SCAT_REPLACE, SCAT_ERASE };

/* One pass of the 42-byte scatter over the whole screen: the word at
 * `pass * 2`, then every OVER_STRIDE bytes, sixteen pixels at a time.
 *
 * SCAT_ERASE does it four pixels at a time - 0x00f9 ANDs with ~ror(0xf0,cl)
 * for cl = 0, 4, 8, 12, which is pixels 0..3, 4..7, 8..11 and 12..15 of the
 * word - and the four sub-passes run back to back, as they do there. */
static void scatter_pass(App *a, const Cg *src, int pass, int mode)
{
    long o;

    for (o = (long)pass * 2; o < CG_PLANE - 1; o += OVER_STRIDE) {
        int row = (int)(o / (GFX_W / 8)), col = (int)(o % (GFX_W / 8));
        int k;
        if (row >= GFX_H) break;
        for (k = 0; k < 16; k++) {
            int x = col * 8 + k, v;
            if (x >= GFX_W) break;
            if (mode == SCAT_ERASE) { a->gfx.px[row][x] = 0; continue; }
            v = cg_pixel(src, x, row);
            if (mode == SCAT_OVER && !v) continue;
            a->gfx.px[row][x] = (unsigned char)v;
        }
    }
}

/* A pass every OVER_FRAMES frames, twenty-one of them.  Returns 1 when the
 * whole scatter is done. */
static int end_scatter(App *a, const Cg *src, int mode)
{
    if (++a->endFrame < OVER_FRAMES) return 0;
    a->endFrame = 0;
    scatter_pass(a, src, a->endPass, mode);
    a->dirty = 1;
    return ++a->endPass >= OVER_PASSES;
}

static int end_wait(App *a, int frames)
{
    return ++a->endFrame >= frames;
}

/* Whatever the step needs before its first frame: the loads the original does
 * into the hidden page. */
static void end_enter(App *a, int step)
{
    a->endStep = step;
    a->endPass = 0;
    a->endSub = 0;
    a->endFrame = 0;
    switch (step) {
    case END_LOAD1:
        load_cg(&a->endBuf, a->dir, "END1.CG", 4);
        break;
    case END_IN2:
        load_cg(&a->endBuf, a->dir, "END2.CG", 4);
        break;
    case END_STAFF1:
        /* 2406:0213 reads 32000 bytes into a800 and 32000 into b000 and then
         * runs out of file, so STAFF1.CG is two planes on a cleared page. */
        load_cg(&a->endTop, a->dir, "STAFF1.CG", 2);
        break;
    case END_STAFF3:
        load_staff_strip(&a->endTop, a->dir, "STAFF3.CG");
        break;
    case END_STAFF4:
        load_staff_strip(&a->endTop, a->dir, "STAFF4.CG");
        break;
    default:
        break;
    }
}

static void end_tick(App *a)
{
    switch (a->endStep) {
    case END_LOAD1:
        end_enter(a, END_ERASE);
        break;
    case END_ERASE:
        if (end_scatter(a, NULL, SCAT_ERASE)) {
            gfx_palette(&a->gfx, CG_PAL_END);            /* 2406:010e */
            end_enter(a, END_IN1);
        }
        break;
    case END_IN1:
        if (end_scatter(a, &a->endBuf, SCAT_REPLACE)) end_enter(a, END_HOLD1);
        break;
    case END_HOLD1:
        if (end_wait(a, END_MS(5000))) end_enter(a, END_STAFF1);
        break;
    case END_STAFF1:
        if (end_scatter(a, &a->endTop, SCAT_OVER)) end_enter(a, END_HOLD2);
        break;
    case END_HOLD2:
        if (end_wait(a, END_MS(8192))) end_enter(a, END_BACK1);
        break;
    case END_BACK1:
        if (end_scatter(a, &a->endBuf, SCAT_REPLACE)) end_enter(a, END_IN2);
        break;
    case END_IN2:
        if (end_scatter(a, &a->endBuf, SCAT_REPLACE)) end_enter(a, END_STAFF3);
        break;
    case END_STAFF3:
        if (end_scatter(a, &a->endTop, SCAT_OVER)) end_enter(a, END_STAFF4);
        break;
    case END_STAFF4:
        if (end_wait(a, END_MS(4000))) end_enter(a, END_BACK2);
        break;
    case END_BACK2:
        if (end_scatter(a, &a->endBuf, SCAT_REPLACE))
            end_enter(a, END_STAFF4_IN);
        break;
    case END_STAFF4_IN:
        if (end_scatter(a, &a->endTop, SCAT_OVER)) end_enter(a, END_HOLD3);
        break;
    case END_HOLD3:
        if (end_wait(a, END_MS(7000))) end_enter(a, END_STRIP);
        break;
    case END_STRIP:
        /* 23b0:0019: the hidden page - black and the staff4 strip - replaces
         * the screen, so everything else scatters away. */
        if (end_scatter(a, &a->endTop, SCAT_REPLACE)) end_enter(a, END_HOLD4);
        break;
    default:
        if (end_wait(a, END_MS(4096))) end_enter(a, END_LOAD1);
        break;
    }
}

/* 1edb:053d: the grid's loop asks FUN_1edb_427c whether every cell looks
 * cleared.  It counts them by reading the screen; the records say the same
 * thing, and draw_select paints a cell from exactly this test. */
static int all_cleared(const App *a)
{
    int n;
    for (n = 0; n < a->stageCount; n++) {
        int rec = a->record[n];
        if (!(rec != 0 && rec < SEL_LOCKED)) return 0;
    }
    return a->stageCount > 0;
}

static void ending_now(App *a)
{
    a->screen = SCR_END;
    end_enter(a, END_LOAD1);
    a->dirty = 1;
}

static void go_ending(App *a)
{
    app_music(a, BGM_END);
    if (music_waiting(a)) {
        a->waitWhat = WAIT_END;
        return;
    }
    ending_now(a);
}

static void start_slide(App *a, int dir, int pushed)
{
    a->facing = app_facing(dir);
    a->animDx = dir == DIR_RIGHT ? 1 : dir == DIR_LEFT ? -1 : 0;
    a->animDy = dir == DIR_DOWN ? 1 : dir == DIR_UP ? -1 : 0;
    a->animPush = pushed;
    a->animLeft = a->px / a->step;
    a->animTick = 0;
}

void app_key(App *a, int key)
{
    /* The original is inside FUN_24d7_00a8's spin and reads nothing.  The
     * ending's loop reads nothing either - there is no way out of it. */
    if (music_waiting(a) || a->screen == SCR_END) return;
    if (a->screen == SCR_BOOT) {
        a->screen = SCR_TITLE;
        gfx_palette(&a->gfx, CG_PAL_TITLE);
        a->dirty = 1;
        return;
    }
    if (a->screen == SCR_TITLE) {
        go_select(a);
        return;
    }
    if (a->screen == SCR_SELECT) {
        if (key == KEY_ENTER && a->pick) app_play(a, a->pick);
        else if (key == KEY_LEFT && a->pick > 1) a->pick--;
        else if (key == KEY_RIGHT && a->pick < MEN_STAGES) a->pick++;
        else if (key == KEY_UP && a->pick > SEL_COLS) a->pick -= SEL_COLS;
        else if (key == KEY_DOWN && a->pick + SEL_COLS <= MEN_STAGES)
            a->pick += SEL_COLS;
        else if (key == KEY_ESC) a->screen = SCR_TITLE;
        else return;
        a->dirty = 1;
        return;
    }

    if (app_busy(a)) return;             /* the original walks, then listens */

    /* Once a stage is over, anything but a retry goes back to the grid -
     * 1edb:15af waits, restarts the stage music and jumps to 0x10cd. */
    if (a->result != RESULT_PLAYING) {
        if (key == KEY_RETRY) app_play(a, a->game.stage);
        else go_select(a);
        return;
    }

    switch (key) {
    case KEY_UP: case KEY_RIGHT: case KEY_DOWN: case KEY_LEFT: {
        int before = a->game.pushes;
        a->facing = app_facing(key);
        if (game_step(&a->game, key))
            start_slide(a, key, a->game.pushes != before);
        /* 1edb:1204's count of boxes off their goals reaching zero is the
         * clear, and that is where BGM 4 comes in; FUN_2329_05b8 stores the
         * step count, which is what turns the grid cell red. */
        if (game_won(&a->game)) {
            if (a->game.stage >= 1)
                a->record[a->game.stage - 1] = a->game.moves;
            a->result = RESULT_CLEAR;
            a->overStep = 0;
            a->overTick = 0;
            app_music(a, BGM_CLEAR);
        } else if (a->game.st && a->game.moves >= a->game.st->moves) {
            /* 1edb:14bb: `cmp steps, limit` then `jae` into the BGM 3 path,
             * which is the "仕事が遅い。やりなおしだっ!!" overlay. */
            a->result = RESULT_FAIL;
            a->overStep = 0;
            a->overTick = 0;
            app_music(a, BGM_FAIL);
        }
        break;
    }
    case KEY_UNDO: {
        int e = a->game.histLen > 0 ? a->game.hist[a->game.histLen - 1] : -1;
        if (e >= 0 && game_undo(&a->game)) {
            /* FUN_1edb_2c10 takes -1 as its first argument to run a step
             * backwards, so an undo slides the other way. */
            start_slide(a, e & 3, e & 4 ? 1 : 0);
            a->animDx = -a->animDx;
            a->animDy = -a->animDy;
        }
        break;
    }
    case KEY_RETRY:
        app_play(a, a->game.stage);
        return;
    case KEY_ESC:
        go_select(a);
        return;
    default:
        return;
    }
    a->dirty = 1;
}

void app_move(App *a, int x, int y)
{
    if (a->screen != SCR_SELECT || music_waiting(a)) return;
    {
        int c = app_cell(x, y);
        if (c && c != a->pick) { a->pick = c; a->dirty = 1; }
    }
}

void app_click(App *a, int x, int y)
{
    /* Nothing is listening while FUN_24d7_00a8 waits out a fade, or ever
     * again once the ending has started. */
    if (music_waiting(a) || a->screen == SCR_END) return;
    if (a->screen == SCR_BOOT || a->screen == SCR_TITLE) {
        app_key(a, KEY_ENTER);
        return;
    }
    if (a->screen == SCR_SELECT) {
        int c = app_cell(x, y);
        if (c) app_play(a, c);
        return;
    }
    if (app_busy(a)) return;
    /* A click walks one square towards the pointer.  The original is
     * mouse-driven and its own routine for this is in the part of the code
     * Ghidra could not reach, so this is the port's reading of it. */
    {
        int cx = (x - a->ox) / a->px;
        int cy = (y - a->oy) / a->px;
        int dx = cx - a->game.x, dy = cy - a->game.y;
        if (abs(dx) >= abs(dy)) {
            if (dx > 0) app_key(a, KEY_RIGHT);
            else if (dx < 0) app_key(a, KEY_LEFT);
        } else {
            if (dy > 0) app_key(a, KEY_DOWN);
            else if (dy < 0) app_key(a, KEY_UP);
        }
    }
}

void app_tick(App *a)
{
    a->frame++;
    if (a->screen == SCR_BOOT) {
        a->bootTick++;
        switch (a->bootPhase) {
        case BOOT_RISE_A:
            logo_scroll(a, LOGO_A_ROW, LOGO_A_ROWS);
            if (++a->bootStep >= LOGO_A_STEPS) {
                a->bootPhase = BOOT_WAIT;
                a->bootStep = 0;
            }
            break;
        case BOOT_WAIT:
            if (++a->bootStep >= BOOT_HOLD_TICKS) {
                a->bootPhase = BOOT_FADE;
                a->bootStep = 0;
            }
            break;
        case BOOT_FADE: {
            int i = a->bootStep / BOOT_FADE_TICKS;
            if (i > 15) i = 15;
            boot_palette(a, i, i / 4 + 12, i);
            if (++a->bootStep >= 16 * BOOT_FADE_TICKS) {
                a->bootPhase = BOOT_RISE_A2;
                a->bootStep = 0;
            }
            break;
        }
        case BOOT_RISE_A2:
            logo_scroll(a, LOGO_A_ROW, LOGO_A_ROWS);
            if (++a->bootStep >= LOGO_A_STEPS) {
                boot_palette(a, 0, 0, 0);
                logo_bring_second(a);
                a->bootPhase = BOOT_RISE_B;
                a->bootStep = 0;
            }
            break;
        case BOOT_RISE_B:
            logo_scroll(a, LOGO_B_ROW, LOGO_B_ROWS);
            if (++a->bootStep >= LOGO_B_STEPS) {
                a->bootPhase = BOOT_DONE;
                a->bootStep = 0;
            }
            break;
        default:
            if (++a->bootStep >= BOOT_HOLD_TICKS) {
                a->screen = SCR_TITLE;
                gfx_palette(&a->gfx, CG_PAL_TITLE);
            }
            break;
        }
        a->dirty = 1;
        return;
    }
    if (a->animLeft > 0) {
        /* 1edb:2c9f: a counter runs 0, 1, then resets to -1 and steps the
         * walk frame, so the frame changes every third sub-step. */
        if (++a->animTick >= PHASE_EVERY) {
            a->animTick = 0;
            a->phase = (a->phase + 1) % 3;
        }
        a->animLeft--;
        a->dirty = 1;
        if (a->animLeft == 0) {
            a->lastSprite = gfx_man(a->facing, a->animPush, a->phase);
            a->animDx = 0;
            a->animDy = 0;
        }
    }

    /* FUN_24d7_00a8's `while (AH=8) ;`.  Nothing below here happens until the
     * driver has finished fading - and nothing above it did in the original
     * either, since the move had already finished animating by the time the
     * clear test asked for BGM 4. */
    if (music_waiting(a)) {
        if (!a->mmd.playing || ++a->songWait >= BGM_WAIT_TICKS) {
            int n = a->songNext, what = a->waitWhat, arg = a->waitArg;
            a->songNext = -1;
            a->waitWhat = WAIT_NOTHING;
            song_start(a, n);
            if (what == WAIT_PLAY) play_now(a, arg);
            else if (what == WAIT_SELECT) select_now(a);
            else if (what == WAIT_END) ending_now(a);
        }
        return;
    }

    if (a->screen == SCR_END) {
        end_tick(a);
        return;
    }

    /* 1edb:053d: every turn of the grid's loop, all thirty cleared means the
     * ending - and the ending never comes back. */
    if (a->screen == SCR_SELECT && all_cleared(a)) {
        go_ending(a);
        return;
    }

    /* 1edb:11d2: the picture only starts to dissolve in once BGM 4 is up. */
    if (a->screen == SCR_PLAY && a->result != RESULT_PLAYING) {
        if (a->overStep < OVER_PASSES &&
            ++a->overTick >= OVER_FRAMES) {
            a->overTick = 0;
            a->overStep++;
            a->dirty = 1;
        }
    }
}

/* --- lettering ------------------------------------------------------------ */

static void put_glyph(App *a, int style, int glyph, int x, int y, int ink)
{
    int gy, gx;
    for (gy = 0; gy < FONT_H; gy++)
        for (gx = 0; gx < FONT_W; gx++) {
            int tx = x + gx, ty = y + gy;
            if (tx < 0 || ty < 0 || tx >= GFX_W || ty >= GFX_H) continue;
            if (!font_ink(&a->font, style, glyph, gx, gy)) continue;
            a->gfx.px[ty][tx] = (unsigned char)ink;
        }
}

/* FUN_2329_0506: `digits` glyphs, most significant first, 9 pixels apart. */
static void put_number(App *a, int x, int y, int digits, int value, int style,
                       int ink)
{
    int i, k, p = 1;
    for (k = 1; k < digits; k++) p *= 10;
    for (i = 0; i < digits; i++) {
        int d = p ? value / p : 0;
        put_glyph(a, style, FONT_DIGIT0 + (d % 10), x + i * FONT_PITCH, y, ink);
        value -= d * p;
        p /= 10;
    }
}


static void fill(App *a, int x0, int y0, int x1, int y1, int colour)
{
    int y, x;
    for (y = y0; y <= y1; y++) {
        if (y < 0 || y >= GFX_H) continue;
        for (x = x0; x <= x1; x++) {
            if (x < 0 || x >= GFX_W) continue;
            a->gfx.px[y][x] = (unsigned char)colour;
        }
    }
}

/* --- the screens ---------------------------------------------------------- */

static void draw_boot(App *a)
{
    int y, x;

    gfx_clear(&a->gfx, BOOT_GROUND);
    /* Whatever the scrolling has left in the plane.  Only the band the
     * original scrolls is ever visible - x = 216 for 208 pixels - so the
     * staging half at x = 432 is masked off. */
    for (y = 0; y < GFX_H; y++)
        for (x = LOGO_BAND_BYTE * 8; x < (LOGO_BAND_BYTE + LOGO_BAND_WORDS * 2) * 8;
             x++) {
            int b = a->logoPlane[y * LOGO_STRIDE + x / 8];
            if ((b >> (7 - (x & 7))) & 1) a->gfx.px[y][x] = BOOT_INK;
        }
}

/* FUN_2329_000d, cell for cell. */
static void draw_select(App *a)
{
    int n;

    gfx_blit(&a->gfx, &a->select, 0, 0, GFX_W, GFX_H, 0, 0);
    fill(a, 0x20, 0x28, 0x25f, 0x17b, SEL_PANEL);

    for (n = 1; n <= a->stageCount; n++) {
        int col = (n - 1) % SEL_COLS, row = (n - 1) / SEL_COLS;
        int x0 = col * SEL_CW + SEL_X0, y0 = row * SEL_CH + SEL_Y0;
        int x1 = col * SEL_CW + 0x7f, y1 = row * SEL_CH + 0x6b;
        int rec = a->record[n - 1];
        int done = rec != 0 && rec < SEL_LOCKED;

        fill(a, x0, y0, x1, y1, SEL_EDGE);
        fill(a, x0 + 1, y0 + 1, x1 - 1, y1 - 1, done ? SEL_DONE : SEL_PANEL);
        put_number(a, x0 + SEL_NUM_DX, y0 + SEL_NUM_DY, 2, n,
                   done ? 1 : 0, done ? SEL_PANEL : SEL_EDGE);
        if (n == a->pick) {
            fill(a, x0, y0, x1, y0 + 1, SEL_DONE);
            fill(a, x0, y1 - 1, x1, y1, SEL_DONE);
            fill(a, x0, y0, x0 + 1, y1, SEL_DONE);
            fill(a, x1 - 1, y0, x1, y1, SEL_DONE);
        }
    }
}

/* FUN_1edb_3907: a white 100x80 panel, the SCORE window art from
 * WINDOWS.CGM on top of it, and the three numbers punched in. */
static void draw_score(App *a, int x, int y)
{
    int dy, dx;

    fill(a, x, y, x + SCORE_W - 1, y + SCORE_H - 1, SEL_PANEL);
    for (dy = 0; dy < SCORE_H; dy++)
        for (dx = 0; dx < SCORE_W; dx++) {
            int tx = x + dx, ty = y + dy;
            if (tx < 0 || ty < 0 || tx >= GFX_W || ty >= GFX_H) continue;
            /* WINDOWS.CGM is one plane drawn ink-on-white, so a clear bit is
             * the ink. */
            if (cg_pixel(&a->windows, SCORE_SRC_X + dx, SCORE_SRC_Y + dy))
                continue;
            a->gfx.px[ty][tx] = SEL_EDGE;
        }
    put_number(a, x + SCORE_STAGE_X, y + SCORE_STAGE_Y, 3, a->game.stage,
               0, SEL_EDGE);
    put_number(a, x + SCORE_STEPS_X, y + SCORE_STEPS_Y, 5, a->game.moves,
               0, SEL_EDGE);
    put_number(a, x + SCORE_LIMIT_X, y + SCORE_LIMIT_Y, 5,
               a->game.st ? a->game.st->moves : 0, 0, SEL_EDGE);
}

static void draw_play(App *a)
{
    const Stage *s = a->game.st;
    int x, y;
    int slide = a->animLeft * a->step;   /* how far back to pull the slide */
    int mx, my, sprite;

    gfx_clear(&a->gfx, PLAY_GROUND);
    for (y = 0; y < s->h; y++)
        for (x = 0; x < s->w; x++) {
            int dx = a->ox + x * a->px, dy = a->oy + y * a->px;
            int t = -1;
            int sliding = 0;
            gfx_tile(&a->gfx, &a->chr, a->px, T_FLOOR, dx, dy, -1);
            if (men_wall(s, x, y)) t = T_WALL;
            else if (game_box(&a->game, x, y)) {
                t = men_goal(s, x, y) ? T_BOX_ON_GOAL : T_BOX + a->boxKind;
                /* The box being pushed is drawn after the board, sliding. */
                if (a->animLeft > 0 && a->animPush &&
                    x == a->game.x + a->animDx && y == a->game.y + a->animDy)
                    sliding = 1;
            } else if (men_goal(s, x, y)) t = T_GOAL;
            if (t < 0 || sliding) continue;
            gfx_tile(&a->gfx, &a->chr, a->px, t, dx, dy, -1);
        }

    if (a->animLeft > 0 && a->animPush) {
        int bx = a->game.x + a->animDx, by = a->game.y + a->animDy;
        int t = men_goal(s, bx, by) ? T_BOX_ON_GOAL : T_BOX + a->boxKind;
        gfx_tile(&a->gfx, &a->chr, a->px, t,
                 a->ox + bx * a->px - a->animDx * slide,
                 a->oy + by * a->px - a->animDy * slide, -1);
    }

    mx = a->ox + a->game.x * a->px - a->animDx * slide;
    my = a->oy + a->game.y * a->px - a->animDy * slide;
    sprite = a->animLeft > 0 ? gfx_man(a->facing, a->animPush, a->phase)
                             : a->lastSprite;
    gfx_tile(&a->gfx, &a->chr, a->px, sprite, mx, my, 1);

    /* FUN_1edb_3a43 keeps the score window out of the man's way by hopping it
     * between (10,10), (0x212,10), (0x212,0x136) and (10,0x136); this puts it
     * at whichever of those corners the man is furthest from. */
    draw_score(a, mx < GFX_W / 2 ? 0x212 : 10, my < GFX_H / 2 ? 0x136 : 10);

    /* The clear and failure pictures, dissolved in the way FUN_23b0_0440 does
     * it - see the note in app.h.  Anything that is not colour 0 goes on. */
    if (a->result != RESULT_PLAYING) {
        const Cg *pic = a->result == RESULT_CLEAR ? &a->clear : &a->peke;
        int pass;

        for (pass = 0; pass <= a->overStep && pass < OVER_PASSES; pass++) {
            long o;
            for (o = (long)pass * 2; o < CG_PLANE - 1; o += OVER_STRIDE) {
                int row = (int)(o / (GFX_W / 8)), col = (int)(o % (GFX_W / 8));
                int k;
                if (row >= GFX_H) break;
                /* One word: the byte at the offset and the one after it. */
                for (k = 0; k < 16; k++) {
                    int x = col * 8 + k, v;
                    if (x >= GFX_W) break;
                    v = cg_pixel(pic, x, row);
                    if (v) a->gfx.px[row][x] = (unsigned char)v;
                }
            }
        }
    }
}

void app_render(App *a)
{
    switch (a->screen) {
    case SCR_BOOT:
        draw_boot(a);
        break;
    case SCR_TITLE:
        gfx_blit(&a->gfx, &a->title, 0, 0, GFX_W, GFX_H, 0, 0);
        break;
    case SCR_SELECT:
        draw_select(a);
        break;
    case SCR_END:
        /* Nothing: the ending scatters straight into the framebuffer, the way
         * it scatters into VRAM, and what is on the screen stays there. */
        break;
    default:
        draw_play(a);
        break;
    }
    a->dirty = 0;
}
