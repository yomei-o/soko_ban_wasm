#!/bin/sh
# Build everything.  There is no make on this machine, so this is the thing
# that runs rather than a Makefile.
#
#   sh tools/build.sh          native tools and the wasm
#   sh tools/build.sh native   just the native side
#   sh tools/build.sh wasm     just soko.js / soko.wasm
#   sh tools/build.sh check    build and run every check, and shoot the PNGs
set -e
cd "$(dirname "$0")/.."

CC="sh tools/cc.sh -O2 -Wall -Wextra -std=c99 -Isrc"
CORE="src/cg.c src/men.c src/game.c src/gfx.c src/font.c src/app.c src/mmd2.c src/opn.c src/ssg.c"

mkdir -p tmp

what=${1:-all}

if [ "$what" = all ] || [ "$what" = native ] || [ "$what" = check ]; then
    echo "== native"
    $CC -o tmp/soko_shot.exe  src/main_shot.c src/png.c $CORE
    $CC -o tmp/game_check.exe tests/game_check.c src/men.c src/game.c \
        src/cg.c src/gfx.c
    $CC -o tmp/sound_check.exe tests/sound_check.c src/mmd2.c src/opn.c \
        src/ssg.c -lm
    $CC -o tmp/end_check.exe  tests/end_check.c $CORE
fi

if [ "$what" = all ] || [ "$what" = wasm ]; then
    sh tools/wasm.sh
fi

if [ "$what" = check ]; then
    echo "== checks"
    ./tmp/game_check.exe
    ./tmp/end_check.exe
    ./tmp/sound_check.exe
    echo "== shots"
    ./tmp/soko_shot.exe board 1 tmp/s01.png
    ./tmp/soko_shot.exe board 24 tmp/s24.png
    ./tmp/soko_shot.exe board 30 tmp/s30.png
    ./tmp/soko_shot.exe pic disk/TITLE.CG  tmp/title.png --pal 0
    ./tmp/soko_shot.exe pic disk/END1.CG   tmp/end1.png  --pal 2
    ./tmp/soko_shot.exe pic disk/CHR98N.CG tmp/chr.png   --pal 1
    ./tmp/soko_shot.exe screen title  tmp/scr_title.png
    ./tmp/soko_shot.exe screen select tmp/scr_select.png --pick 11
    ./tmp/soko_shot.exe screen 1      tmp/scr_play.png --press rrd
    ./tmp/soko_shot.exe screen end    tmp/scr_end.png --ticks 430
    if [ -f soko.js ]; then
        echo "== wasm checks"
        PATH="/c/prog/emsdk/emsdk/node/22.16.0_64bit/bin:$PATH"             node tests/wasm_check.js
    fi
fi

echo "-> tmp/"
