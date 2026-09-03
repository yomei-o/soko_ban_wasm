#!/bin/sh
# Build everything.  There is no make on this machine, so this is the thing
# that runs rather than a Makefile.
#
#   sh tools/build.sh          native tools (and the wasm once it exists)
#   sh tools/build.sh native   just the native side
#   sh tools/build.sh check    build and run the checks
set -e
cd "$(dirname "$0")/.."

CC="sh tools/cc.sh -O2 -Wall -Wextra -std=c99 -Isrc"
CORE="src/cg.c src/men.c src/game.c src/gfx.c"

mkdir -p tmp

what=${1:-all}

if [ "$what" = all ] || [ "$what" = native ] || [ "$what" = check ]; then
    echo "== native"
    $CC -o tmp/soko_shot.exe  src/main_shot.c src/png.c $CORE
    $CC -o tmp/game_check.exe tests/game_check.c src/men.c src/game.c
fi

if [ "$what" = check ]; then
    echo "== checks"
    ./tmp/game_check.exe
    echo "== shots"
    ./tmp/soko_shot.exe board 1 tmp/s01.png
    ./tmp/soko_shot.exe board 24 tmp/s24.png
    ./tmp/soko_shot.exe board 30 tmp/s30.png
    ./tmp/soko_shot.exe pic disk/TITLE.CG  tmp/title.png --pal 0
    ./tmp/soko_shot.exe pic disk/END1.CG   tmp/end1.png  --pal 2
    ./tmp/soko_shot.exe pic disk/CHR98N.CG tmp/chr.png   --pal 1
fi

echo "-> tmp/"
