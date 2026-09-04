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

enum { SCR_BOOT, SCR_TITLE, SCR_SELECT, SCR_PLAY, SCR_END };

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

/* [0x04a6], the speed the game hands to the driver's AH=6, and what it costs:
 * seventeen steps nine ticks apart, plus the close-down count, is about 2.8
 * seconds.
 *
 * BGM_WAIT_TICKS is this port's own safety net rather than anything the game
 * has.  The driver's clock here is the audio callback, and a browser that has
 * not been given a gesture yet - or a host with no sound at all, like the
 * shot tool - never calls it, so the wait would never end.  Longer than the
 * fade can take, so it only ever fires when nothing is being heard anyway. */
#define BGM_FADE_SPEED 8
#define BGM_WAIT_TICKS 200

/* What the wait does once the fade is over. */
enum { WAIT_NOTHING, WAIT_SELECT, WAIT_PLAY, WAIT_END };

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

/* THE ENDING - FUN_1edb_40bc.
 *
 * The grid's loop asks FUN_1edb_427c whether all thirty are cleared (it counts
 * the cells by reading their pixels; the port asks its own records) and, when
 * they are, never comes back: the ending is an endless loop of five pictures.
 *
 *     bgm(1)
 *     loop:
 *       page(1) end1        the picture is decoded into the hidden page
 *       page(0) 00f9        the screen is scattered away to black
 *               010e        the ending palette goes on
 *       page(1) 0180        the hidden page is saved to the buffer at 0x80000
 *       page(0) 01f5        and scattered back on, black and all
 *               wait 5000
 *       page(1) 1a83 staff1
 *       page(0) 0440        the credits scatter on, colour 0 transparent
 *               wait 8192
 *               01f5        the buffer comes back, wiping the credits
 *       page(1) end2 0180
 *       page(0) 01f5
 *       page(1) 1a83 staff3
 *       page(0) 0440
 *       page(1) 1a83 staff4
 *               wait 4000
 *       page(0) 01f5 0440   end2 back, then staff4 on top
 *               wait 7000
 *               0019        the hidden page replaces the screen: black plus
 *                           the staff4 strip, nothing else
 *               wait 4096
 *
 * All three effects are the same twenty-one pass, 42-byte scatter as the clear
 * picture (OVER_* above); they differ only in what lands:
 *
 *     0440   colour 0 is left alone            the picture goes on top
 *     01f5   the source lands as it is         the buffer replaces the screen
 *     0019   the same, but out of the hidden page rather than the buffer
 *     00f9   nothing lands; the screen clears  four pixels at a time, four
 *                                              sub-passes to a pass
 *
 * The waits are milliseconds - FUN_28a3_000e counts HSYNCs with a constant
 * INT 18h picked for the machine's speed, 100ms a hundred units.  The port's
 * tick is the browser's frame, so END_MS turns them into frames. */
#define END_MS(n) ((n) * 6 / 100)

enum {
    END_LOAD1,        /* end1 into the buffer */
    END_ERASE,        /* 00f9 */
    END_IN1,          /* 01f5, end1 */
    END_HOLD1,        /* 5000 */
    END_STAFF1,       /* 0440, staff1 */
    END_HOLD2,        /* 8192 */
    END_BACK1,        /* 01f5, end1 again */
    END_IN2,          /* end2 into the buffer, then 01f5 */
    END_STAFF3,       /* 0440, staff3 */
    END_STAFF4,       /* staff4 is loaded and the screen waits 4000 */
    END_BACK2,        /* 01f5, end2 again */
    END_STAFF4_IN,    /* 0440, staff4 */
    END_HOLD3,        /* 7000 */
    END_STRIP,        /* 0019: the hidden page replaces the screen */
    END_HOLD4,        /* 4096, then round again */
    END_STEPS
};

/* SBPUSER.DAT - the records.
 *
 * FUN_2329_07c6 reads it with fread(&DS:0x3ee4, 10, 0x1f), so it is 31 records
 * of 10 bytes: one per stage with 0 unused, exactly the 310 bytes on the disk.
 * The record itself is the FIRST WORD of a record - FUN_2329_0861 reads
 * `*(word *)(stage * 10 + 0x3ee4)` and FUN_2329_05b8 writes it, both indexed
 * by the stage number - and what the other eight bytes are for has not been
 * found; the shipped file is all zeros.  This port keeps them zero.
 *
 * FUN_1edb_3d80 writes the two bytes back to the file and then re-reads the
 * whole thing, which is how the table in memory catches up.
 *
 * WHEN it writes is the part this port cannot follow.  A stage cleared for the
 * first time (its record still 0) is saved with no fuss, but clearing one that
 * already has a record puts up
 *
 *     前回:%05d 今回:%05d steps でした、今回の手順を保存しますか？
 *
 * and waits for a mouse button - left saves, right does not.  That message
 * goes out through the PC-98's TEXT plane with the machine's own font ROM,
 * which is not ours to ship, so the port cannot draw it.  It behaves as though
 * the answer were always yes. */
#define USER_RECS 31
#define USER_REC_BYTES 10
#define USER_BYTES (USER_RECS * USER_REC_BYTES)

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
    /* The ending's two pages: `endBuf` is the RAM at 0x80000 that holds the
     * END picture, `endTop` the hidden VRAM page a staff strip sits in. */
    Cg endBuf, endTop;
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

    char dir[64];                        /* where the game's files are */

    Mmd2 mmd;
    unsigned char voi[1024];
    long voiLen;
    unsigned char bgm[BGM_COUNT][4096];
    long bgmLen[BGM_COUNT];
    int song;                             /* which one is playing, or -1 */
    int music;                            /* [0x128d]: is BGM on at all */

    /* FUN_24d7_00a8 does not come back until the driver has finished fading:
     * it asks for the fade, spins on AH=8, and only then loads the next song.
     * The game stands still for those three seconds - the screen it was on
     * stays up, keys do nothing, the clear picture does not begin to dissolve
     * - so whatever the caller meant to do next waits here with the song. */
    int songNext;                         /* the song the wait will load */
    int songWait;                         /* ticks waited; see BGM_WAIT_TICKS */
    int waitWhat;                         /* WAIT_*: what happens afterwards */
    int waitArg;
    long audioAcc;                        /* the fraction of a tick carried */
    int bootPhase;                        /* 0..5, the sequence below */
    int bootStep;                         /* steps done inside a phase */

    int record[MEN_STAGES];              /* 0 = never cleared */
    int recordStamp;                     /* bumped whenever one changes, so a
                                          * host can tell it has to save */

    int px;                              /* the board's tile size and origin */
    int ox, oy;
    int boxKind;                         /* which of the fifteen packages */
    int endStep;                         /* END_* */
    int endPass;                         /* 0..OVER_PASSES, the scatter */
    int endSub;                          /* 0..3 inside an erase pass */
    int endFrame;                        /* frames spent on this pass or wait */

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

/* SBPUSER.DAT's 310 bytes, built from the records and read back into them.
 * The host decides where to keep them: the page puts them in localStorage,
 * the native builds do not keep them at all. */
void app_user_save(const App *a, unsigned char *out);
void app_user_load(App *a, const unsigned char *in, long len);

/* Fill `frames` of mono 16-bit, ticking the driver as the OPN timer would. */
void app_audio(App *a, short *out, int frames, int rate);

#endif
