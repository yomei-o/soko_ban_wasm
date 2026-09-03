// Drive the wasm from node, so the browser build is checked without opening
// a browser.
//
//     node tests/wasm_check.js
//
// Run from the repo root, after tools/wasm.sh.
'use strict';
const path = require('path');
const SokoBan = require(path.join(process.cwd(), 'soko.js'));

const W = 640, H = 400;
const KEY = { UP: 0, RIGHT: 1, DOWN: 2, LEFT: 3, UNDO: 4, RETRY: 5, ESC: 6,
              ENTER: 7 };
const SCR = { TITLE: 0, SELECT: 1, PLAY: 2 };

let fails = 0;
function ok(cond, what) {
  if (!cond) { console.log('FAIL ' + what); fails++; }
}

SokoBan().then(M => {
  ok(M._soko_init() === 0, 'the embedded data loads');
  ok(M._soko_width() === W && M._soko_height() === H, '640x400');
  ok(M._soko_screen() === SCR.TITLE, 'it opens on the title');

  // A frame has to be real pixels, not a blank buffer.
  function pixels() {
    const p = M._soko_frame();
    return M.HEAPU8.subarray(p, p + W * H * 4);
  }
  function histogram(px) {
    const seen = new Map();
    for (let i = 0; i < px.length; i += 4) {
      const k = (px[i] << 16) | (px[i + 1] << 8) | px[i + 2];
      seen.set(k, (seen.get(k) || 0) + 1);
    }
    return seen;
  }

  let px = pixels();
  ok(px.length === W * H * 4, 'the frame is the right size');
  let h = histogram(px);
  ok(h.size > 4, 'the title has more than a handful of colours (' + h.size + ')');
  // TITLE.CG's ground is entry 9 of the wood palette, fd9.
  ok(h.get(0xffdd99) > 150000, 'the title sits on its cream ground');

  // Title -> select -> a stage.
  M._soko_key(KEY.ENTER);
  ok(M._soko_screen() === SCR.SELECT, 'a key leaves the title');
  pixels();

  // The grid, exactly as FUN_1edb_042c hit-tests it.
  M._soko_move(32 + 5, 40 + 5);
  ok(M._soko_pick() === 1, 'the top-left cell is stage 1');
  M._soko_move(32 + 5 * 96 + 5, 40 + 4 * 68 + 5);
  ok(M._soko_pick() === 30, 'the bottom-right cell is stage 30');
  M._soko_move(10, 10);
  ok(M._soko_pick() === 30, 'outside the panel changes nothing');

  M._soko_click(32 + 5, 40 + 5);
  ok(M._soko_screen() === SCR.PLAY, 'clicking a cell starts the stage');
  ok(M._soko_stage() === 1, 'and it is stage 1');
  ok(M._soko_target() === 71, 'stage 1 asks for 71 moves');
  ok(M._soko_boxes() === 4 && M._soko_done() === 0, 'four boxes, none home');
  ok(M._soko_moves() === 0, 'no moves yet');

  px = pixels();
  h = histogram(px);
  // FUN_1edb_109b fills the screen with colour 3 of the tile palette, fdb.
  ok(h.get(0xffddbb) > 100000, 'the board sits on colour 3 of the tile palette');
  ok(h.size > 6, 'the board is drawn (' + h.size + ' colours)');

  // Walking and undo, through the wasm rather than the C.
  M._soko_key(KEY.LEFT);
  ok(M._soko_moves() === 1, 'a step counts');
  M._soko_key(KEY.RIGHT);
  M._soko_key(KEY.RIGHT);
  ok(M._soko_moves() === 2, 'the wall refuses the third');
  M._soko_key(KEY.UNDO);
  M._soko_key(KEY.UNDO);
  ok(M._soko_moves() === 0, 'undo winds all the way back');
  ok(M._soko_won() === 0, 'not won');

  // Retry and escape.
  M._soko_key(KEY.DOWN);
  M._soko_key(KEY.RETRY);
  ok(M._soko_moves() === 0, 'retry resets the stage');
  M._soko_key(KEY.ESC);
  ok(M._soko_screen() === SCR.SELECT, 'escape goes back to the grid');

  // Every stage has to start and draw without falling over.
  for (let n = 1; n <= 30; n++) {
    M._soko_play(n);
    ok(M._soko_stage() === n, 'stage ' + n + ' starts');
    ok(M._soko_boxes() > 0, 'stage ' + n + ' has boxes');
    const q = pixels();
    let nonzero = 0;
    for (let i = 0; i < q.length; i += 4 * 97) if (q[i] | q[i + 1] | q[i + 2]) nonzero++;
    ok(nonzero > 100, 'stage ' + n + ' draws something');
  }

  // Stage 24 is the one where a single push moves a box off its goal.
  M._soko_play(24);
  const before = M._soko_done();
  M._soko_key(KEY.RIGHT);
  ok(M._soko_pushes() === 1, 'stage 24: one push');
  ok(M._soko_done() === before - 1, 'the box left its goal');
  M._soko_key(KEY.UNDO);
  ok(M._soko_done() === before && M._soko_pushes() === 0, 'and came back');

  if (fails) { console.log(fails + ' checks failed'); process.exit(1); }
  console.log('wasm checks passed');
}).catch(e => { console.log('FAIL ' + e); process.exit(1); });
