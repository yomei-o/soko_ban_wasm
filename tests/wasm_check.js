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
const SCR = { BOOT: 0, TITLE: 1, SELECT: 2, PLAY: 3 };

let fails = 0;
function ok(cond, what) {
  if (!cond) { console.log('FAIL ' + what); fails++; }
}

SokoBan().then(M => {
  ok(M._soko_init() === 0, 'the embedded data loads');
  ok(M._soko_width() === W && M._soko_height() === H, '640x400');
  ok(M._soko_screen() === SCR.BOOT, 'it opens on the loading logo');

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
  // FUN_1edb_000e sets colour 0 to white before dropping LOGO.CG in, so the
  // loading screen is a white field with the Kao crescent near the bottom.
  ok(h.size === 2, 'the loading screen is two colours (' + h.size + ')');
  ok(h.get(0xffffff) > 200000, 'and mostly white');

  // Logo -> title -> select -> a stage.
  M._soko_key(KEY.ENTER);
  ok(M._soko_screen() === SCR.TITLE, 'a key leaves the logo');
  px = pixels();
  h = histogram(px);
  // TITLE.CG's ground is entry 9 of the wood palette, fd9.
  ok(h.get(0xffdd99) > 150000, 'the title sits on its cream ground');
  M._soko_key(KEY.ENTER);
  ok(M._soko_screen() === SCR.SELECT, 'and another leaves the title');
  px = pixels();
  h = histogram(px);
  // FUN_2329_000d fills the panel white and every cell is white until it is
  // cleared, so the grid is mostly white with black borders and numbers.
  ok(h.get(0xffffff) > 100000, 'the grid panel is white');

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

  // Walking and undo, through the wasm rather than the C.  A step starts a
  // slide and the original ignores input until it finishes, so each press is
  // followed by settling it.
  function press(k) { M._soko_key(k); M._soko_settle(); }

  press(KEY.LEFT);
  ok(M._soko_moves() === 1, 'a step counts');
  ok(M._soko_busy() === 0, 'and the slide has run out');
  press(KEY.RIGHT);
  press(KEY.RIGHT);
  ok(M._soko_moves() === 2, 'the wall refuses the third');
  press(KEY.UNDO);
  press(KEY.UNDO);
  ok(M._soko_moves() === 0, 'undo winds all the way back');
  ok(M._soko_won() === 0, 'not won');

  // A press during a slide is dropped, the way the original drops it.
  M._soko_key(KEY.LEFT);
  ok(M._soko_busy() === 1, 'the slide is running');
  M._soko_key(KEY.LEFT);
  ok(M._soko_moves() === 1, 'the second press was ignored');
  M._soko_settle();

  // Retry and escape.
  press(KEY.DOWN);
  press(KEY.RETRY);
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
  press(KEY.RIGHT);
  ok(M._soko_pushes() === 1, 'stage 24: one push');
  ok(M._soko_done() === before - 1, 'the box left its goal');
  press(KEY.UNDO);
  ok(M._soko_done() === before && M._soko_pushes() === 0, 'and came back');

  // The solver's answer for stage 1, played through the wasm.  43 moves,
  // inside the stage's own limit of 71.
  M._soko_play(1);
  const RUN = 'uuldllldlurrrrrddllulluldrrrdrrullllrrrulll';
  const K = { u: KEY.UP, r: KEY.RIGHT, d: KEY.DOWN, l: KEY.LEFT };
  for (const c of RUN) press(K[c]);
  ok(M._soko_won() === 1, 'stage 1 is cleared by the solved run');
  ok(M._soko_moves() === 43, '43 moves');
  ok(M._soko_moves() <= M._soko_target(), 'inside the limit of ' + M._soko_target());
  ok(M._soko_done() === M._soko_boxes(), 'every box is home');

  // Audio.  The page pulls int16 out of soko_audio; here it just has to be a
  // real signal, and the song has to be the one the original picks for the
  // screen: 0 on the title and the grid, 4 in a stage.
  function audio(frames) {
    const p = M._soko_audio(frames, 44100);
    const a = M.HEAP16.subarray(p >> 1, (p >> 1) + frames);
    let sum = 0;
    for (let i = 0; i < frames; i++) sum += a[i] * a[i];
    return Math.sqrt(sum / frames);
  }

  M._soko_play(1);
  ok(M._soko_song() === 4, 'a stage plays SBPBGM4');
  let r = audio(4096);
  for (let i = 0; i < 20 && r < 50; i++) r = audio(4096);
  ok(r > 50, 'the stage music makes a signal (rms ' + r.toFixed(0) + ')');

  M._soko_key(KEY.ESC);
  ok(M._soko_song() === 0, 'the grid plays SBPBGM0');
  r = audio(4096);
  for (let i = 0; i < 20 && r < 50; i++) r = audio(4096);
  ok(r > 50, 'the grid music makes a signal (rms ' + r.toFixed(0) + ')');

  // Turning it off keys everything off, but the FM release still has to ring
  // out - the driver only writes register 0x28, it does not mute the chip -
  // so what this checks is that it dies away.
  M._soko_music(-1);
  ok(M._soko_song() === -1, 'music off');
  const first = audio(4096);
  let last = first;
  for (let i = 0; i < 12; i++) last = audio(4096);
  ok(last < first * 0.2 || last < 5,
     'and it dies away (' + first.toFixed(0) + ' -> ' + last.toFixed(0) + ')');

  if (fails) { console.log(fails + ' checks failed'); process.exit(1); }
  console.log('wasm checks passed');
}).catch(e => { console.log('FAIL ' + e); process.exit(1); });
