/* The thirty stages, out of SBPMEN.DAT. */
#ifndef SOKO_MEN_H
#define SOKO_MEN_H

#define MEN_REC 250                     /* one record */
#define MEN_TAIL 48                     /* what is left on the end */
#define MEN_ROWS 20
#define MEN_COLS 32
#define MEN_STAGES 30

/* Three bitmaps of 20 rows by 32 columns, plus where the man starts.
 *
 *   +0x00  1  tile size, 3 or 2; the seven widest boards use 2
 *   +0x01  1  the man's starting column
 *   +0x02  1  the man's starting row
 *   +0x03  2  the target move count, little endian
 *   +0x05  5  zero
 *   +0x0a  80 goals
 *   +0x5a  80 walls
 *   +0xaa  80 boxes
 *
 * A row's bit 31 is column 0, the same way round as the PC-98 screen.
 * Record 0 is empty; records 1..30 are the stages. */
typedef struct {
    unsigned char tile;                 /* 3 = 40px tiles, 2 = 32px */
    unsigned char sx, sy;
    unsigned short moves;               /* the target, not a limit */
    unsigned long goal[MEN_ROWS];       /* +0x0a, in file order */
    unsigned long wall[MEN_ROWS];       /* +0x5a */
    unsigned long box[MEN_ROWS];        /* +0xaa */
    int w, h;                           /* the used part of the board */
    int size;                           /* 0..3, the index into the tables */
    int tilePx;                         /* 20, 24, 32 or 40 */
    int shiftX, shiftY;                 /* where the board sits in the grid */
} Stage;

/* Parse the whole file.  `out` takes MEN_STAGES entries.  Returns how many
 * were read, so a short or absent file is visible rather than fatal. */
int men_load(Stage *out, const unsigned char *data, long len);

/* Work out the display size and where the board sits, FUN_1edb_31c5's way.
 * men_load calls it for every stage. */
void men_fit(Stage *s);

/* Bit tests, so nothing else needs to know which way round a row is. */
int men_wall(const Stage *s, int x, int y);
int men_goal(const Stage *s, int x, int y);
int men_box(const Stage *s, int x, int y);

/* 40 for tile 3, 32 for tile 2, which is what FUN_1edb_31c5 works out from
 * the board's extent; men_load fills the same answer into `tilePx`. */
int men_tile_px(const Stage *s);

/* FUN_1edb_31c5's four display sizes, indexed the same way the stage header's
 * first byte is: tile pixels, and the grid in cells (as a LAST INDEX, so one
 * less than the count). */
extern const int menTilePx[4];
extern const int menGridW[4];
extern const int menGridH[4];

#endif
