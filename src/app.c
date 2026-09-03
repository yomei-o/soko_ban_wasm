/* The screens.
 *
 * Everything with an address in a comment came out of out/sbp98.c or
 * out/sbp98.asm; everything else is this port's own choice and is called out
 * as such, because the original's drawing for those bits has not been
 * recovered yet.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"

/* WINDOWS.CGM carries the digits: y = 300, nine rows tall, nine pixels apart
 * from x = 0, so digit d starts at (d * 9, 300).  It is a one-plane sheet
 * drawn ink-on-white, so a CLEAR bit is the ink. */
#define DIGIT_Y 300
#define DIGIT_W 9
#define DIGIT_H 9

/* The play screen's ground.  FUN_1edb_109b does setfillstyle(SOLID, 3) and
 * then bar(0, 0, 0x27f, 399) before anything else, so the board sits on
 * colour 3 of the tile palette - a cream, not black. */
#define PLAY_GROUND 3

/* The select screen runs on the title palette (FUN_1edb_03c9 calls
 * FUN_2406_000c, which is the DS:0x02a0 table), so the thumbnails have to be
 * drawn in colours that exist there.  The original's thumbnail drawing has
 * not been found yet; these are this port's choice out of that palette:
 * 0x0f white ground, 0x04 dark brown wall, 0x0c red goal, 0x0a yellow box. */
#define SEL_GROUND 15
#define SEL_WALL 4
#define SEL_GOAL 12
#define SEL_BOX 10
#define SEL_INK 0

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

    memset(a, 0, sizeof *a);
    if (load_cg(&a->title, dir, "TITLE.CG", 4)) return -1;
    if (load_cg(&a->select, dir, "SELECT.CG", 4)) return -2;
    if (load_cg(&a->chr, dir, "CHR98N.CG", 4)) return -3;
    if (load_cg(&a->windows, dir, "WINDOWS.CGM", 1)) return -4;

    d = slurp(dir, "SBPMEN.DAT", &len);
    if (!d) return -5;
    a->stageCount = men_load(a->stages, d, len);
    free(d);
    if (a->stageCount < MEN_STAGES) return -6;

    a->screen = SCR_TITLE;
    a->pick = 0;
    a->dirty = 1;
    gfx_palette(&a->gfx, CG_PAL_TITLE);
    return 0;
}

int app_cell(int x, int y)
{
    int col, row;

    /* The original's own bounds, strict on both sides: 0x1f < x < 0x260 and
     * 0x27 < y < 0x17c. */
    if (x <= 0x1f || x >= 0x260 || y <= 0x27 || y >= 0x17c) return 0;
    col = (x - SEL_X0) / SEL_CW;
    row = (y - SEL_Y0) / SEL_CH;
    if (col < 0 || col >= SEL_COLS || row < 0 || row >= SEL_ROWS) return 0;
    return row * SEL_COLS + col + 1;
}

void app_play(App *a, int stage)
{
    const Stage *s;

    if (stage < 1 || stage > a->stageCount) return;
    s = &a->stages[stage - 1];
    game_start(&a->game, s, stage);
    /* FUN_1edb_31c5 picks the size and shifts the board into the middle of
     * that grid in whole cells, so the origin is a multiple of the tile.
     * Centring in pixels instead is off by half a tile on the odd boards. */
    a->px = s->tilePx;
    a->ox = s->shiftX * a->px;
    a->oy = s->shiftY * a->px;
    /* FUN_1edb_2bb9 turns a box into 0x1d + [0x3ed7] % 15, and [0x3ed7] comes
     * from the clock once per stage, so every box in a stage is the same
     * product.  Keying it to the stage instead keeps a screenshot the same
     * from run to run. */
    a->boxKind = (stage - 1) % T_BOX_KINDS;
    a->screen = SCR_PLAY;
    gfx_palette(&a->gfx, CG_PAL_TILES);
    a->dirty = 1;
}

void app_key(App *a, int key)
{
    if (a->screen == SCR_TITLE) {
        a->screen = SCR_SELECT;
        gfx_palette(&a->gfx, CG_PAL_TITLE);
        a->dirty = 1;
        return;
    }
    if (a->screen == SCR_SELECT) {
        if (key == KEY_ENTER && a->pick) app_play(a, a->pick);
        else if (key == KEY_LEFT && a->pick > 1) a->pick--;
        else if (key == KEY_RIGHT && a->pick < MEN_STAGES) a->pick++;
        else if (key == KEY_UP && a->pick > SEL_COLS) a->pick -= SEL_COLS;
        else if (key == KEY_DOWN && a->pick + SEL_COLS <= MEN_STAGES)
            a->pick += SEL_COLS;
        else if (key == KEY_ESC) { a->screen = SCR_TITLE; }
        else return;
        a->dirty = 1;
        return;
    }

    /* the board */
    switch (key) {
    case KEY_UP: case KEY_RIGHT: case KEY_DOWN: case KEY_LEFT:
        game_step(&a->game, key);
        break;
    case KEY_UNDO:
        game_undo(&a->game);
        break;
    case KEY_RETRY:
        game_start(&a->game, a->game.st, a->game.stage);
        break;
    case KEY_ESC:
        a->screen = SCR_SELECT;
        gfx_palette(&a->gfx, CG_PAL_TITLE);
        break;
    default:
        return;
    }
    a->dirty = 1;
}

void app_move(App *a, int x, int y)
{
    if (a->screen != SCR_SELECT) return;
    {
        int c = app_cell(x, y);
        if (c && c != a->pick) { a->pick = c; a->dirty = 1; }
    }
}

