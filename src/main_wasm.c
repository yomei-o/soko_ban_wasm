/* The browser side.  The page gets an RGBA buffer and blits it with
 * putImageData; there is no WebGL anywhere, because the machines this has to
 * run on do not have it.
 *
 * The game's own files are embedded at disk/... so app.c's fopen finds them
 * unchanged from the native build.
 */
#include <emscripten.h>
#include <string.h>

#include "app.h"

static App app;
static unsigned char rgba[GFX_W * GFX_H * 4];
static int ready;

EMSCRIPTEN_KEEPALIVE int soko_init(void)
{
    int rc = app_init(&app, "disk");
    ready = rc == 0;
    return rc;
}

EMSCRIPTEN_KEEPALIVE int soko_width(void) { return GFX_W; }
EMSCRIPTEN_KEEPALIVE int soko_height(void) { return GFX_H; }

EMSCRIPTEN_KEEPALIVE void soko_key(int k)
{
    if (ready) app_key(&app, k);
}

EMSCRIPTEN_KEEPALIVE void soko_click(int x, int y)
{
    if (ready) app_click(&app, x, y);
}

EMSCRIPTEN_KEEPALIVE void soko_move(int x, int y)
{
    if (ready) app_move(&app, x, y);
}

EMSCRIPTEN_KEEPALIVE void soko_tick(void)
{
    if (ready) app_tick(&app);
}

/* Run a slide out in one call, for scripted play and for the checks. */
EMSCRIPTEN_KEEPALIVE void soko_settle(void)
{
    if (ready) app_settle(&app);
}

EMSCRIPTEN_KEEPALIVE int soko_busy(void)
{
    return ready ? app_busy(&app) : 0;
}

/* Render and hand back the pixels.  The palette is whatever screen the app is
 * on, so the conversion has to happen here rather than once at startup. */
EMSCRIPTEN_KEEPALIVE unsigned char *soko_frame(void)
{
    int y, x;

    if (!ready) {
        memset(rgba, 0, sizeof rgba);
        return rgba;
    }
    app_render(&app);
    for (y = 0; y < GFX_H; y++)
        for (x = 0; x < GFX_W; x++) {
            const unsigned char *c = app.gfx.pal[app.gfx.px[y][x] & 15];
            unsigned char *o = rgba + ((long)y * GFX_W + x) * 4;
            o[0] = c[0];
            o[1] = c[1];
            o[2] = c[2];
            o[3] = 255;
        }
    return rgba;
}

/* Audio.  The page pulls this from a ScriptProcessorNode; there is no
 * WebAudio worklet here on purpose - the node is deprecated but works
 * everywhere, and the whole point of this port is to run on machines that do
 * not have the new things. */
#define AUDIO_MAX 8192
static short audio[AUDIO_MAX];

EMSCRIPTEN_KEEPALIVE short *soko_audio(int frames, int rate)
{
    if (frames > AUDIO_MAX) frames = AUDIO_MAX;
    if (!ready) {
        int k;
        for (k = 0; k < frames; k++) audio[k] = 0;
        return audio;
    }
    app_audio(&app, audio, frames, rate);
    return audio;
}

EMSCRIPTEN_KEEPALIVE void soko_music(int n) { if (ready) app_music(&app, n); }
EMSCRIPTEN_KEEPALIVE int soko_song(void) { return app.song; }

EMSCRIPTEN_KEEPALIVE int soko_screen(void) { return app.screen; }
EMSCRIPTEN_KEEPALIVE int soko_stage(void) { return app.game.stage; }
EMSCRIPTEN_KEEPALIVE int soko_pick(void) { return app.pick; }
EMSCRIPTEN_KEEPALIVE int soko_moves(void) { return app.game.moves; }
EMSCRIPTEN_KEEPALIVE int soko_pushes(void) { return app.game.pushes; }
EMSCRIPTEN_KEEPALIVE int soko_target(void)
{
    return app.game.st ? app.game.st->moves : 0;
}
EMSCRIPTEN_KEEPALIVE int soko_done(void) { return app.game.done; }
EMSCRIPTEN_KEEPALIVE int soko_boxes(void) { return app.game.boxes; }
EMSCRIPTEN_KEEPALIVE int soko_won(void) { return game_won(&app.game); }
EMSCRIPTEN_KEEPALIVE int soko_result(void) { return app.result; }

EMSCRIPTEN_KEEPALIVE void soko_play(int stage)
{
    if (ready) app_play(&app, stage);
}

int main(void) { return 0; }
