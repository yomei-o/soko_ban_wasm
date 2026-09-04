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

/* Which operators are carriers, per algorithm.  MMD2.SYS 0x093b picks the
 * registers to write by the same three-way split: algorithms 0..3 touch only
 * 0x4c + ch, which is operator 4; 4 is two carriers; 5 and 6 are three; 7 is
 * all four. */
static const unsigned char CARRIERS[8] = {
    0x08, 0x08, 0x08, 0x08, 0x0c, 0x0e, 0x0e, 0x0f
};

/* 0x08a8's FM half, at 0x093b: put the level on the carriers.
 *
 * A single-carrier algorithm (0..3) takes the level straight - 0x0959 sends it
 * to register 0x4c + ch and nothing else.  The others (0x0965, 0x0978, 0x0994)
 * first take the SMALLEST of the carriers' own total levels out of the voice,
 * then write `voiceTl + (level - smallest)` to each of them: the carriers keep
 * whatever balance the voice gave them and the loudest of them lands exactly
 * on the level.
 *
 * Writing the level to every carrier instead - which is what this did at first
 * - makes a three-carrier voice far louder than its volume asks for. */
static void fm_level(Mmd2 *m, int ch, int voice, int level)
{
    const unsigned char *v;
    int alg, k, lowest = 0x7f, off;

    if (!m->voi || voice < 0 || voice >= MMD2_VOICES) return;
    if (VOI_FM + (long)(voice + 1) * MMD2_VOICE_BYTES > m->voiLen) return;
    v = m->voi + VOI_FM + voice * MMD2_VOICE_BYTES;
    alg = v[0] & 7;

    if (alg < 4) {                       /* 0x0959 */
        reg(m, 0x4c + ch, level & 0x7f);
        return;
    }
    for (k = 0; k < 4; k++)
        if (((CARRIERS[alg] >> k) & 1) && (v[5 + k] & 0x7f) < lowest)
            lowest = v[5 + k] & 0x7f;
    off = level - lowest;
    for (k = 0; k < 4; k++) {
        int tl;
        if (!((CARRIERS[alg] >> k) & 1)) continue;
        tl = (v[5 + k] & 0x7f) + off;
        if (tl < 0) tl = 0;
        if (tl > 0x7f) tl = 0x7f;
        reg(m, 0x40 + ch + k * 4, tl);
    }
}

/* 0x08a8's front half: the fade-out.
 *
 * Each channel has a (count, attenuation) pair.  Every call bumps the count,
 * and when it reaches the speed the attenuation takes a step - 4 on the FM
 * side up to 0x7f (0x0921), 1 on the SSG side up to 15 (0x08d8) - and the
 * level that is about to be written moves by it.  FM levels are attenuations
 * so they go up; SSG volumes are volumes so they go down.
 *
 * The count is bumped by the CALL, not by the tick, which is why 0x05d3 calls
 * this once a tick per track while a fade is running: notes and volume codes
 * call it too, so a busy channel fades slightly faster than a quiet one. */
static int fade_level(Mmd2 *m, int t, int level)
{
    int speed = m->fade & 0x7f, a;

    if (!(m->fade & 0x80)) return level;
    if (m->fadeCount[t] == speed) {
        m->fadeCount[t] = 0;
        a = m->fadeAtten[t] + (t < MMD2_FM ? 4 : 1);
        if (t < MMD2_FM) m->fadeAtten[t] = a >= 0x80 ? 0x7f : a;
        else m->fadeAtten[t] = a >= 0x0f ? 0x0f : a;
    } else {
        m->fadeCount[t]++;
    }
    a = m->fadeAtten[t];
    if (t < MMD2_FM) {
        level += a;                      /* 0x0931 */
        return level >= 0x80 ? 0x7f : level;
    }
    level -= a;                          /* 0x08ea */
    return level < 0 ? 0 : level;
}

/* 0x08a8 as a whole: the level out to whichever half of the chip the track
 * lives on.
 *
 * The SSG side differs from the driver in one byte and only when the volume
 * has reached nought: 0x08fd sends 0xff rather than 0, which on the chip is
 * "follow the envelope generator".  The driver never writes registers 11, 12
 * or 13, so that generator is never started and the channel is silent either
 * way - and ssg.c has no envelope generator, so 0xff would come out as full
 * volume.  0 is what the chip actually sounds like. */
static void apply_level(Mmd2 *m, int t)
{
    Mmd2Track *tr = &m->tr[t];
    int level = fade_level(m, t, tr->level);
    if (t < MMD2_FM) fm_level(m, t, tr->voice, level);
    else reg(m, 8 + (t - MMD2_FM), level & 15);
}

/* 0x09f6: the pitch word out.  For FM register 0xa4 + ch takes
 * (octave << 3) | the F-number's high bits and 0xa0 + ch the low byte; for the
 * SSG it is the period, low byte then high nibble. */