void app_click(App *a, int x, int y)
{
    if (a->screen == SCR_TITLE) {
        a->screen = SCR_SELECT;
        a->dirty = 1;
        return;
    }
    if (a->screen == SCR_SELECT) {
        int c = app_cell(x, y);
        if (c) app_play(a, c);
        return;
    }
    /* On the board a click walks one square towards the pointer, which is how
     * a mouse-driven Sokoban has to work; the original's own routine for this
     * is in the part of the code Ghidra could not reach. */
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
}

/* One digit out of WINDOWS.CGM, ink where the source bit is clear. */
static void draw_digit(App *a, int d, int x, int y, int ink)
{
    int dy, dx;
    if (d < 0 || d > 9) return;
    for (dy = 0; dy < DIGIT_H; dy++)
        for (dx = 0; dx < DIGIT_W; dx++) {
            int tx = x + dx, ty = y + dy;
            if (tx < 0 || ty < 0 || tx >= GFX_W || ty >= GFX_H) continue;
            if (cg_pixel(&a->windows, d * DIGIT_W + dx, DIGIT_Y + dy)) continue;
            a->gfx.px[ty][tx] = (unsigned char)ink;
        }
}

static void draw_number(App *a, int n, int x, int y, int ink)
{
    char buf[12];
    int i;
    snprintf(buf, sizeof buf, "%d", n);
    for (i = 0; buf[i]; i++)
        draw_digit(a, buf[i] - '0', x + i * DIGIT_W, y, ink);
}

static void fill(App *a, int x, int y, int w, int h, int colour)
{
    int dy, dx;
    for (dy = 0; dy < h; dy++) {
        int ty = y + dy;
        if (ty < 0 || ty >= GFX_H) continue;
        for (dx = 0; dx < w; dx++) {
            int tx = x + dx;
            if (tx < 0 || tx >= GFX_W) continue;
            a->gfx.px[ty][tx] = (unsigned char)colour;
        }
    }
}

static void draw_thumb(App *a, int stage, int cx, int cy)
{
    const Stage *s = &a->stages[stage - 1];
    int pad = 3, top = DIGIT_H + 1;
    int availW = SEL_CW - pad * 2, availH = SEL_CH - top - pad;
    int cell = availW / s->w;
    int x, y, ox, oy;

    if (availH / s->h < cell) cell = availH / s->h;
    if (cell < 1) cell = 1;
    ox = cx + (SEL_CW - s->w * cell) / 2;
    oy = cy + top + (availH - s->h * cell) / 2;

    for (y = 0; y < s->h; y++)
        for (x = 0; x < s->w; x++) {
            int c = -1;
            if (men_wall(s, x, y)) c = SEL_WALL;
            else if (men_box(s, x, y)) c = SEL_BOX;
            else if (men_goal(s, x, y)) c = SEL_GOAL;
            if (c >= 0) fill(a, ox + x * cell, oy + y * cell, cell, cell, c);
        }
    draw_number(a, stage, cx + pad, cy + 1, SEL_INK);
}

static void draw_select(App *a)
{
    int n;

    gfx_blit(&a->gfx, &a->select, 0, 0, GFX_W, GFX_H, 0, 0);
    /* The picture's white panel is the grid; the thumbnails go on top of it. */
    for (n = 1; n <= a->stageCount; n++) {
        int col = (n - 1) % SEL_COLS, row = (n - 1) / SEL_COLS;
        int cx = SEL_X0 + col * SEL_CW, cy = SEL_Y0 + row * SEL_CH;
        if (n == a->pick) fill(a, cx, cy, SEL_CW, SEL_CH, SEL_GOAL);
        else fill(a, cx, cy, SEL_CW, SEL_CH, SEL_GROUND);
        draw_thumb(a, n, cx, cy);
    }
}

static void draw_play(App *a)
{
    const Stage *s = a->game.st;
    int x, y;

    gfx_clear(&a->gfx, PLAY_GROUND);
    for (y = 0; y < s->h; y++)
        for (x = 0; x < s->w; x++) {
            int dx = a->ox + x * a->px, dy = a->oy + y * a->px;
            int t = -1;
            gfx_tile(&a->gfx, &a->chr, a->px, T_FLOOR, dx, dy, -1);
            if (men_wall(s, x, y)) t = T_WALL;
            else if (game_box(&a->game, x, y))
                t = men_goal(s, x, y) ? T_BOX_ON_GOAL : T_BOX + a->boxKind;
            else if (men_goal(s, x, y)) t = T_GOAL;
            if (t >= 0) gfx_tile(&a->gfx, &a->chr, a->px, t, dx, dy, -1);
        }
    /* The last entry in the history says whether that step was a push, which
     * is what picks the pushing sprite; the step count drives the phase so
     * walking cycles as the man goes. */
    {
        int pushing = a->game.histLen > 0 &&
                      (a->game.hist[a->game.histLen - 1] & 4);
        int t = gfx_man(a->game.facing, pushing, a->game.moves);
        gfx_tile(&a->gfx, &a->chr, a->px, t,
                 a->ox + a->game.x * a->px, a->oy + a->game.y * a->px, 1);
    }

    /* STEPS and the stage number, in the corners the SCORE window uses. */
    draw_number(a, a->game.stage, 8, 8, 0);
    draw_number(a, a->game.moves, GFX_W - 8 - DIGIT_W * 5, 8, 0);
}

void app_render(App *a)
{
    switch (a->screen) {
    case SCR_TITLE:
        gfx_blit(&a->gfx, &a->title, 0, 0, GFX_W, GFX_H, 0, 0);
        break;
    case SCR_SELECT:
        draw_select(a);
        break;
    default:
        draw_play(a);
        break;
    }
    a->dirty = 0;
}
