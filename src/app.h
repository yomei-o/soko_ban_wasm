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
#include "mmd2.h"

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
 * 0xb000:0x4c90 - offset 19600, which is 19600 / 80 = row 245.  The strip
 * holds BOTH logos side by side, but they are never on screen together: the
 * right half is staged there and moved into the left half's slot for the
 * second one.
 *
 * FUN_23b0_03a9 scrolls a band 0x1b bytes in from the left, 0xd words wide,
 * from row 0x2d50 / 0x50 = 145, 0xc8 = 200 rows tall, 0x64 = 100 times.  So
 * the Kao crescent rises from row 245 to row 145.
 *
 * FUN_23b0_03ef first does `rep movsw` of 0xfa0 words = 8000 bytes from
 * 0x4cc6 to 0x4cab - byte 54 of row 245 to byte 27 of row 245, which drags
 * "soft office THINKING RABBIT PRESENTS" from x = 432 into the same slot at
 * x = 216 - and then scrolls the same band from row 0x30c0 / 0x50 = 156,
 * 0xb4 = 180 rows, 0x5a = 90 times.
 */
#define LOGO_Y 245
#define LOGO_ROWS 100
#define LOGO_STRIDE 80
#define LOGO_BAND_BYTE 0x1b             /* x = 216 */
#define LOGO_BAND_WORDS 0xd             /* 26 bytes, 208 pixels */
#define LOGO_A_ROW 145
#define LOGO_A_ROWS 200
#define LOGO_A_STEPS 100
#define LOGO_B_FROM 54                  /* byte 54 of row 245, x = 432 */
#define LOGO_B_ROW 156
#define LOGO_B_ROWS 180
#define LOGO_B_STEPS 90
#define LOGO_PLANE (LOGO_STRIDE * GFX_H)

/* FUN_1edb_3907's score window: 100 x 80, white, with the SCORE panel out of
 * WINDOWS.CGM on top and three numbers punched into it.
 *
 *     getimage(x, y, x+99, y+0x4f)                     save the background
 *     setfillstyle(SOLID, 0x0f); bar(x, y, x+99, y+0x4f)
 *     putimage(x, y, the window art)
 *     FUN_2329_0506(x+0x3d, y+0x19, 3, stage)
 *     FUN_2329_0506(x+0x06, y+0x2b, 5, steps)
 *     FUN_2329_0506(x+0x32, y+0x3d, 5, limit)
 *
 * The art is at (504, 0) in WINDOWS.CGM - the ink there is exactly 100 columns
 * wide and 80 rows tall, which is the window.  FUN_1edb_3a43 hops it between
 * the four corners as the man gets close. */
#define SCORE_W 100
#define SCORE_H 80
#define SCORE_SRC_X 504
#define SCORE_SRC_Y 0
#define SCORE_STAGE_X 0x3d
#define SCORE_STAGE_Y 0x19
#define SCORE_STEPS_X 0x06
#define SCORE_STEPS_Y 0x2b
#define SCORE_LIMIT_X 0x32
#define SCORE_LIMIT_Y 0x3d

enum { SCR_BOOT, SCR_TITLE, SCR_SELECT, SCR_PLAY };

/* Which song goes where.  FUN_24d7_001d jumps through a table at 24d7:0034
 * whose case n pushes the DS offset of "sbpbgm<n>.bgm", so n is the file
 * number; and scanning the whole image for `callf 14d7:001d` finds SIX call
 * sites, two more than Ghidra reached:
 *
 *   1edb:03a7   0   just before FUN_1edb_03c9, the stage grid
 *   1edb:10ae   2   entering a stage
 *   1edb:121b   4   see below
 *   1edb:14e5   3   1edb:14bb has just done `cmp [0x152b], [0x11db]` - the
 *                   steps against the limit - and taken the `jae` branch
 *   1edb:15af   2   after the wait, jumping back to 0x10cd to try again
 *   1edb:40c0   1   the ending, alongside END1.CG
 *
 * The one that matters is 0x121b.  It sits behind
 *
 *     call FUN_1edb_3182 ; or ax, ax ; je 0x1210 (which plays 4)
 *
 * and FUN_1edb_3182 counts the board cells equal to 2 - the boxes that are
 * NOT on a goal.  So it is zero only when the stage is solved: **4 is the
 * clear music, not the stage music.**  While you play, what keeps going is
 * the 2 started on the way in.
 *
 * The first pass had 4 as the in-stage music, so a stage played its own
 * fanfare the whole way through.
 *
 * 5 is never called from anywhere: every one of the six filenames is
 * referenced exactly once, all inside that jump table.
 */
