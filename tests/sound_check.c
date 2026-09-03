/* The music, checked without a speaker.
 *
 *     tmp/sound_check.exe            the checks
 *     tmp/sound_check.exe wav 0 8    write tmp/bgm0.wav, eight seconds
 *
 * Run from the repo root; it reads disk/SBPBGM*.BGM and disk/SBPVOICE.VOI.
 *
 * The pitch checks go through a Goertzel filter rather than a full transform:
 * one note is played, a second of it is rendered, and the energy at the note's
 * own frequency is compared with the energy a semitone either side.  That
 * catches a wrong note table, a wrong octave shift and a silent chip, which
 * are the three ways this can be wrong without being obviously wrong.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mmd2.h"

#define RATE 44100

static int fails;

static void ok(int cond, const char *what)
{
    if (!cond) { printf("FAIL %s\n", what); fails++; }
}

static unsigned char *slurp(const char *path, long *len)
{
    FILE *f = fopen(path, "rb");
    unsigned char *b;
    long n;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    b = malloc((size_t)n);
    if (!b || fread(b, 1, (size_t)n, f) != (size_t)n) { free(b); fclose(f); return NULL; }
    fclose(f);
    *len = n;
    return b;
}

/* Render `frames` at the timer's real rate. */
static void run(Mmd2 *m, short *out, long frames)
{
    long acc = 0;
    mmd2_run(m, out, frames, RATE, &acc);
}

static double rms(const short *p, long n)
{
    double s = 0;
    long k;
    for (k = 0; k < n; k++) s += (double)p[k] * p[k];
    return n ? sqrt(s / n) : 0;
}

