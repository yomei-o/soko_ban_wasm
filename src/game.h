/* The rules: one man, some boxes, the same number of goals. */
#ifndef SOKO_GAME_H
#define SOKO_GAME_H

#include "men.h"

#define GAME_UNDO 4096                  /* TRACE.DAT is 112000 bytes, so the
                                         * original keeps a long history too */

enum { DIR_UP, DIR_RIGHT, DIR_DOWN, DIR_LEFT };

typedef struct {
    const Stage *st;
    int stage;                          /* 1..30 */
    int x, y;                           /* the man */
    int facing;
    unsigned long box[MEN_ROWS];        /* moves; walls and goals do not */
    int moves, pushes;
    int boxes, done;                    /* how many boxes, how many on goals */
    int won;
    unsigned char hist[GAME_UNDO];      /* direction | 4 if a box came along */
    int histLen;
} Game;

void game_start(Game *g, const Stage *st, int stage);

/* Returns 1 if the man moved.  A push that cannot happen is not a move, and
 * that is what the original's step counter agrees with: only a step that
 * happens is counted. */
int game_step(Game *g, int dir);

int game_undo(Game *g);

int game_box(const Game *g, int x, int y);
int game_won(const Game *g);

#endif
