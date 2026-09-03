/* The MMD2 music driver, as MMD2.SYS plays a .BGM.
 *
 * The whole grammar and every table address is written out in docs/sound.md;
 * the short version:
 *
 *   * A .BGM is six tracks, each ended by 0x00.  The first three drive the
 *     YM2203's FM channels and the last three its SSG channels
 *     (FUN 0x0411 calls FUN 0x0460 twice, and the tick calls the interpreter
 *     with 0, 1, 2 then 0x80, 0x81, 0x82).
 *   * A track is a byte stream.  Codes are 1-based and 0x0cca's nineteen
 *     (count, handler) triples carve 1..255 into ranges - notes, rests,
 *     octave, volume, length, and so on.
 *   * The tick runs off the OPN timer.  Each interrupt adds 0x40 to a byte
 *     counter and the note lengths only advance when it wraps, so a musical
 *     tick is four interrupts.
 *   * An FM voice is 25 bytes: byte 0 to register 0xb0 + ch, then 24 bytes to
 *     0x30 + ch stepping 4.  SBPVOICE.VOI holds 32 of them at +0x0e0, which
 *     is exactly the end of its 1024 bytes.
 *
 * This plays the stream by writing chip registers, the same way the driver
 * does, so opn.c and ssg.c see what a real YM2203 would.
 */
#ifndef SOKO_MMD2_H
#define SOKO_MMD2_H

#include "opn.h"
#include "ssg.h"

#define MMD2_TRACKS 6
#define MMD2_FM 3                       /* the first three are FM */
#define MMD2_NOTES 36                   /* twelve semitones in three steps */
#define MMD2_VOICES 32
#define MMD2_VOICE_BYTES 25

/* SBPVOICE.VOI's four sections, from the pointers at 0x0db4..0x0dba. */
#define VOI_VIB 0x000                   /* 16 x 3, range 9  (codes 115..130) */
#define VOI_SLIDE 0x030                 /* 16 x 3, range 10 (codes 131..146) */
#define VOI_MODE 0x060                  /* 32 x 4, code 253 */
#define VOI_FM 0x0e0                    /* 32 x 25, the FM voices */

/* 1edb:2c10's tempo: 0x40 added to a byte per interrupt, so four interrupts
 * to a musical tick. */
#define MMD2_TEMPO_ADD 0x40

/* The interrupt rate is not a guess: MMD2.SYS 0x0202 writes
 *
 *     register 0x26 = 0xf0        timer B's period
 *     register 0x27 = 0x3a        load timer B, enable its interrupt
 *
 * and a YM2203's timer B runs at clock / (1152 * (256 - N)), so
 * 3993600 / (1152 * 16) = 216.67 Hz.  With four interrupts to a musical tick
 * that is 54.17 ticks a second, and since a whole note is 96 ticks (a length
 * byte of `fe 60`) a crotchet lands at 135 BPM.
 *
 * The first pass at this guessed 600 Hz, which played everything at nearly
 * three times speed. */
#define MMD2_TIMER_B 0xf0
#define MMD2_CLOCK 3993600L
#define MMD2_TIMER_DIV (1152L * (256 - MMD2_TIMER_B))

/* How many loop frames a track can nest.  1edb... MMD2.SYS 0x0747 shifts a
 * twelve-byte area by three to make room, so four. */
#define MMD2_LOOPS 4

typedef struct {
    int active;
    long p;                             /* +0x01, the position in the song */
    int count;                          /* +0x00, the remaining length */
    int len;                            /* +0x03, the current length */
    int pitch;                          /* +0x04 as one word: the FM F-number
                                         * or the SSG period */
    int high;                           /* +0x05's spare bits (SSG noise) */
    int octave;                         /* +0x06 */
    int vol;                            /* +0x07, 0..15 */
    int level;                          /* +0x08 */
    int gate;                           /* +0x09 */
    int flags;                          /* +0x0a: 0x40 tie, low 3 bits q */
    int noise;                          /* the SSG's noise period, or -1 */

    /* Range 9 (codes 115..130), the level envelope.  0x078f copies a 3-byte
     * preset out of .VOI+0x00 into +0x0b..+0x0e and saves the level in +0x0f,
     * and 0x05a5 walks it: every `period` ticks the level moves by `delta`,
     * stopping at `limit`.  Because +0x08 is the level - the volume codes put
     * 0xda4's table value there - this is a software envelope, not a vibrato. */
    int lvlCount, lvlPeriod, lvlLimit, lvlDelta, lvlSaved;

    /* Range 10 (codes 131..146), the pitch slide.  0x07b8 copies its preset
     * into +0x10..+0x13 and 0x05e6 walks it: every `period` ticks the pitch
     * word gains a signed 16-bit `delta`, with no limit, until the next note
     * clears the counter at 0x0603. */
    int sldCount, sldPeriod, sldDelta;

    int voice;                          /* +0x14, the FM voice number */
    int mode;                           /* +0x15, the modulation mode */
    /* +0x1c: a stack of (count, pointer).  Code 98 + n starts a loop that
     * runs n times, with 0 meaning for ever, and code 114 ends one - which is
     * how every song loops: each track opens with 0x62 (98, count 0) and
     * closes with 0x72 0x00 (114 then the terminator). */
    int loopCount[MMD2_LOOPS];
    long loopBack[MMD2_LOOPS];
    int loopTop;
    long notes;                         /* note-ons, for the drift check */
} Mmd2Track;

typedef struct {
    Opn opn;
    Ssg ssg;
    const unsigned char *song;
    long songLen;
    const unsigned char *voi;
    long voiLen;
    Mmd2Track tr[MMD2_TRACKS];
    int mixer;                          /* 0x0de8, the shadow of SSG reg 7 */
    int tickAcc;                        /* 0x0de6 */
    int playing;
    long ticks;
} Mmd2;

void mmd2_reset(Mmd2 *m);

/* Point the player at a song and a voice bank.  Neither is copied, so both
 * have to outlive the player.  Returns the number of tracks found. */
int mmd2_play(Mmd2 *m, const unsigned char *song, long songLen,
              const unsigned char *voi, long voiLen);

void mmd2_stop(Mmd2 *m);

/* One OPN timer interrupt. */
void mmd2_tick(Mmd2 *m);

/* Mix `samples` frames without ticking.  Use mmd2_run unless the ticking is
 * being driven from outside. */
void mmd2_render(Mmd2 *m, short *out, int samples, int rate);

/* Tick at the timer's real rate and render, keeping the fraction of a sample
 * between calls in `acc`. */
void mmd2_run(Mmd2 *m, short *out, long frames, int rate, long *acc);

extern const unsigned short mmd2Fnum[MMD2_NOTES];
extern const unsigned short mmd2Period[MMD2_NOTES];
extern const unsigned char mmd2Tl[16];

#endif
