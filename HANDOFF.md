# tmt 引き継ぎ (2026-11-20 / 版 1.1.29 = kInkVer 139)

新しいチャットの最初に、この HANDOFF.md を読んでから作業を続けてください。

## プロジェクト概要
- **tmt**：Splatoon 3 の exlaunch MOD（subsdk9 として注入）。廃止された Inkdraw v706 を土台に改造中。
- **対象ゲーム版**：12.0.0、main ビルドID `6cbb1669`、SDK `6A0A2600`。
- **マウント名**：`tmt`（pchtxt は `SD:/tmt/`）。
- **ユーザーは日本語**。返答は日本語。ユーザーはホストでプラベ（プライベートマッチ）を触っている。

## リポジトリ / 作業ディレクトリ
- GitHub: `https://github.com/tomatoo0609/tmt`（毎版 `main` に push 済み。push まで必ずやる）。
- 作業ルート: `/home/claude/tmt`
- **主要ソース**: `build_normal/source/program/main.cpp`（約1.2万行。ほぼ全ロジックここ）。

## ビルド & 配布の手順（毎回これ）
```
cd /home/claude/tmt/build_normal
bash q2.sh     2>&1 | grep -E "error"   # 通常版 → subsdk9
bash q2_en.sh  2>&1 | grep -E "error"   # 英語版 → subsdk9_en
cd /home/claude/tmt
python3 tools/make_update.py <VER> build_normal/subsdk9 build_normal/subsdk9_en
git add -A update/ build_normal/source/program/main.cpp
git commit -m "..."   # 末尾に Co-Authored-By と Claude-Session 行
git push origin main
```
- `q2.sh`：clang++ クロス + `ld.lld` + `../tc/elf2nso`。devkitPro ではない。
- ネットは allowlist（pip/npm/GitHub のみ）。pip は `--break-system-packages`。
- バージョン番号は main.cpp の `constexpr int kInkVer = N;`（1行）。更新確認もこの番号。
- `update/version.txt` は `tmt <ver> <size> <crc32>` と `tmt_en ...` の2行（make_update.py が生成）。
- 更新は `raw.githubusercontent.com/tomatoo0609/tmt/main/update` から。エラーコード `rc = 30 + HTTP/100`。

## ビルド上の既知の落とし穴
- **無名名前空間の一致**：関数の宣言を `namespace { }` 内に置いたら定義も同じ無名名前空間に。ズレると `@plt` 未解決 → 実行時に PC=0 の命令アボートで落ちる（v118→v119 で踏んだ）。
- `--shared --export-dynamic` なので未定義シンボルでもリンクは通る。注意。
- メニューのカテゴリ行配列 `kCatItems[kCatCount][20]`：各行は必ず `-1` 終端（末尾まで埋まる行は全要素が有効扱い）。幅を変えたら `CatCount` の `n < 20` も合わせる。

## クラッシュログの読み方（毎回使う）
- Atmosphère のクラッシュレポート。`PC`/`LR` が `tmt + 0xXXXX` なら自分のコード。
- tmt 内オフセットの関数特定：`cd build_normal && llvm-objdump -d tmt.elf | grep ...`、または
  `llvm-objdump -d --start-address=0xSTART --stop-address=0xEND tmt.elf`。
- Type: Data Abort = メモリ参照、Instruction Abort + PC=0 = null 関数ポインタ呼び出し。

## ゲーム本体の解析資材（/mnt/user-data/uploads にある。新チャットでは再アップが必要かも）
- `main`：12.0.0 の NSO（本体）。`/home/claude/work/new.img` に展開済み（`tools/nso_decomp.py`）。
  - テキスト終端 ~`0x4E9C410`。IDA のアドレス = このイメージのアドレス。
- IDA 逆コンパイル: `*-team5_funcs.txt`, `*-team6/7/8_funcs.txt`, `*-team5/6/7/8_notes.txt`。
- 逆アセンブル補助（work ディレクトリ）: `fdis.py`（単発）, `xref.py`/`xrange.py`/`gotref.py`（参照探し。numpy 使用）。
- IDA スクリプトは `tools/ida/tmt_ida6.py ... tmt_ida8.py`（ユーザーが main.elf.i64 で実行して出力をくれる）。