static void write_pitch(Mmd2 *m, int t)
{
    Mmd2Track *tr = &m->tr[t];
    if (t < MMD2_FM) {
        reg(m, 0xa4 + t, ((tr->octave & 7) << 3) | ((tr->pitch >> 8) & 7));
        reg(m, 0xa0 + t, tr->pitch & 0xff);
    } else {
        int ch = t - MMD2_FM;
        int p = tr->pitch;
        if (p < 1) p = 1;
        if (p > 0xfff) p = 0xfff;
        reg(m, ch * 2, p & 0xff);
        reg(m, ch * 2 + 1, (p >> 8) & 0x0f);
    }
}

/* 0x06bf: the volume goes into +0x07, the level into +0x08 - through 0xda4's
 * table on the FM side and raw on the SSG side - and the envelope counters are
 * cleared. */
static void set_volume(Mmd2 *m, int t, int vol)
{
    Mmd2Track *tr = &m->tr[t];
    tr->vol = vol & 15;
    tr->level = t < MMD2_FM ? mmd2Tl[tr->vol] : tr->vol;
    tr->lvlCount = 0;
    tr->lvlPeriod = 0;
    (void)m;
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
        tr->pitch = mmd2Fnum[note];
        write_pitch(m, t);
        apply_level(m, t);
        reg(m, 0x28, 0xf0 | t);          /* all four operators on */
    } else {
        int ch = t - MMD2_FM;
        /* 0x064f: the SSG period is the table entry shifted down by the
         * octave, rounded up. */
        tr->pitch = mmd2Period[note] >> (tr->octave & 7);
        write_pitch(m, t);
        apply_level(m, t);
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

void mmd2_fade(Mmd2 *m, int speed)
{
    if (!m->playing) { mmd2_stop(m); return; }   /* 0x0130 */
    if (m->fade & 0x80) return;                  /* 0x013d: one at a time */
    m->fade = (speed & 0x7f) | 0x80;             /* 0x0420 */
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
        tr->level = t < MMD2_FM ? mmd2Tl[15] : 15;
        tr->octave = 4;
        tr->noise = -1;
        tr->voice = -1;
        tr->loopTop = 0;
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
        tr->notes++;
        tr->count = tr->len;
        /* 0x080a: the key-off point is length * q / 8, computed as
         * (len << 8) >> 3 times q, keeping the high byte. */
        tr->gate = (tr->len * 32 * (tr->flags & 7)) >> 8;
        return 1;   /* 0x0603 cleared the slide counter before the fetch */
    }
    n -= 36;
    if (n < 1) {                         /* 37  rest */
        key_off(m, t);
        tr->count = tr->len;
        tr->gate = 0;
        /* 0x0697: a rest adds 0x40 to the flags, so a tie that was set turns
         * bit 7 on, and the routine then clears bit 7 - which leaves both
         * gone.  A rest cancels a tie. */
        tr->flags &= ~0x40;
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
        set_volume(m, t, (tr->vol + (n ? 1 : -1)) & 15);
        return 0;
    }
    n -= 2;
    if (n < 16) { set_volume(m, t, n); return 0; }         /* 50..65 */
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
    if (n < 16) {                                         /* 98..113 */
        /* 0x0747: push a frame.  The count is the offset in the range and 0
         * means for ever, which is what every track's opening 0x62 asks for. */
        if (tr->loopTop < MMD2_LOOPS) {
            tr->loopCount[tr->loopTop] = n;
            tr->loopBack[tr->loopTop] = tr->p;
            tr->loopTop++;
        }
        return 0;
    }
    n -= 16;
    if (n < 1) {                                          /* 114  loop end */
        /* 0x076a: count 0 always goes back; otherwise count down and pop the
         * frame when it reaches zero.
         *
         * THIS MUST NOT COST A TICK.  0x076a ends in a plain `ret`, so it
         * comes back to 0x063b, whose `jmp 0x618` reads the next code in the
         * same pass - only the note handler at 0x063d and the rest handler at
         * 0x068e escape the fetch loop, and they do it by popping the return
         * address.  Returning 1 here made a track lose one tick every time it
         * came round, so tracks with short loops fell behind tracks with long
         * ones and the channels drifted apart over a minute or so. */
        int top = tr->loopTop - 1;
        if (top < 0) return 0;
        if (tr->loopCount[top] == 0) {
            tr->p = tr->loopBack[top];
            return 0;
        }
        if (--tr->loopCount[top] > 0) {
            tr->p = tr->loopBack[top];
            return 0;
        }
        tr->loopTop = top;
        return 0;
    }
    n -= 1;
    if (n < 16) {                                         /* 115..130 */
        /* 0x078f: three bytes out of .VOI+0x00. */
        long o = VOI_VIB + (long)n * 3;
        if (o + 2 < m->voiLen) {
            tr->lvlCount = tr->lvlPeriod = m->voi[o];
            tr->lvlLimit = m->voi[o + 1];
            tr->lvlDelta = m->voi[o + 2];
            tr->lvlSaved = tr->level;
        }
        return 0;
    }
    n -= 16;
    if (n < 16) {                                         /* 131..146 */
        /* 0x07b8: three bytes out of .VOI+0x30, the last two a signed word. */
        long o = VOI_SLIDE + (long)n * 3;
        if (o + 2 < m->voiLen) {
            int d = m->voi[o + 1] | (m->voi[o + 2] << 8);
            if (d & 0x8000) d -= 0x10000;
            tr->sldCount = tr->sldPeriod = m->voi[o];
            tr->sldDelta = d;
        }
        return 0;
    }
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
        /* Like the loop end, a jump reads on in the same pass; the guard in
         * mmd2_tick is what stops a bad one spinning. */
        return 0;
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

    /* 0x0288: while the close-down count is up, the interrupt does no music
     * at all - it just adds 8 to a byte, and when that wraps the driver stops
     * calling itself playing.  Thirty-two interrupts, an eighth of a second. */
    if (m->closing) {
        m->closing = (m->closing + 8) & 0xff;
        if (!m->closing) m->playing = 0;
        return;
    }

    /* 0x048d's head: the fade is over when every FM channel has gone past
     * [0x0f7d].  0x0374 clears the player and the fade, 0x041b re-opens the
     * song, and the count at 0x0cc8 starts. */
    if (m->fadeAtten[0] > MMD2_FADE_END && m->fadeAtten[1] > MMD2_FADE_END &&
        m->fadeAtten[2] > MMD2_FADE_END) {
        mmd2_play(m, m->song, m->songLen, m->voi, m->voiLen);
        m->closing = 8;
        return;
    }

    m->ticks++;
    /* 0x04dd: 0x40 into a byte, and the lengths only move when it wraps. */
    m->tickAcc = (m->tickAcc + MMD2_TEMPO_ADD) & 0xff;
    advance = m->tickAcc == 0;
    if (!advance) return;

    for (t = 0; t < MMD2_TRACKS; t++) {
        Mmd2Track *tr = &m->tr[t];
        int guard = 0, applied;
        if (!tr->active) continue;
        if (tr->count > 0) tr->count--;

        /* 0x05a5: the level envelope.  The bound at 0x05b1 is an 8-bit add
         * whose carry decides which way the limit is a stop - a positive delta
         * climbs to it, a negative one falls to it. */
        applied = 0;
        if (tr->lvlCount > 0 && --tr->lvlCount == 0) {
            int sum = (tr->level + tr->lvlDelta) & 0xff;
            int carried = tr->level + tr->lvlDelta > 0xff;
            if (carried ? sum >= tr->lvlLimit : sum <= tr->lvlLimit) {
                tr->level = sum;
                apply_level(m, t);
                applied = 1;
            }
            tr->lvlCount = tr->lvlPeriod;
        }

        /* 0x05d3: every path that did not just write the level comes through
         * here, and while a fade is running it writes it anyway - which is
         * what walks the ramp along. */
        if (!applied && (m->fade & 0x80)) apply_level(m, t);

        /* 0x05e6: the pitch slide, every `period` ticks and unbounded. */
        if (tr->sldCount > 0 && --tr->sldCount == 0) {
            tr->sldCount = tr->sldPeriod;
            tr->pitch += tr->sldDelta;
            write_pitch(m, t);
        }
        /* 0x0584: the note is released when what is left of it reaches the
         * gate point.  A gate of nought only releases on the FM side - the
         * SSG holds until the next note. */
        if (tr->count == tr->gate && (tr->gate != 0 || t < MMD2_FM))
            key_off(m, t);
        if (tr->count == 0 && tr->active) {
            /* 0x0603..0x0617, run once before the codes are read:
             *
             *     [si+0x10] = 0                 stop any slide
             *     al = [si+0x0c]                the level envelope's period
             *     if (al != 0) {
             *         [si+0x0b] = al            re-arm its counter
             *         [si+8] = [si+0x0f]        and put the level back
             *     }
             *
             * The slide has to be stopped HERE and not in the note handler.
             * A track writes `slide` and then the note, so clearing it when
             * the note arrives throws away the slide that was just asked for
             * - which is what this port did, and the lead came out as flat
             * notes with no glide at all. */
            tr->sldCount = 0;
            if (tr->lvlPeriod) {
                tr->lvlCount = tr->lvlPeriod;
                tr->level = tr->lvlSaved;
            }
        }
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

/* Timer B fires every MMD2_TIMER_DIV chip cycles, so a tick is
 * rate * MMD2_TIMER_DIV / MMD2_CLOCK samples - not a whole number, so `acc`
 * carries the remainder in 256ths across calls. */
void mmd2_run(Mmd2 *m, short *out, long frames, int rate, long *acc)
{
    /* In double, not integers: rate * MMD2_TIMER_DIV * 256 is 2.1e11 at
     * 44.1 kHz, which overflows a 32-bit long and made the ticking nonsense. */
    long spt = (long)((double)rate * (double)MMD2_TIMER_DIV * 256.0
                      / (double)MMD2_CLOCK);
    long done = 0;

    if (spt < 256) spt = 256;
    while (done < frames) {
        long n;
        if (*acc <= 0) {
            mmd2_tick(m);
            *acc += spt;
        }
        n = (*acc + 255) / 256;
        if (n > frames - done) n = frames - done;
        if (n < 1) n = 1;
        mmd2_render(m, out + done, (int)n, rate);
        *acc -= n * 256;
        done += n;
    }
}
