/* The rules.  Sokoban is small enough that the interesting part is not the
 * move but what the original counts and what it lets you undo, and both of
 * those are visible on the game's own windows: SCORE shows STAGE / STEPS /
 * LIMIT, and TRACE replays a stored run one step at a time.
 */
#include "game.h"

static const int DX[4] = { 0, 1, 0, -1 };
static const int DY[4] = { -1, 0, 1, 0 };

static void set_box(Game *g, int x, int y, int on)
{
    unsigned long m;
    if (x < 0 || y < 0 || x >= MEN_COLS || y >= MEN_ROWS) return;
    m = 1UL << (MEN_COLS - 1 - x);
    if (on) g->box[y] |= m;
    else g->box[y] &= ~m;
}

int game_box(const Game *g, int x, int y)
{
    if (x < 0 || y < 0 || x >= MEN_COLS || y >= MEN_ROWS) return 0;
    return (g->box[y] >> (MEN_COLS - 1 - x)) & 1UL ? 1 : 0;
}

static void recount(Game *g)
{
    int x, y;
    g->boxes = 0;
    g->done = 0;
    for (y = 0; y < MEN_ROWS; y++)
        for (x = 0; x < MEN_COLS; x++)
            if (game_box(g, x, y)) {
                g->boxes++;
                if (men_goal(g->st, x, y)) g->done++;
            }
    g->won = g->boxes > 0 && g->boxes == g->done;
}

void game_start(Game *g, const Stage *st, int stage)
{
    int k;

    g->st = st;
    g->stage = stage;
    g->x = st->sx;
    g->y = st->sy;
    g->facing = DIR_DOWN;               /* the sheet's first walking frame */
    for (k = 0; k < MEN_ROWS; k++) g->box[k] = st->box[k];
    g->moves = 0;
    g->pushes = 0;
    g->histLen = 0;
    recount(g);
}

int game_step(Game *g, int dir)
{
    int nx, ny, bx, by, pushed = 0;

    if (dir < 0 || dir > 3) return 0;
    g->facing = dir;                    /* turning happens even if blocked */
    if (g->won) return 0;

    nx = g->x + DX[dir];
    ny = g->y + DY[dir];
    if (men_wall(g->st, nx, ny)) return 0;

    if (game_box(g, nx, ny)) {
        bx = nx + DX[dir];
        by = ny + DY[dir];
        if (men_wall(g->st, bx, by) || game_box(g, bx, by)) return 0;
        set_box(g, nx, ny, 0);
        set_box(g, bx, by, 1);
        pushed = 1;
    }

    g->x = nx;
    g->y = ny;
    g->moves++;
    if (pushed) g->pushes++;
    if (g->histLen < GAME_UNDO)
        g->hist[g->histLen++] = (unsigned char)(dir | (pushed ? 4 : 0));
    recount(g);
    return 1;
}

int game_undo(Game *g)
{
    int e, dir, pushed, bx, by;

    if (g->histLen <= 0) return 0;
    e = g->hist[--g->histLen];
    dir = e & 3;
    pushed = e & 4;

    if (pushed) {
        /* After a push the man stands on the box's old square and the box is
         * one step further on, so the box comes back to where the man is and
         * the man steps back off it. */
        bx = g->x + DX[dir];
        by = g->y + DY[dir];
        set_box(g, bx, by, 0);
        set_box(g, g->x, g->y, 1);
        g->pushes--;
    }
    g->x -= DX[dir];
    g->y -= DY[dir];
    g->moves--;
    g->won = 0;
    recount(g);
    return 1;
}

int game_won(const Game *g) { return g->won; }
