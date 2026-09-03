# 引継ぎ（2026-09-03 夜の時点）

家の Claude へ。この節を最初に読めば続きから入れる。以下は全部 push 済み。
https://github.com/yomei-o/soko_ban_wasm — Pages で動く:
**https://yomei-o.github.io/soko_ban_wasm/**

`index.html` はバージョンを付けられないので、ブラウザ確認は **Ctrl+Shift+R**。
音は最初の操作か「音を出す」ボタンで始まる（ブラウザが操作なしの再生を
許さないため）。

## いまの状態

**30 面すべて遊べて、絵も音も鳴っている。**

user 確認済み:

* 絵は全部合っている
* 起動ロゴは花王 → THINKING RABBIT の順（同時ではない）
* 歩数の表示は SCORE 窓の中
* 音の速さは正しい
* **曲の種類は合っている**（プレイ中 = BGM 2、クリア = BGM 4 に直したあと）
* **音が特定のチャンネルだけずれる。1 分ほど聴くとずれてくる** ← この日の最後に
  原因を見つけて直した（下記）。**要再確認**

## この日の最後に直したこと — チャンネルのずれ

`src/mmd2.c` の `step_code` で **ループ終了（コード 114）が 1 ティック
消費していた**。

原作の `0x076a` は素の `ret` で戻り、`0x063b` の `jmp 0x618` が**同じ
パスの中で次のコードを読む**。フェッチループから抜けるのは音（`0x063d`）と
休符（`0x068e`）だけで、それも戻り番地を `pop` するという形でやっている。

つまりループ終了はティックを食わない。食わせていたので、**1 周ごとに 1
ティック遅れ**、周回の速いトラックほど余分に遅れて、時間が経つほど
チャンネル間が開いていった。コード 252 のジャンプも同じ間違いだった。

検査を足した（`tmp/sound_check.exe`）:

```
トラック 0: loop { 96 ティックの音 }        1 周 = 1 音
トラック 1: loop { 48 ティックの音 }        1 周 = 1 音、周回は倍
```

周回数が倍違うので、ループ終了がティックを食うと片方だけ余分に遅れる。
1 分後と 4 分後に「トラック 1 の音数 == トラック 0 の 2 倍」を測り、
**ずれが増えないこと**を見る。

* 直った状態: 1 分 34/68（差 0）、4 分 136/272（差 0）
* バグを戻すと: 1 分 34/67（差 −1）、4 分 135/266（差 −4）と**増える**

2 音を 1 周に入れる形では両トラックが同じ回数ループしてしまい、同じだけ
遅れて**ずれが見えない**。それで気付けなかった。

## この日に user から指摘された読み違い（同じ癖に注意）

1. **起動ロゴを両方同時に出した。** `LOGO.CG` に花王と THINKING RABBIT が
   1 枚に並んでいるのでそう描いたが、根拠が無かった。実際は
   `FUN_23b0_03a9` / `03ef` が **x=216 から 208 画素幅の帯**を上へ送る作りで、
   右半分は置き場。`03ef` が 8000 バイトを x=432 → x=216 へ複写してから
   上げる。**2 つが同時に出ることはない**
2. **BGM 4 をプレイ中の曲にした。** `0x121b` の手前が
   `call FUN_1edb_3182 ; or ax,ax ; je`（4 を鳴らす側）で、
   `FUN_1edb_3182` は**ゴールに乗っていない荷物の数**を数える。0 になるのは
   解けたときだけなので **4 はクリア曲**、プレイ中は入るときの 2
3. **テンポを 600Hz と推測した。** 実際は `0x0202` の
   `レジスタ 0x26 = 0xf0` から `3993600/(1152×16) = 216.67Hz`、
   その 1/4 = 54.17 音楽ティック/秒
4. **Ghidra の逆コンパイルを信じすぎた。** `callf 14d7:001d` を生バイナリで
   走査したら呼び出しは 6 か所で、Ghidra は 4 か所しか拾っていなかった。
   **迷ったら生バイナリを正規表現で走査する**

## 次にやること（優先順）

1. **チャンネルのずれの再確認。** 直したはずだが user にまだ聴いてもらって
   いない
2. **音の残り。**
   * 範囲 9 / 10（コード 115..146）のビブラートと音程スライド。表の形は
     `.VOI` の +0x00 と +0x30 に「周期・符号付き 16 ビットの深さ」16 個ずつと
     分かっているが、`[si+0x0b]`〜`[si+0x0f]` を毎ティックどう効かせるかが
     未実装（`0x078f` / `0x07b8` を読む）
   * `0x08a8` のソフトエンベロープ（`[0xf6e]` / `[0xf6f]` を使う）
   * コード 253 の変調モード（`[si+0x15]` の下位 2 ビットで `0xd0c` の 4 種、
     `0x0b7a` / `0x0b84` / `0x0bae` / `0x0bcf`）
   * フェードアウト（AH=0x06）と効果音（AH=0x0c 読み込み / 0x0d 再生）。
     ゲームは効果音バンクを読み込んでいないので、どこから鳴らすのか未調査
