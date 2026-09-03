# 倉庫番 Select 30 — WASM 移植

Thinking Rabbit の PC-98 版『倉庫番 Select 30』を、実物のフロッピーから
解析して C で書き直し、ブラウザで動かす。

エミュレータではない。ディスクイメージを Ghidra と自作の道具で読み、
何をしている関数なのかを確かめてから、同じ振る舞いをする C を手で書く。
描画はソフトウェアラスタライズのみ（**WebGL は使わない**）。

同じやり方の前作: [lord_monarch_wasm](https://github.com/yomei-o/lord_monarch_wasm)

## いまの状態

**データは全部読めた。盤面が描けて動く。まだ画面遷移と音は無い。**

* [x] ディスクを開いた — FAT12、41 ファイル（`tools/fat12.py`）
* [x] 実行形式 — MZ、ラージモデルの Borland C、DS = 0x19ca。
      Ghidra 12.1.3 で解析済み（`out/sbp98.c` 395 関数、`out/sbp98.asm` 18682 命令）
* [x] **30 面の盤面** — `SBPMEN.DAT`（`tools/men.py`、30 面 0 不整合）
* [x] **`.CG` / `.CGM` の展開** — 実行形式の 2406:09xx を読んで確定
* [x] **パレット** — DS:02a0 / 02d0 / 0300
* [x] **タイルの格子** — 40px と 32px、CHR98N.CG の y=148/188 と 84/116
* [x] 盤面の描画と規則（歩く・押す・undo・勝ち判定）+ 検査 60 項目
* [ ] 画面遷移（タイトル → SELECT → GAME、`WINDOWS.CGM` の 6 窓）
* [ ] `.BGM` と `.VOI`
* [ ] WASM 化と Pages

詳細は [docs/format.md](docs/format.md)。

```
sh tools/build.sh check          全部ビルドして検査して PNG を吐く
./tmp/soko_shot.exe board 1 tmp/s1.png --press rrdd
./tmp/soko_shot.exe pic disk/TITLE.CG tmp/title.png --pal 0
```

## 道具

```
python tools/lzh.py   orig/*.lzh orig       書庫を開く
python tools/fat12.py orig/*.fdi disk       ディスクを開く
python tools/men.py   disk/SBPMEN.DAT       30 面を絵にする
python tools/men.py   disk/SBPMEN.DAT --c   30 面を C の表にする
python tools/strings.py tmp/SBP98.BIN       文字列（16bit コードの雑音を除く）
python tools/cg.py disk/TITLE.CG t.png --pal 0
```

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
