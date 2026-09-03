/* The MMD2 player.  See mmd2.h and docs/sound.md for where every number in
 * here came from. */
#include <string.h>

#include "mmd2.h"

#include "mmd2_tables.inc"

/* A YM2203 takes the SSG on registers 0x00..0x0f and the FM above that, which
 * is how the driver's one write routine reaches both. */
static void reg(Mmd2 *m, int r, int v)
{
    if (r < 0x10) ssg_write(&m->ssg, r, v & 0xff);
    else opn_write(&m->opn, r, v & 0xff);
}

void mmd2_reset(Mmd2 *m)
{
    int k;

    memset(m, 0, sizeof *m);
    opn_reset(&m->opn, 3993600.0);
    ssg_reset(&m->ssg);

    /* FUN 0x0392: key every FM channel off, then silence the operators the
     * way the driver does before a voice is ever loaded. */
    for (k = 0; k < MMD2_FM; k++) reg(m, 0x28, k);
    for (k = 0x90; k <= 0x9e; k++) reg(m, k, 0);
    for (k = 0x80; k <= 0x8f; k++) reg(m, k, 0xff);
    for (k = 0x40; k <= 0x4f; k++) reg(m, k, 0x7f);

    /* FUN 0x03d8: the SSG mixer with everything shut off. */
    m->mixer = 0xbf;
    reg(m, 7, m->mixer);
    for (k = 0; k < 3; k++) reg(m, 8 + k, 0);
}

/* The FM voice: byte 0 to 0xb0 + ch, then 24 bytes to 0x30 + ch stepping 4
 * (1edb... no - MMD2.SYS 0x071a onwards). */
static void load_voice(Mmd2 *m, int ch, int n)
{
    const unsigned char *v;
    int k, r;

    if (!m->voi || n < 0 || n >= MMD2_VOICES) return;
    if (VOI_FM + (long)(n + 1) * MMD2_VOICE_BYTES > m->voiLen) return;
    v = m->voi + VOI_FM + n * MMD2_VOICE_BYTES;

    reg(m, 0x28, ch);                    /* key off first, as 0x06f6 does */
    for (k = 0; k < 4; k++) reg(m, 0x80 + ch + k * 4, 0xff);
    reg(m, 0xb0 + ch, v[0]);
    r = 0x30 + ch;
    for (k = 0; k < 24; k++) {
        reg(m, r, v[1 + k]);
        r += 4;
    }
}

/* The carrier operators of each algorithm, so a volume can be applied without
 * wrecking the modulators.  The driver leaves the voice's own total levels in
 * place and scales through 0xda4; putting that on the carriers only is what
 * makes the scale audible rather than a timbre change. */
static const unsigned char CARRIERS[8] = {
    0x08, 0x08, 0x08, 0x08, 0x0c, 0x0e, 0x0e, 0x0f
};

static void fm_level(Mmd2 *m, int ch, int voice, int vol)
{
    const unsigned char *v;
    int alg, k, add;

    if (!m->voi || voice < 0 || voice >= MMD2_VOICES) return;
    if (VOI_FM + (long)(voice + 1) * MMD2_VOICE_BYTES > m->voiLen) return;
    v = m->voi + VOI_FM + voice * MMD2_VOICE_BYTES;
    alg = v[0] & 7;
    add = mmd2Tl[15] - mmd2Tl[vol & 15];   /* louder volume, smaller offset */
    for (k = 0; k < 4; k++) {
        int tl;
        if (!((CARRIERS[alg] >> k) & 1)) continue;
        tl = (v[1 + 4 + k] & 0x7f) + add;
        if (tl > 0x7f) tl = 0x7f;
        reg(m, 0x40 + ch + k * 4, tl);
    }
}

static void key_off(Mmd2 *m, int t)
{
    Mmd2Track *tr = &m->tr[t];
    if (t < MMD2_FM) {
        reg(m, 0x28, t);                 /* 0x08a1: register 0x28, no bits */
    } else {
        /* 0x0887: 9 << ch sets both the tone and the noise bit for that
         * channel in the mixer. */
        int ch = t - MMD2_FM;
        m->mixer |= 9 << ch;
        reg(m, 7, m->mixer);
        reg(m, 8 + ch, 0);
    }
    (void)tr;
}