3. **エンディング。** `FUN_1edb_40bc` が BGM 1 と `END1.CG` を出す。
   `FUN_1edb_042c` の `FUN_1edb_427c() == 0` が入口条件で、それが
   「全面クリアしたか」かどうかは未確認。`END2.CG` と `STAFF1/3/4.CG`
   （スタッフロール。PRODUCED by Hiroyuki Imabayashi / Kao Corporation、
   MUSIC by Kenzo Kumei / MICRO CABIN）もまだ出していない
4. **TRACE。** 手順の記録・再生。1 手 3 ビット（水平か・符号・押したか）で
   1 バイト 2 手、3500 バイト/面 × 32 面 = `TRACE.DAT` の 112000 と一致。
   `WINDOWS.CGM` に TRACE 窓（上下の矢印と START）がある
5. **OPTION と EDIT。** `WINDOWS.CGM` に窓がある（EDIT は STAGE / EDIT PLAY /
   SAVE LOAD / ORIGINAL LOAD / CLEAR BGM / EXIT、TOOL は 壁 / 点 / ラビ君 /
   荷物 と EXIT）。`usermen.dat` がユーザ面のファイル
6. **記録の保存。** いまは実行のあいだだけ。`SBPUSER.DAT` と `TRACE.DAT` に
   相当するものを localStorage に置く（lord_monarch の `index.html` が
   base64 でやっているのが参考になる）
7. モノクロモード。`SBP98.DOC` に「起動時に SHIFT を押しっぱなしで
   モノクロ」と書いてあり、`[0x90]` がその旗で 4 プレーンと 3 プレーンを
   切り替える

## 作業のしかた（守ること）

* **GUI を開かない。** PNG に描いて `Read` で見る
* **WebGL 禁止**（対象機に無い）。ソフトで描く
* **推測で埋めない。** 分からないところは binary を読み直すか、分からないと
  書く。この日は 4 回指摘された（上の節）
* **Ghidra を信じすぎない。** 逆コンパイルは far call を落とすし、タイムアウト
  する関数もある。番地が要るときは `out/sbp98.asm`（逆アセンブル）を見て、
  それでも足りなければ生バイナリを正規表現で走査する
* Bash のヒアドキュメントは**バックスラッシュを食う**。`\n` を含む C や JS を
  流し込むと `s.replace` が**黙って空振りする**。python のパッチは
  **scratchpad のファイルに書いてから走らせる**
* Windows のファイル名は大文字小文字を区別しない

## 検査（全部通ることを確認してからコミット）

```
sh tools/build.sh check          全部ビルドして検査して PNG を吐く
./tmp/game_check.exe             30 面のデータと規則、60 項目ほど
./tmp/sound_check.exe            音程・音量・ずれ・6 曲
./tmp/sound_check.exe wav 0 8    tmp/bgm0.wav に 8 秒
node tests/wasm_check.js         wasm を node で。画面遷移・曲番号・音の信号
```

node は PATH に無いので
`PATH="/c/prog/emsdk/emsdk/node/22.16.0_64bit/bin:$PATH"` を前に付ける。

## 解析の道具

```
python tools/lzh.py   orig/*.lzh orig       書庫を開く
python tools/fat12.py orig/*.fdi disk       ディスクを開く
python tools/men.py   disk/SBPMEN.DAT       30 面を絵にする
python tools/men.py   disk/SBPMEN.DAT --c   30 面を C の表にする
python tools/cg.py    disk/TITLE.CG t.png --pal 0
python tools/bgm.py   disk/SBPBGM0.BGM 0 --bars    .BGM を MML に起こす
python tools/solve.py disk/SBPMEN.DAT 1     面を解いて手順を出す
python tools/mmdis.py disk/MMD2.SYS map 0x78 0x264 音源ドライバを読む
python tools/strings.py tmp/SBP98.BIN
```

Ghidra の結果は `out/sbp98.c`（395 関数の逆コンパイル）と
`out/sbp98.asm`（18682 命令の逆アセンブル）。作り直すなら

```
export JAVA_HOME='C:\prog\ghidra\jdk-21.0.12.1+1'
cmd //c "C:\prog\ghidra\ghidra_12.1.3_PUBLIC\support\analyzeHeadless.bat" \
    ghidra_proj sbp -process SBP98.EXE -noanalysis \
    -scriptPath tools/ghidra -postScript DumpAsm.java out/sbp98.asm
```

## 形式は全部 docs にある

* [docs/format.md](docs/format.md) — ディスク、`SBPMEN.DAT` の 30 面、
  `.CG` / `.CGM` の展開、パレット、`FONT.CG`、表示寸法
* [docs/sound.md](docs/sound.md) — MMD2 ドライバ（`INT 0xD2` の API）、
  MML の全文法、音程表、`.VOI` の割り付け、テンポ、曲と場面の対応

## もう一つの移植

**ロードモナーク**が `C:\prog\claude2\lord_monarch_wasm` にあり、
そちらの `RESUME.md` に引継ぎがある。この日そちらも 8.5 倍速だったのを
直した（`[0x32d1]` に 8 を入れて垂直同期で減らす作り）。