#define BGM_SELECT 0
#define BGM_PLAY 2
#define BGM_CLEAR 4
#define BGM_FAIL 3
#define BGM_END 1
#define BGM_COUNT 6

/* FUN_23b0_0440 does not blit the clear picture on: it dissolves it in.
 *
 *     ax = 0
 *   outer:
 *     bx = ax
 *     inner:
 *       out 0xa6, 1                    read the page the picture is on
 *       cx = the OR of the four planes' words at [bx]
 *       out 0xa6, 0                    back to the visible page
 *       not cx                         where the picture is blank
 *       each plane: [bx] &= cx ; [bx] |= its word
 *       bx += 0x2a                     42 bytes on
 *     while bx < 0x8000
 *     ax += 2
 *   while ax != 0x2a                   21 passes, starting 0, 2, 4 ... 40
 *
 * So it lands one word in every 42 bytes, twenty-one passes covering every
 * word in the end.  42 bytes is a bit over half a row, so each pass paints a
 * sparse diagonal scatter and the picture fills in.  Colour 0 is what "blank"
 * means - the OR of the four planes - which is why CLEAR.CG's cloud is drawn
 * in colour 1 rather than 0, even though the tile palette has both black.
 *
 * (CLEARM.CG and PEKEM.CG are not masks.  They are the monochrome versions of
 * the same pictures, for the 8-colour path at FUN_23b0_04bc.) */
#define OVER_PASSES 21
#define OVER_STRIDE 42
#define OVER_FRAMES 2                    /* frames a pass; the original's speed
                                          * is whatever the VRAM allows */

/* What a stage ended as.  FUN_1edb_3182 == 0 is a clear; the steps reaching
 * the limit at 1edb:14bb is a failure. */
enum { RESULT_PLAYING, RESULT_CLEAR, RESULT_FAIL };

enum {
    KEY_UP, KEY_RIGHT, KEY_DOWN, KEY_LEFT,
    KEY_UNDO, KEY_RETRY, KEY_ESC, KEY_ENTER,
    KEY_COUNT
};

typedef struct {
    Gfx gfx;
    Cg title, select, chr, windows, logo;
    Cg clear, clearMask, peke, pekeMask;
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
    unsigned char logoPlane[LOGO_PLANE];  /* the one plane the logo lives in */

    Mmd2 mmd;
    unsigned char voi[1024];
    long voiLen;
    unsigned char bgm[BGM_COUNT][4096];
    long bgmLen[BGM_COUNT];
    int song;                             /* which one is playing, or -1 */
    int music;                            /* [0x128d]: is BGM on at all */
    long audioAcc;                        /* the fraction of a tick carried */
    int bootPhase;                        /* 0..5, the sequence below */
    int bootStep;                         /* steps done inside a phase */

    int record[MEN_STAGES];              /* 0 = never cleared */

    int px;                              /* the board's tile size and origin */
    int ox, oy;
    int boxKind;                         /* which of the fifteen packages */
    int result;                          /* RESULT_* */
    int overStep;                        /* how much of the picture is in */
    int overTick;

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

/* Start one of the six songs, or -1 for silence. */
void app_music(App *a, int n);

/* Fill `frames` of mono 16-bit, ticking the driver as the OPN timer would. */
void app_audio(App *a, short *out, int frames, int rate);

#endif