static void key_on(Mmd2 *m, int t, int note)
{
    Mmd2Track *tr = &m->tr[t];

    if (note < 0 || note >= MMD2_NOTES) return;
    if (t < MMD2_FM) {
        int f = mmd2Fnum[note];
        /* 0x09f6: register 0xa4 + ch takes (octave << 3) | the F-number's
         * high bits, and 0xa0 + ch the low byte. */
        tr->fnum = f & 0xff;
        tr->high = (f >> 8) & 7;
        reg(m, 0xa4 + t, ((tr->octave & 7) << 3) | tr->high);
        reg(m, 0xa0 + t, tr->fnum);
        fm_level(m, t, tr->voice, tr->vol);
        reg(m, 0x28, 0xf0 | t);          /* all four operators on */
    } else {
        int ch = t - MMD2_FM;
        int p = mmd2Period[note] >> (tr->octave & 7);
        if (p < 1) p = 1;
        if (p > 0xfff) p = 0xfff;
        reg(m, ch * 2, p & 0xff);
        reg(m, ch * 2 + 1, (p >> 8) & 0x0f);
        reg(m, 8 + ch, tr->vol & 15);
        /* Tone on; noise on as well when a noise period has been set. */
        m->mixer &= ~(1 << ch);
        if (tr->noise >= 0) {
            reg(m, 6, tr->noise & 0x1f);
            m->mixer &= ~(8 << ch);
        } else {
            m->mixer |= 8 << ch;
        }
        reg(m, 7, m->mixer);
    }
}

void mmd2_stop(Mmd2 *m)
{
    int t;
    for (t = 0; t < MMD2_TRACKS; t++) {
        m->tr[t].active = 0;
        key_off(m, t);
    }
    m->playing = 0;
}

int mmd2_play(Mmd2 *m, const unsigned char *song, long songLen,
              const unsigned char *voi, long voiLen)
{
    long p = 0;
    int t, found = 0;

    mmd2_reset(m);
    m->song = song;
    m->songLen = songLen;
    m->voi = voi;
    m->voiLen = voiLen;
    if (!song || songLen <= 0) return 0;

    /* FUN 0x0460's scan: a track runs to a 0x00, and a code of 0xfc carries
     * two argument bytes while anything above it carries one. */
    for (t = 0; t < MMD2_TRACKS && p < songLen; t++) {
        Mmd2Track *tr = &m->tr[t];
        memset(tr, 0, sizeof *tr);
        tr->active = 1;
        tr->p = p;
        tr->count = 1;                   /* fetch on the first tick */
        tr->len = 12;
        tr->vol = 15;
        tr->octave = 4;
        tr->noise = -1;
        tr->voice = -1;
        tr->loopBack = -1;
        found++;
        while (p < songLen) {
            unsigned char c = song[p++];
            if (c == 0) break;
            if (c < 0xfc) continue;
            if (p < songLen) p++;
            if (c == 0xfc && p < songLen) p++;
        }
    }
    m->playing = found > 0;
    return found;
}

/* One code.  Returns 1 when the code set a length, which is what ends the
 * fetch loop for this tick. */
