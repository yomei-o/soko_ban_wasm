/* Render a board to a PNG, with no window anywhere.
 *
 *     tmp/soko_shot.exe board 11 tmp/s11.png
 *     tmp/soko_shot.exe board 1 tmp/s1.png --press rrdd
 *     tmp/soko_shot.exe pic disk/TITLE.CG tmp/title.png --pal 0
 *     tmp/soko_shot.exe tiles tmp/tiles.png 40
 *
 * Verification on this machine is by looking at a PNG, never by opening a
 * window: the desktop is shared.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "cg.h"
#include "game.h"
#include "gfx.h"
#include "men.h"
#include "png.h"

static unsigned char *slurp(const char *path, long *len)
{
    FILE *f = fopen(path, "rb");
    unsigned char *b;
    long n;

    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    b = malloc((size_t)n + 1);
    if (!b) { fclose(f); return NULL; }
    if (fread(b, 1, (size_t)n, f) != (size_t)n) { free(b); fclose(f); return NULL; }
    fclose(f);
    *len = n;
    return b;
}

static int save(const char *path, const Gfx *g)
{
    unsigned char pal[16][3];
    int i, j;
    for (i = 0; i < 16; i++)
        for (j = 0; j < 3; j++) pal[i][j] = g->pal[i][j];
    /* png_write_indexed returns 1 for success, so this inverts it to the
     * 0-is-fine convention the callers below use. */
    return png_write_indexed(path, GFX_W, GFX_H, &g->px[0][0], GFX_W, pal, 16)
           ? 0 : -1;
}

/* The board, centred the way the original centres it. */
static void draw_board(Gfx *g, const Cg *sheet, const Game *gm)
{
    const Stage *s = gm->st;
    int px = s->tilePx;
    int ox = s->shiftX * px;
    int oy = s->shiftY * px;
    int x, y;

    gfx_clear(g, 3);
    for (y = 0; y < s->h; y++) {
        for (x = 0; x < s->w; x++) {
            int dx = ox + x * px, dy = oy + y * px;
            int t = -1;
            /* The sheet's first cell is blank, which is the floor: without it
             * only the walls and the marked squares would have any ground
             * under them and the rest of the board would stay black. */
            gfx_tile(g, sheet, px, T_FLOOR, dx, dy, -1);
            if (men_wall(s, x, y)) t = T_WALL;
            else if (game_box(gm, x, y))
                t = men_goal(s, x, y) ? T_BOX_ON_GOAL
                                      : T_BOX + (gm->stage - 1) % T_BOX_KINDS;
            else if (men_goal(s, x, y)) t = T_GOAL;
            if (t >= 0) gfx_tile(g, sheet, px, t, dx, dy, -1);
        }
    }
    {
        int pushing = gm->histLen > 0 && (gm->hist[gm->histLen - 1] & 4);
        gfx_tile(g, sheet, px, gfx_man(gm->facing, pushing, gm->moves),
                 ox + gm->x * px, oy + gm->y * px, 1);
    }
}

static int arg_int(int argc, char **argv, const char *flag, int def)
{
    int i;
    for (i = 1; i < argc - 1; i++)
        if (!strcmp(argv[i], flag)) return atoi(argv[i + 1]);
    return def;
}

static const char *arg_str(int argc, char **argv, const char *flag)
{
    int i;
    for (i = 1; i < argc - 1; i++)
        if (!strcmp(argv[i], flag)) return argv[i + 1];
    return NULL;
}