## この版までで入れた機能（1.1.14〜1.1.20）
- v126 **チーム10人**：決定前のみ。P1〜P10（`kDynTeamP0`=530〜539）。
- v127 **名前プリセット**：8枠を `tmt:/tmt/namepre.txt` に保存。`kDynNamePre`(509 選択/適用)・`kDynNamePreSave`(510 編集)。キーボード target 2 = プリセット編集。
- v128 **スプラッシュ白基調**：`SplashDraw`(main.cpp ~10760) の配色を白へ。
- v129 **メニュー縦型 第1案**：`rowH` 38→50、スライダーを名前の下（`y + rowH - 14`）に全幅、2つ目メニュー幅 400→336。**実機スクショで要調整**。
- v130 **スプラッシュにローポリの鳥+葉**：`SplBird`/`SplTri`/`SplSeg`/`SplLeaf`（main.cpp の `Blend` 直後, ~10128）。白黒。鳥は右、砕片が右上へ、葉は右上と左下。

- v131 **白基調メニュー + 飾り**：`WC()`(暗色→白用の色写像)・`kAcc`(深い青緑) で白基調化（ネオン=`SwTopLine`は不変）。最上位メニューの行高は `rowT`=34（他は `rowC`=50）。
  飾りは新規 `build_normal/source/program/deco.hpp`（`Cv/Pack/Blend` もここへ移動。**ストレートアルファ**で透明=alpha0）。ローポリの鳥 `DecoBird`/葉 `DecoLeaf`/枝 `DecoSprig`/バラ `DecoRose`。
  スプラッシュ: 板の周囲に余白 `kSplPad`=64、右に下地+鳥、右上/左下に枝と葉（枠の外へはみ出す）。メニュー: 余白 `kMDL/T/R/B`、右上に鳥、左下にバラ(L字)。
  枠外の飾りは矩形コピーでは出せないので、`SprRuns` で透けていない画素を横の連なりにして `copyB2T`（SwCopy の rect 後、スプラッシュは copy2 の後）。GPU(GxOn)/半透明モードでは飾りは出ない(安全側)。
  半透明の濃さ判定は `InkLv`(暗い画素=濃く) に変更（白基調のため）。`g_Base` は 1100x900 に拡大。
  **v131 は起動直後に SDK abort で落ちた**(BSS が +3.7MB 増えたのが疑い)。v132 で飾りのスプライトは malloc(`DecoMem`)、`g_Base` は 1000x640 に戻し、余白が入らない大きさのときは飾りなし(`dm`)。BSS は 26.0MB。
  v133: 鳥・バラは白黒(`decoc::k*`)、つまみ色 `kAcc`=キャラメル、メニュー余白 L/T=0 (左上ぴったり)・R=36(鳥は未展開時のみ)・B=20、`gap`=6。鳥は右上の角、バラは左下の角(板の下へはみ出す)。
  v134: 鳥・葉は純粋な黒と白だけ(灰色なし)、面を少なく(`DecoFacet`)。葉は4面のひし形で白黒の割り振りがたねで変わる。メニューのバラは廃止(鳥のみ)。
  v135: 起動画面を横長に(`BH0`=130)、下地の線を左へ向かって薄く(Ver 表示の右で消える)。メニューの鳥は廃止。濃さ初期値 80%。半透明の背景は常に最粗(`g_BlLod`=3、横4画素共有)。`kAcc` を濃いキャラメルに。
  v139: ジャイロ左右の kS(近いほど弱める 0.55〜1)を 1.0 固定にして直前の減速をなくした。**実機未確認**
  v138: ジャイロ版エイムボット(GaTick): 上下の角速度上限 wpc 4.8→18、rpc 固定6000(1フレーム最大100度)。左右は符号反転(行き過ぎ)を検出して kG を0.6へ下げる。**実機未確認**
  v137: エイムボット(スティック版 LookTickBody): 敵位置を指数平滑(0.4)、左右は 5.7度以上ずれたら制限なし1フレームで向く(確かめ済みの場所のみ)、近いずれは gain 1.0。展開メニューの行高 rowC 50→40。**実機未確認**
  v136: 起動画面の板は固定高さ(`BH0`=160, 伸びない)。ロゴ(上)・バー(`ly`=86, 真ん中より下)・アップデート文字「最新です」等(バーの下)。葉は枝2本ずつに減らし黒多め。半透明背景は行間隔12/16・横8画素共有・4フレームに1回更新。
  **実機未確認**：メニューの白基調の見え方、飾りの位置/大きさ(`kMb*`/`kMr*`/`kMDx`)、スプラッシュの飾り。