static int step_code(Mmd2 *m, int t)
{
    Mmd2Track *tr = &m->tr[t];
    int code, n;

    if (tr->p < 0 || tr->p >= m->songLen) { tr->active = 0; return 1; }
    code = m->song[tr->p++];
    if (code == 0) {                     /* 0x061d: the track is over */
        key_off(m, t);
        tr->active = 0;
        return 1;
    }
    n = code - 1;                        /* the codes are 1-based */

    if (n < 36) {                        /* 1..36  a note */
        if (!(tr->flags & 0x40)) key_off(m, t);
        key_on(m, t, n);
        tr->count = tr->len;
        return 1;
    }
    n -= 36;
    if (n < 1) {                         /* 37  rest */
        key_off(m, t);
        tr->count = tr->len;
        return 1;
    }
    n -= 1;
    if (n < 2) {                         /* 38, 39  octave -1 / +1 */
        tr->octave = (tr->octave + (n ? 1 : -1)) & 7;
        return 0;
    }
    n -= 2;
    if (n < 8) { tr->octave = n; return 0; }              /* 40..47 */
    n -= 8;
    if (n < 2) {                                          /* 48, 49 */
        tr->vol = (tr->vol + (n ? 1 : -1)) & 15;
        return 0;
    }
    n -= 2;
    if (n < 16) { tr->vol = n; return 0; }                /* 50..65 */
    n -= 16;
    if (n < 32) {                                         /* 66..97 */
        if (t < MMD2_FM) {
            tr->voice = n;
            load_voice(m, t, n);
        } else {
            tr->noise = n;                 /* 0x06eb: [si+5] = 0x80 */
        }
        return 0;
    }
    n -= 32;
    if (n < 16) return 0;                                 /* 98..113 */
    n -= 16;
    if (n < 1) {                                          /* 114  loop */
        if (tr->loopBack < 0) {
            tr->loopBack = tr->p;
        } else if (tr->loopCount > 0 && --tr->loopCount > 0) {
            tr->p = tr->loopBack;
        }
        return 0;
    }
    n -= 1;
    if (n < 16) return 0;                                 /* 115..130  vib */
    n -= 16;
    if (n < 16) return 0;                                 /* 131..146  slide */
    n -= 16;
    if (n < 4) {                                          /* 147..150 */
        tr->len += n < 2 ? -(int)(2 - n) : (int)(n - 1);
        if (tr->len < 1) tr->len = 1;
        return 0;
    }
    n -= 4;
    if (n < 91) { tr->len = n; return 0; }                /* 151..241 */
    n -= 91;
    if (n < 2) {                                          /* 242, 243  tie */
        if (n) tr->flags |= 0x40;
        else tr->flags &= ~0x40;
        return 0;
    }
    n -= 2;
    if (n < 8) {                                          /* 244..251  q */
        tr->flags = (tr->flags & ~7) | n;
        return 0;
    }
    n -= 8;
    if (n < 1) {                                          /* 252  jump */
        long to;
        if (tr->p + 1 >= m->songLen) { tr->active = 0; return 1; }
        to = (long)(m->song[tr->p] | (m->song[tr->p + 1] << 8));
        tr->p += 2;
        /* The operand is an address inside the driver's own song buffer at
         * 0x1384, so it becomes an offset from the start of the file. */
        to -= 0x1384;
        if (to >= 0 && to < m->songLen) tr->p = to;
        else tr->active = 0;
        return 1;                        /* one jump a tick, so a bad loop
                                          * cannot spin for ever */
    }
    n -= 1;
    if (tr->p >= m->songLen) { tr->active = 0; return 1; }
    {
        int arg = m->song[tr->p++];
        if (n < 1) {                                      /* 253  voice */
            tr->mode = arg;
        } else if (n < 2) {                               /* 254  length */
            tr->len = arg;
        } else {                                          /* 255  detune */
            tr->level = arg;
        }
    }
    return 0;
}

void mmd2_tick(Mmd2 *m)
{
    int t, advance;

    if (!m->playing) return;
    m->ticks++;
    /* 0x04dd: 0x40 into a byte, and the lengths only move when it wraps. */
    m->tickAcc = (m->tickAcc + MMD2_TEMPO_ADD) & 0xff;
    advance = m->tickAcc == 0;
    if (!advance) return;

    for (t = 0; t < MMD2_TRACKS; t++) {
        Mmd2Track *tr = &m->tr[t];
        int guard = 0;
        if (!tr->active) continue;
        if (tr->count > 0) tr->count--;
        while (tr->count == 0 && tr->active && guard++ < 256)
            if (step_code(m, t)) break;
    }

    for (t = 0, advance = 0; t < MMD2_TRACKS; t++)
        if (m->tr[t].active) advance = 1;
    if (!advance) m->playing = 0;
}

void mmd2_render(Mmd2 *m, short *out, int samples, int rate)
{
    int k;
    for (k = 0; k < samples; k++) out[k] = 0;
    ssg_render(&m->ssg, out, samples, rate);
    opn_render(&m->opn, out, samples, rate);
}
