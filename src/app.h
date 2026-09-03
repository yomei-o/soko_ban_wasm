/* The screens, and everything that does not care which host it runs on.
 *
 * The flow is the original's, read out of FUN_1edb_000e:
 *
 *     display probe -> a white screen with LOGO.CG at y = 245, the Kao
 *        crescent and "soft office THINKING RABBIT PRESENTS"
 *     -> TITLE.CG            (palette DS:0x02a0)
 *     -> FUN_1edb_03c9       SELECT.CG plus FUN_2329_000d's grid of thirty
 *                            numbered cells
 *     -> FUN_1edb_109b       the board, ground colour 3 of DS:0x02d0
 *
 * FUN_1edb_07e9, which follows the grid in main, is only the shutdown, so the
 * grid's own loop is where the game lives.  A cell is enterable when its
 * record is under 15001 (FUN_1edb_042c), and records start at zero, so all
 * thirty are open from the start; what the cell's colour says is whether it
 * has been cleared.
 */
#ifndef SOKO_APP_H
#define SOKO_APP_H

#include "cg.h"
#include "font.h"
#include "game.h"
#include "gfx.h"
#include "men.h"

/* FUN_2329_000d's geometry, and FUN_1edb_042c's hit test on top of it:
 *
 *     bar(0x20, 0x28, 0x25f, 0x17b)                      the white panel
 *     outer bar(col*0x60+0x20, row*0x44+0x28,            one cell
 *               col*0x60+0x7f, row*0x44+0x6b)
 *     inner bar(+1, +1, -1, -1)
 *     FUN_2329_0506(col*0x60+0x24, row*0x44+0x2f, 2, n, style)
 *
 *     if (0x27 < y && y < 0x17c && 0x1f < x && x < 0x260)
 *         stage = ((y - 0x28) / 0x44) * 6 + (x - 0x20) / 0x60 + 1;
 */
#define SEL_X0 0x20
#define SEL_Y0 0x28
#define SEL_CW 0x60
#define SEL_CH 0x44
#define SEL_COLS 6
#define SEL_ROWS 5
#define SEL_NUM_DX 0x24
#define SEL_NUM_DY 0x2f
#define SEL_LOCKED 15000                 /* a record at or over this is shut */

/* LOGO.CG is 8000 bytes = 80 x 100, and FUN_2406_01b7 drops it at
 * 0xb000:0x4c90 - offset 19600, which is 19600 / 80 = row 245. */
#define LOGO_Y 245
#define LOGO_ROWS 100

enum { SCR_BOOT, SCR_TITLE, SCR_SELECT, SCR_PLAY };

enum {
    KEY_UP, KEY_RIGHT, KEY_DOWN, KEY_LEFT,
    KEY_UNDO, KEY_RETRY, KEY_ESC, KEY_ENTER,
    KEY_COUNT
};

typedef struct {
    Gfx gfx;
    Cg title, select, chr, windows, logo;
    Font font;
    Stage stages[MEN_STAGES];
    int stageCount;

    Game game;
    int screen;
    int pick;                            /* 1..30, the cell under the pointer */
    int dirty;
    int frame;
    int bootTick;
    unsigned char bootPal[16][3];         /* the loading screen's own palette */

    int record[MEN_STAGES];              /* 0 = never cleared */

    int px;                              /* the board's tile size and origin */
    int ox, oy;
    int boxKind;                         /* which of the fifteen packages */

    /* FUN_1edb_2c10's slide.  A move is committed to the model at once and
     * then drawn pulled back by what is left of it, which puts the same
     * pixels on the screen as the original's forward walk. */
    int step;                            /* cc_step, pixels a sub-step */
    int animLeft;                        /* sub-steps still to run */
    int animDx, animDy;                  /* the direction it is running in */
    int animPush;                        /* a box is coming along */
    int animTick;                        /* counts to three, then the phase */
    int phase;                           /* 0..2 */
    int facing;                          /* 0 left, 1 right, 2 down, 3 up */
    int lastSprite;                      /* [0x1537]: what to leave standing */
} App;

int app_init(App *a, const char *dir);

void app_key(App *a, int key);
void app_click(App *a, int x, int y);
void app_move(App *a, int x, int y);
void app_tick(App *a);
void app_render(App *a);

int app_cell(int x, int y);
void app_play(App *a, int stage);
int app_busy(const App *a);               /* mid-slide, so input waits */
void app_settle(App *a);                  /* run the slide out */

/* DIR_* to the original's facing, and back. */
int app_facing(int dir);

#endif