## 未完 / 次やること（優先順）
1. **メニュー縦型の位置調整（最優先）**：v129 は第1案。ユーザーの**メニュー開いたスクショ**を見て、名前/値/つまみの重なり・幅・行高を詰める。スライダーは `SwBar(v, bx, y + rowH - 14, bw, ...)`、名前は `y + ty`。該当は main.cpp ~11124〜11160。
2. **チーム「4人以上同チーム」制限の回避（9対1）**：決定前の手動割り当てで通るかまずユーザーに確認。弾かれる/戻るなら、per-team の人数上限の強制箇所を IDA で特定して外す（`sub_32CB7B0` team build、`sub_3899340` 付近）。既存の `TeamCApply`（charlie/第3チーム）は `0x32CBF20`/`0x32CC030` の `cmp #2→#4` 等をパッチしている。
3. チーム決定後の変更：**不可能と判明**（`sub_38C8A28` のチーム分けオブジェクト `off_5F2DB40` が決定後は null → 診断 R=-3）。ユーザー合意で「決定前のみ」で確定。深追いは試合設定DB書き換えで高リスク。

## チーム変更の仕組み（重要なまとめ）
- 決定**前**：`sub_38C8A28(slot, team)`（ゲーム自身のUIボタン関数）が有効。内部で playerObj を解決し `SetTeam`(`sub_3F684A4`, +588 に書く) を呼びイベント発火。
- 呼び出しは **`DynInput`（入力処理）から直接**（FrameHook は決定後のシーンで回らないため）。ポインタ連鎖 `off_5F46808`/`off_5F2DB40`→+40→vtable を検証してから呼ぶ（main.cpp ~1281）。
- 決定**後**：権威は各プレイヤー +588（`sub_3F69188` が毎フレーム再適用）。だが divide オブジェクトが消えるので UI 経路では触れない。
- `v2[267..276]`（`off_5F3A780`→*+184）は決定前の編集用。決定後は読まれない。

## 浮遊（フリーフライを他人に見せる）＝ 断念済み
- 送信側だけでは不可能と確定（v117 で関連コード全削除）。受信側（相手機）が相手アバターを自前の物理で描き、送った座標を地面に拘束する。スライドイベント(`SideStepStart` 126)だけは行動として直接適用されるが「スライドの動き」が出る。相手機は変えられない。

## 重要オフセット（12.0.0）
- `kTeamSetUiOff` = `0x38C8A28`（sub_38C8A28, 先頭命令 `0xA9BD7BFD`）
- SetTeam `0x3F684A4`（+588 書込）、再適用 `0x3F69188`、TeamChangeChild `0x390BDC0`
- team data: `off_5F3A780`（*+184 = v2、v2[267+i] が slot i のチーム）
- divide manager: `off_5F2DB40`（決定後 null）, seq `off_5F46808`, step `off_5F2C8A0`(+1224), gate `off_5F2DAA0`(+208)
- ロビー判定バイト `0x61056D8`、PaintRequest `0x37246F0`、FrameHook `0x373EB70`、BodyCalcHook `0x4A94230`/`0x2796...`
- pchtxt @nsobid は 12.0.0 の `6cbb1669` を動的に照合（`InitMainBuildId`, main.cpp ~816）。

## コミットの末尾（指示されている署名）
```
Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01ALtPd8K6or7o2FqD8Jquzk
```
（モデルが変わると署名も変わるので、その時のリマインダに従う）

## 新チャットでユーザーに最初に聞くとよいこと
- メニュー（v129/130）の実機スクショ。
- チームで 9対1 が通るか（決定前）。
- スプラッシュの鳥・葉の見た目の感想（調整するなら座標は `SplBird` の `F`/`FR`/`SP` 表）。
