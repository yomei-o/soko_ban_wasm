#!/bin/sh
# The wasm build, kept separate because emcc needs its own PATH entry.
set -e
cd "$(dirname "$0")/.."
EMSDK=C:/prog/emsdk/emsdk
PATH="/c/prog/emsdk/emsdk/node/22.16.0_64bit/bin:$PATH"
export PATH

CORE="src/cg.c src/men.c src/game.c src/gfx.c src/font.c src/app.c"

# Only the files the port actually reads are embedded, so the page stays small.
EMB=""
for f in TITLE.CG SELECT.CG CHR98N.CG WINDOWS.CGM LOGO.CG FONT.CG SBPMEN.DAT; do
    EMB="$EMB --embed-file disk/$f@/disk/$f"
done

"$EMSDK/upstream/emscripten/emcc.exe" -O2 -std=c99 -Isrc -o soko.js \
    src/main_wasm.c $CORE $EMB \
    -s MODULARIZE=1 -s EXPORT_NAME=SokoBan \
    -s EXPORTED_RUNTIME_METHODS=ccall,cwrap,HEAPU8,HEAPU32 \
    -s ALLOW_MEMORY_GROWTH=1 -s ENVIRONMENT=web,worker,node

ls -la soko.js soko.wasm
