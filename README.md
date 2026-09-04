# 倉庫番 Select 30 — WASM 移植

Thinking Rabbit の PC-98 版『倉庫番 Select 30』を、実物のフロッピーから
解析して C で書き直し、ブラウザで動かす。

エミュレータではない。ディスクイメージを Ghidra と自作の道具で読み、
何をしている関数なのかを確かめてから、同じ振る舞いをする C を手で書く。
描画はソフトウェアラスタライズのみ（**WebGL は使わない**）。

**遊ぶ: https://yomei-o.github.io/soko_ban_wasm/**

同じやり方の前作: [lord_monarch_wasm](https://github.com/yomei-o/lord_monarch_wasm)

## いまの状態 — 完成

**30 面ぜんぶ遊べて、絵も音もエンディングも原作どおり。記録も残る。**
残っているのは、原作の流れにそもそも無いもの（TRACE / OPTION / EDIT）と、
入れない判断をしたもの（モノクロモード）だけ。

* [x] ディスクを開いた — FAT12、41 ファイル（`tools/fat12.py`）
* [x] 実行形式 — MZ、ラージモデルの Borland C、DS = 0x19ca。
      Ghidra 12.1.3 で解析（`out/sbp98.c` 395 関数中 387 本、`out/sbp98.asm` 18682 命令。
      `out/` は clone に入らないので RESUME の手順で作り直す）。
      Ghidra を出さずに済む所は `tools/exedis.py` で読める
* [x] **30 面の盤面** — `SBPMEN.DAT`（`tools/men.py`、30 面 0 不整合）
* [x] **`.CG` / `.CGM` の展開** — 実行形式の 2406:09xx を読んで確定
* [x] **パレット** — DS:02a0 / 02d0 / 0300、成分は R G B
* [x] **プレーンとビットの対応** — 教科書どおりではない。原作が色を塗るときに
      GRCG へ流す 16×4 の表（`CS:0x1c2b`）が、**ビット 1 を b800、ビット 2 を
      b000** に乗せている。これを直して床が地の色と揃い、NIVEA の缶が青に、
      ゴールに乗った荷物の**花王のマークが緑**になった
      （[docs/format.md](docs/format.md) の「プレーンとビットの対応」）
* [x] **タイルの格子** — 40px と 32px、CHR98N.CG の y=148/188 と 84/116
* [x] 盤面の描画と規則（歩く・押す・undo・勝ち判定）+ 検査 60 項目
* [x] 画面遷移 — タイトル → 面選択（6×5 の格子は原作の当たり判定そのまま）→ 盤面。
      **面選択と盤面は「散らし」で入れ替わる**（`23b0:0019`。原作は裏ページに
      描いてから 21 段で写す）
* [x] クリア／失敗の演出 — `CLEAR.CG` / `PEKE.CG` を 21 段でディゾルブ
      （`FUN_23b0_0440`）
* [x] WASM 化と Pages。`tests/wasm_check.js` が node で叩く
* [x] **音**。MMD2 ドライバと MML を読んで演奏器を書いた
      （[docs/sound.md](docs/sound.md)）。YM2203 の FM3 + SSG3、6 曲。
      ブラウザでは「音を出す」ボタンか最初の操作で鳴る
* [x] **画面が変わるときのフェードアウト** — 原作は曲を止める手段をこれしか
      持っていない。AH=0x06 で約 2.8 秒かけて落とし、**ドライバが止まるまで
      ゲームごと待つ**（前の画面が出たまま）
* [x] **エンディング** — 30 面全部クリアすると面選択の輪の中から始まり、
      二度と戻らない。`END1` → スタッフロール → `END2` → SPECIAL THANKS →
      COPYRIGHT を 3 種類の散らし（重ねる・置き換える・黒へ消す）で
      入れ替える無限ループ。BGM 1
* [-] TRACE と OPTION / EDIT は**製品の流れに無い**。`trace.dat` を開くのは
      記録の保存の 1 か所だけで読む所が無く、OPTION / EDIT の窓は
      `WINDOWS.CGM` にある未使用の絵（main と面選択の輪にどこからも出て
      こない）。詳しくは RESUME
* [x] **記録の保存** — `SBPUSER.DAT` の 310 バイト（10 バイト × 31、記録は
      各レコードの先頭ワード）をそのまま localStorage に置く。原作が
      「前回:%05d 今回:%05d steps でした、今回の手順を保存しますか？」と
      訊く所だけは出せない（PC-98 のテキスト画面＋本体のフォント ROM が要る）
      ので、常に「はい」の側で動く
* [-] モノクロ（8 色）モードは入れていない（描画の道が 2 本になる）。
      `FONT.CG` の字形も使う数字と英字だけ

詳細は [docs/format.md](docs/format.md) と [docs/sound.md](docs/sound.md)。
作業の引継ぎは [RESUME.md](RESUME.md)。

```
sh tools/build.sh check          全部ビルドして検査して PNG を吐く
sh tools/build.sh wasm           soko.js / soko.wasm だけ
./tmp/soko_shot.exe screen select tmp/sel.png --pick 11
./tmp/soko_shot.exe screen 1 tmp/play.png --press rrdd
./tmp/soko_shot.exe screen end tmp/end.png --ticks 430
./tmp/end_check.exe              エンディングの順番を検定する
./tmp/soko_shot.exe pic disk/TITLE.CG tmp/title.png --pal 0
node tests/wasm_check.js
./tmp/sound_check.exe            音程・音量・6 曲を検定する
./tmp/sound_check.exe wav 0 8    tmp/bgm0.wav に 8 秒書き出す
```

操作は矢印キーか WASD、`Z` で戻す、`R` でやり直し、`Esc` で面選択。
盤面はクリックでも歩く。記録はブラウザに残るので、続きから遊べる
（30 面ぜんぶクリアするとエンディングが始まり、そこからは戻れない ——
これは原作どおり）。

## 道具

```
python tools/lzh.py   orig/*.lzh orig       書庫を開く
python tools/fat12.py orig/*.fdi disk       ディスクを開く
python tools/men.py   disk/SBPMEN.DAT       30 面を絵にする
python tools/men.py   disk/SBPMEN.DAT --c   30 面を C の表にする
python tools/strings.py tmp/SBP98.BIN       文字列（16bit コードの雑音を除く）
python tools/cg.py disk/TITLE.CG t.png --pal 0
python tools/solve.py disk/SBPMEN.DAT 1    面を解いて手順を出す
python tools/mmdis.py disk/MMD2.SYS map 0x78 0x264   音源ドライバを読む
python tools/exedis.py fn 1edb:40bc 0x80    本体を Ghidra 無しで逆アセンブル
python tools/exedis.py calls 24d7:001d      そこを呼ぶ far call を全部
```

`tools/cgpy.py` は絵を python から測るための読み取り器（`import cgpy`)。
「この画素は何番の色か」を screenshot でなく数値で確かめるのに使う。

面 11 は例えばこう出る:

```
11  15x12 tile=2 start=(4,10) boxes=15 moves=1389
    |###############|
    |###$#      ####|
    |##$$# .  . #  #|
    |#$$$# ## . #  #|
    |#$$$$$  #..   #|
    |##$$$$.    #. #|
    |#### #######  #|
    |#   .        ##|
    |#  . #  .# . ##|
    |# .### . # .. #|
    |#   @#  ##    #|
    |###############|
```

## 決まりごと

* **GUI の窓を開かない。** 確認は PNG に描いて読む
* **WebGL 禁止**（対象機に無い）
* Bash のヒアドキュメントはバックスラッシュを食う。`\n` を含むものは
  Write/Edit で書く
* `orig/` は入れない（書庫とディスクイメージ）。`disk/` の中身は入れる
