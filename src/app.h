/* The screens, and everything that does not care which host it runs on.
 *
 * The flow is the original's, read out of FUN_1edb_000e:
 *
 *     display probe -> loading band + LOGO.CG -> file check -> fade
 *     -> TITLE.CG            (palette DS:0x02a0)
 *     -> FUN_1edb_03c9       SELECT.CG, the 6x5 grid of thirty stages
 *     -> FUN_1edb_109b       the board, background colour 3 of DS:0x02d0
 */
#ifndef SOKO_APP_H
#define SOKO_APP_H

#include "cg.h"
#include "game.h"
#include "gfx.h"
#include "men.h"

/* The select screen's grid, from FUN_1edb_042c:
 *
 *     if (0x27 < y && y < 0x17c && 0x1f < x && x < 0x260)
 *         stage = ((y - 0x28) / 0x44) * 6 + (x - 0x20) / 0x60 + 1;
 *
 * which is six columns of 96 from x = 32 and five rows of 68 from y = 40 -
 * exactly the white panel in SELECT.CG, and exactly thirty cells. */
#define SEL_X0 32
#define SEL_Y0 40
#define SEL_CW 96
#define SEL_CH 68
#define SEL_COLS 6
#define SEL_ROWS 5

enum { SCR_TITLE, SCR_SELECT, SCR_PLAY };

/* What a host sends in.  The original is mouse-driven - MOUSE.SYS is on the
 * disk and FUN_1edb_042c hit-tests pixel coordinates - but the board also
 * wants the keyboard, so both go in. */
enum {
    KEY_UP, KEY_RIGHT, KEY_DOWN, KEY_LEFT,
    KEY_UNDO, KEY_RETRY, KEY_ESC, KEY_ENTER,
    KEY_COUNT
};

typedef struct {
    Gfx gfx;
    Cg title, select, chr, windows;
    Stage stages[MEN_STAGES];
    int stageCount;

    Game game;
    int screen;
    int pick;                            /* 1..30, the cell under the pointer */
    int dirty;
    int frame;

    int px;                              /* the board's tile size and origin, */
    int ox, oy;                          /* recomputed when a stage starts */
} App;

/* Reads the game's own files with fopen, which is why the wasm build embeds
 * them at the same paths.  Returns 0 on success. */
int app_init(App *a, const char *dir);

void app_key(App *a, int key);
void app_click(App *a, int x, int y);
void app_move(App *a, int x, int y);     /* the pointer, for the highlight */
void app_tick(App *a);
void app_render(App *a);

/* Which stage cell a point falls in, or 0. */
int app_cell(int x, int y);

void app_play(App *a, int stage);

#endif