/* Energy at one frequency. */
static double goertzel(const short *p, long n, double hz)
{
    double w = 2 * 3.14159265358979 * hz / RATE;
    double c = 2 * cos(w), s1 = 0, s2 = 0;
    long k;
    for (k = 0; k < n; k++) {
        double s0 = p[k] + c * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    return sqrt(s1 * s1 + s2 * s2 - c * s1 * s2) / n;
}

/* A one-note song, built by hand out of the codes in docs/sound.md, so a
 * pitch can be checked against a frequency this test works out itself. */
static long one_note(unsigned char *buf, int track, int note, int voice,
                     int octave, int vol)
{
    long p = 0;
    int k;
    for (k = 0; k < MMD2_TRACKS; k++) {
        if (k == track) {
            buf[p++] = (unsigned char)(40 + octave);   /* set octave */
            buf[p++] = (unsigned char)(50 + vol);      /* set volume */
            buf[p++] = (unsigned char)(66 + voice);    /* voice or noise */
            buf[p++] = (unsigned char)(151 + 90);      /* the longest length */
            buf[p++] = (unsigned char)(1 + note);      /* the note */
        }
        buf[p++] = 0;                                  /* end of track */
    }
    return p;
}

/* The FM F-number and block give the frequency straight from the datasheet. */
static double fm_hz(int note, int octave)
{
    double f = mmd2Fnum[note];
    return f * 3993600.0 / 2.0 / 1048576.0 * pow(2.0, octave - 4);
}

int main(int argc, char **argv)
{
    static Mmd2 m;
    static short buf[RATE * 12];
    static unsigned char song[96];
    unsigned char *voi;
    long voiLen;
    int k;

    voi = slurp("disk/SBPVOICE.VOI", &voiLen);
    if (!voi) { printf("FAIL cannot read disk/SBPVOICE.VOI\n"); return 1; }
    ok(voiLen == 1024, "SBPVOICE.VOI is 1024 bytes");
    ok(VOI_FM + MMD2_VOICES * MMD2_VOICE_BYTES == 1024,
       "the four sections fill it exactly");

    /* Every voice slot has a plausible algorithm and a carrier that is not
     * silenced, which is what says the 25-byte stride is right. */
    {
        int sane = 0;
        for (k = 0; k < MMD2_VOICES; k++) {
            const unsigned char *v = voi + VOI_FM + k * MMD2_VOICE_BYTES;
            int alg = v[0] & 7;
            if (alg <= 7 && v[1 + 4 + 3] <= 0x7f) sane++;
        }
        ok(sane == MMD2_VOICES, "all 32 voices look like OPN patches");
    }

    if (argc > 1 && !strcmp(argv[1], "wav")) {
        int n = argc > 2 ? atoi(argv[2]) : 0;
        int secs = argc > 3 ? atoi(argv[3]) : 8;
        char path[64], out[64];
        unsigned char *bgm;
        long bgmLen, frames;
        FILE *f;
        unsigned char hdr[44];
        long bytes;

        if (secs > 12) secs = 12;
        /* A single digit picks one of the six; anything else is taken as
         * a path, so a hand-made one-track song renders the same way. */
        if (argv[2][0] >= '0' && argv[2][0] <= '9' && !argv[2][1])
            snprintf(path, sizeof path, "disk/SBPBGM%d.BGM", n);
        else {
            snprintf(path, sizeof path, "%s", argv[2]);
            n = -1;
        }
        bgm = slurp(path, &bgmLen);
        if (!bgm) { printf("cannot read %s\n", path); return 1; }
        mmd2_play(&m, bgm, bgmLen, voi, voiLen);
        frames = (long)RATE * secs;
        run(&m, buf, frames);
        bytes = frames * 2;
        if (n < 0) snprintf(out, sizeof out, "tmp/one.wav");
        else snprintf(out, sizeof out, "tmp/bgm%d.wav", n);
        if (argv[2][1]) snprintf(out, sizeof out, "tmp/one.wav");
        f = fopen(out, "wb");
        if (!f) { printf("cannot write %s\n", out); return 1; }
        memcpy(hdr, "RIFF----WAVEfmt ", 16);
        hdr[16] = 16; hdr[17] = hdr[18] = hdr[19] = 0;
        hdr[20] = 1; hdr[21] = 0; hdr[22] = 1; hdr[23] = 0;
        hdr[24] = (unsigned char)(RATE & 0xff);
        hdr[25] = (unsigned char)((RATE >> 8) & 0xff);
        hdr[26] = hdr[27] = 0;
        hdr[28] = (unsigned char)((RATE * 2) & 0xff);
        hdr[29] = (unsigned char)(((RATE * 2) >> 8) & 0xff);
        hdr[30] = hdr[31] = 0;
        hdr[32] = 2; hdr[33] = 0; hdr[34] = 16; hdr[35] = 0;
        memcpy(hdr + 36, "data", 4);
        hdr[4] = (unsigned char)((bytes + 36) & 0xff);
        hdr[5] = (unsigned char)(((bytes + 36) >> 8) & 0xff);
        hdr[6] = (unsigned char)(((bytes + 36) >> 16) & 0xff);
        hdr[7] = (unsigned char)(((bytes + 36) >> 24) & 0xff);
        hdr[40] = (unsigned char)(bytes & 0xff);
        hdr[41] = (unsigned char)((bytes >> 8) & 0xff);
        hdr[42] = (unsigned char)((bytes >> 16) & 0xff);
        hdr[43] = (unsigned char)((bytes >> 24) & 0xff);
        fwrite(hdr, 1, 44, f);
        fwrite(buf, 2, (size_t)frames, f);
        fclose(f);
        printf("%s: %ld bytes, %d tracks -> %s (rms %.0f)\n", path, bgmLen,
               mmd2_play(&m, bgm, bgmLen, voi, voiLen), out, rms(buf, frames));
        return 0;
    }

    /* A single FM note has to come out at the frequency its F-number and
     * block say, and louder there than a semitone off. */
    {
        long n = one_note(song, 0, 15, 0, 4, 15);
        double want, at, up, down;
        mmd2_play(&m, song, n, voi, voiLen);
        run(&m, buf, RATE);
        want = fm_hz(15, 4);
        at = goertzel(buf, RATE, want);
        up = goertzel(buf, RATE, want * 1.0595);
        down = goertzel(buf, RATE, want / 1.0595);
        ok(rms(buf, RATE) > 200, "an FM note makes a sound");
        ok(at > up * 2 && at > down * 2, "and it is the right pitch");
        printf("  FM note 15 octave 4: %.1f Hz, energy %.0f (%.0f / %.0f a "
               "semitone off)\n", want, at, down, up);
    }

    /* The octave is a right shift on the SSG and a block on the FM, so an
     * octave up has to double the frequency on both sides. */
    {
        long n = one_note(song, 0, 15, 0, 5, 15);
        double at, half;
        mmd2_play(&m, song, n, voi, voiLen);
        run(&m, buf, RATE);
        at = goertzel(buf, RATE, fm_hz(15, 5));
        half = goertzel(buf, RATE, fm_hz(15, 4));
        ok(at > half * 2, "FM octave 5 is an octave above octave 4");
    }
    {
        long n = one_note(song, MMD2_FM, 15, 0, 4, 15);
        double want = 3993600.0 / 4.0 / 16.0 / (mmd2Period[15] >> 4);
        double at, up;
        mmd2_play(&m, song, n, voi, voiLen);
        run(&m, buf, RATE);
        at = goertzel(buf, RATE, want);
        up = goertzel(buf, RATE, want * 1.0595);
        ok(rms(buf, RATE) > 200, "an SSG note makes a sound");
        ok(at > up * 2, "and it is the right pitch");
        printf("  SSG note 15 octave 4: %.1f Hz, energy %.0f (%.0f a semitone "
               "off)\n", want, at, up);
    }

    /* Volume 0 has to be quieter than volume 15 on both sides. */
    for (k = 0; k < 2; k++) {
        int track = k ? MMD2_FM : 0;
        double loud, soft;
        long n = one_note(song, track, 15, 0, 4, 15);
        mmd2_play(&m, song, n, voi, voiLen);
        run(&m, buf, RATE / 2);
        loud = rms(buf, RATE / 2);
        n = one_note(song, track, 15, 0, 4, 0);
        mmd2_play(&m, song, n, voi, voiLen);
        run(&m, buf, RATE / 2);
        soft = rms(buf, RATE / 2);
        ok(loud > soft * 2, k ? "SSG volume 15 is louder than 0"
                              : "FM volume 15 is louder than 0");
    }

    /* NO DRIFT BETWEEN CHANNELS.  Every track opens with an endless loop, and
     * the loop end must not cost a tick - MMD2.SYS 0x076a returns to 0x063b,
     * whose `jmp 0x618` reads on in the same pass.  Getting that wrong makes a
     * track lose one tick a lap, so a short loop falls behind a long one and
     * the channels pull apart after a minute.
     *
     * Two hand-made tracks catch it: one whole note round a loop against two
     * half notes round a loop.  However long they run, the second has to have
     * played exactly twice as many notes as the first. */
    {
        long p = 0;
        int k;
        long ticks;
        /* track 0: loop { l96 note } */
        song[p++] = 98;                          /* loop, endless */
        song[p++] = (unsigned char)(151 + 90);   /* the longest coded length */
        song[p++] = 254; song[p++] = 96;         /* ... make it exactly 96 */
        song[p++] = 66;                          /* voice 0 */
        song[p++] = 1 + 12;
        song[p++] = 114;                         /* loop end */
        song[p++] = 0;
        /* track 1: loop { l48 note } - ONE note a lap, so it goes round twice
         * as often as track 0.  That is the point: if the loop end costs a
         * tick, this track pays it twice as often and the two pull apart.  Two
         * notes a lap would slow both by the same amount and hide it. */
        song[p++] = 98;
        song[p++] = 254; song[p++] = 48;
        song[p++] = 66;
        song[p++] = 1 + 12;
        song[p++] = 114;
        song[p++] = 0;
        for (k = 2; k < MMD2_TRACKS; k++) song[p++] = 0;

        mmd2_play(&m, song, p, voi, voiLen);
        /* Measured at one minute and again at four.  A boundary can leave the
         * counts one apart - the run simply stops between two of track 1's
         * notes - but that error must not GROW, and with the tick-losing bug
         * it grew by about one a lap. */
        {
            long off1, off4;
            /* 54.17 musical ticks a second, four timer interrupts each. */
            for (ticks = 0; ticks < 60L * 217; ticks++) mmd2_tick(&m);
            off1 = m.tr[1].notes - m.tr[0].notes * 2;
            printf("  a minute in:    track 0 %4ld notes, track 1 %4ld "
                   "(off by %ld)\n", m.tr[0].notes, m.tr[1].notes, off1);
            for (ticks = 0; ticks < 180L * 217; ticks++) mmd2_tick(&m);
            off4 = m.tr[1].notes - m.tr[0].notes * 2;
            printf("  four minutes:   track 0 %4ld notes, track 1 %4ld "
                   "(off by %ld)\n", m.tr[0].notes, m.tr[1].notes, off4);
            ok(m.tr[0].notes > 100, "the whole-note track kept playing");
            ok(off1 >= -1 && off1 <= 1, "in phase after a minute");
            ok(off4 >= -1 && off4 <= 1, "and still in phase after four");
        }
    }

    /* The six real songs: six tracks each, and each one makes a noise. */
    for (k = 0; k < 6; k++) {
        char path[64];
        unsigned char *bgm;
        long bgmLen;
        int tracks;
        double r;

        snprintf(path, sizeof path, "disk/SBPBGM%d.BGM", k);
        bgm = slurp(path, &bgmLen);
        if (!bgm) { printf("FAIL cannot read %s\n", path); fails++; continue; }
        tracks = mmd2_play(&m, bgm, bgmLen, voi, voiLen);
        ok(tracks == MMD2_TRACKS, "the song has six tracks");
        run(&m, buf, RATE * 4);
        r = rms(buf, RATE * 4);
        ok(r > 100, "the song plays");
        printf("  SBPBGM%d.BGM %5ld bytes, %d tracks, rms %6.0f%s\n", k,
               bgmLen, tracks, r, m.playing ? "" : " (ended)");
        free(bgm);
    }

    free(voi);
    if (fails) printf("%d checks failed\n", fails);
    else printf("all sound checks passed\n");
    return fails ? 1 : 0;
}
