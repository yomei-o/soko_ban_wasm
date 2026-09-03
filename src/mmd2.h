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

typedef struct {
    int active;
    long p;                             /* +0x01, the position in the song */
    int count;                          /* +0x00, the remaining length */
    int len;                            /* +0x03, the current length */
    int fnum, high;                     /* +0x04, +0x05 */
    int octave;                         /* +0x06 */
    int vol;                            /* +0x07, 0..15 */
    int level;                          /* +0x08 */
    int gate;                           /* +0x09 */
    int flags;                          /* +0x0a: 0x40 tie, low 3 bits q */
    int noise;                          /* the SSG's noise period, or -1 */
    int voice;                          /* +0x14, the FM voice number */
    int mode;                           /* +0x15, the modulation mode */
    int loopCount;                      /* +0x1c */
    long loopBack;                      /* +0x1d */
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

/* Mix `samples` frames.  The caller runs mmd2_tick at MMD2_TICK_HZ. */
void mmd2_render(Mmd2 *m, short *out, int samples, int rate);

/* The tick rate the OPN timer is left at.  The driver writes register 0x27 =
 * 0x2a to enable and clear both timers but the period registers are set
 * outside the code Ghidra and tools/mmdis.py could reach, so this is the
 * port's choice: 600 Hz gives 150 musical ticks a second, which puts a
 * 12-tick sixteenth at 8 a second - a brisk but ordinary tempo. */
#define MMD2_TICK_HZ 600

extern const unsigned short mmd2Fnum[MMD2_NOTES];
extern const unsigned short mmd2Period[MMD2_NOTES];
extern const unsigned char mmd2Tl[16];

#endif