int main(int argc, char **argv)
{
    static Cg sheet;
    static Gfx g;
    static Stage stages[MEN_STAGES];
    unsigned char *d;
    long len;

    if (argc < 3) {
        fprintf(stderr, "usage: soko_shot screen boot|title|select|end|<n> <out.png>\n"
                        "       soko_shot board <n> <out.png> [--press rrdd]\n"
                        "       soko_shot pic <file.cg> <out.png> [--pal n]\n"
                        "       soko_shot tiles <out.png> [40|32]\n");
        return 2;
    }

    if (!strcmp(argv[1], "pic")) {
        static Cg pic;
        int pal = arg_int(argc, argv, "--pal", CG_PAL_TILES);
        int planes = arg_int(argc, argv, "--planes",
                             strlen(argv[2]) && argv[2][strlen(argv[2]) - 1] == 'M'
                             ? 1 : 4);
        d = slurp(argv[2], &len);
        if (!d) { fprintf(stderr, "cannot read %s\n", argv[2]); return 1; }
        cg_load(&pic, d, len, planes);
        free(d);
        gfx_palette(&g, pal);
        gfx_blit(&g, &pic, 0, 0, GFX_W, GFX_H, 0, 0);
        if (save(argv[3], &g)) { fprintf(stderr, "cannot write\n"); return 1; }
        printf("%s: %d planes -> %s\n", argv[2], pic.planes, argv[3]);
        return 0;
    }

    d = slurp("disk/CHR98N.CG", &len);
    if (!d) { fprintf(stderr, "cannot read disk/CHR98N.CG\n"); return 1; }
    cg_load(&sheet, d, len, 4);
    free(d);
    gfx_palette(&g, CG_PAL_TILES);

    if (!strcmp(argv[1], "tiles")) {
        int px = argc > 3 ? atoi(argv[3]) : 40;
        int t;
        gfx_clear(&g, 0);
        for (t = 0; t < 32; t++) {
            int sx, sy;
            gfx_tile_at(px, t, &sx, &sy);
            gfx_blit(&g, &sheet, sx, sy, px, px,
                     (t % 12) * (px + 4) + 4, (t / 12) * (px + 4) + 4);
        }
        if (save(argv[2], &g)) { fprintf(stderr, "cannot write %s\n", argv[2]); return 1; }
        printf("tiles %d -> %s\n", px, argv[2]);
        return 0;
    }

    if (!strcmp(argv[1], "screen")) {
        static App app;
        const char *which = argv[2];
        int rc = app_init(&app, "disk");
        if (rc) { fprintf(stderr, "app_init failed: %d\n", rc); return 1; }
        if (!strcmp(which, "boot")) {
            int n = arg_int(argc, argv, "--ticks", 0);
            app.screen = SCR_BOOT;
            while (n-- > 0) app_tick(&app);
        }
        else if (!strcmp(which, "title")) {
            app.screen = SCR_TITLE;
            gfx_palette(&app.gfx, CG_PAL_TITLE);   /* app_key's, skipped here */
        }
        else if (!strcmp(which, "end")) {
            /* The ending only happens when all thirty are cleared, so say so
             * and let the grid's own test start it. */
            int n;
            for (n = 0; n < MEN_STAGES; n++) app.record[n] = 1;
            app.screen = SCR_SELECT;
            gfx_palette(&app.gfx, CG_PAL_TITLE);
            app_render(&app);            /* the grid is what it erases */
        }
        else if (!strcmp(which, "select")) {
            app.screen = SCR_SELECT;
            gfx_palette(&app.gfx, CG_PAL_TITLE);
            app.pick = arg_int(argc, argv, "--pick", 0);
        } else {
            int n = atoi(which);
            app_play(&app, n ? n : 1);
        }
        {
            int extra = arg_int(argc, argv, "--ticks", 0);
            const char *keys = arg_str(argc, argv, "--press");
            if (keys) {
                const char *k;
                for (k = keys; *k; k++) {
                    int key = *k == 'u' ? KEY_UP : *k == 'r' ? KEY_RIGHT :
                              *k == 'd' ? KEY_DOWN : *k == 'l' ? KEY_LEFT :
                              *k == 'z' ? KEY_UNDO : -1;
                    if (key >= 0) {
                        app_key(&app, key);
                        app_settle(&app);   /* the slide finishes before the
                                             * next press is looked at */
                    }
                }
            }
            while (extra-- > 0) app_tick(&app);
        }
        app_render(&app);
        if (save(argv[3], &app.gfx)) { fprintf(stderr, "cannot write\n"); return 1; }
        printf("screen %s -> %s\n", which, argv[3]);
        return 0;
    }

    if (!strcmp(argv[1], "board")) {
        Game gm;
        int n = atoi(argv[2]);
        const char *keys = arg_str(argc, argv, "--press");

        d = slurp("disk/SBPMEN.DAT", &len);
        if (!d) { fprintf(stderr, "cannot read disk/SBPMEN.DAT\n"); return 1; }
        if (men_load(stages, d, len) < MEN_STAGES)
            fprintf(stderr, "warning: short stage file\n");
        free(d);
        if (n < 1 || n > MEN_STAGES) { fprintf(stderr, "stage 1..30\n"); return 2; }

        game_start(&gm, &stages[n - 1], n);
        if (keys) {
            const char *k;
            for (k = keys; *k; k++) {
                int dir = *k == 'u' ? DIR_UP : *k == 'r' ? DIR_RIGHT :
                          *k == 'd' ? DIR_DOWN : *k == 'l' ? DIR_LEFT : -1;
                if (dir >= 0) game_step(&gm, dir);
            }
        }
        draw_board(&g, &sheet, &gm);
        if (save(argv[3], &g)) { fprintf(stderr, "cannot write %s\n", argv[3]); return 1; }
        printf("stage %d %dx%d tile=%d moves=%d/%d pushes=%d on-goal=%d/%d%s"
               " -> %s\n", n, gm.st->w, gm.st->h, men_tile_px(gm.st),
               gm.moves, gm.st->moves, gm.pushes, gm.done, gm.boxes,
               game_won(&gm) ? " WON" : "", argv[3]);
        return 0;
    }

    fprintf(stderr, "unknown command %s\n", argv[1]);
    return 2;
}
