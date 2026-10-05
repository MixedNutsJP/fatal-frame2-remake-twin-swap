# TwinSwap

**FATAL FRAME II: Crimson Butterfly REMAKE** 用の Mod です。
A mod for FATAL FRAME / PROJECT ZERO II: Crimson Butterfly REMAKE.

Created by MixedNuts

**2.0.0 から MixedNutsModLoader（1.0.0 以降）が必要です。**
**2.0.0 requires MixedNutsModLoader (1.0.0 or later).**

Nexus Mods: https://www.nexusmods.com/fatalframe2crimsonbutterflyremake/mods/26
GitHub: https://github.com/MixedNutsJP/fatal-frame2-remake-mod-loader

---

# 日本語

## これは何か

操作キャラ（澪）と同行キャラ（繭）の**見た目を入れ替えます**。
繭を操作して澪を連れて歩く、二人とも繭、二人とも澪、の 3 通りを設定ファイルで選べます。
2.1.0 からは、**黒澤紗重・黒澤八重**の姿も選べます（例：紗重を操作して八重を連れて歩く）。
2.4.0 からは、**立花千歳**の姿も選べます。

入れ替わるのはモデル（顔・髪・体・衣装）だけです。動き・声・字幕・ストーリーは
元のままです（例えば、繭の足を引きずる歩き方は、澪の姿になっても同行キャラに残ります）。

2.0.0 は **MixedNutsModLoader のプラグイン**です。1.x は `xinput1_4.dll` で単体で
動いていましたが、2.0.0 からはローダーを別に導入する必要があります。

## 動作環境

- FATAL FRAME II: Crimson Butterfly REMAKE（Steam 版）
- **MixedNutsModLoader 1.0.0 以降**（別途導入が必要です）
  Nexus Mods: https://www.nexusmods.com/fatalframe2crimsonbutterflyremake/mods/26
  GitHub: https://github.com/MixedNutsJP/fatal-frame2-remake-mod-loader
  （Releases からダウンロードしてください）

ゲームのファイルは一切変更しないため、Steam のファイル整合性チェックに
引っかかることはありません。

## 同梱ファイル

| ファイル | 役割 |
|---|---|
| `MixedNuts\Mods\twinswap\twinswap.dll` | 本体 |
| `MixedNuts\Mods\twinswap\twinswap.ini` | 設定ファイル |
| `MixedNuts\Mods\twinswap\README.md` | このファイル |
| `MixedNuts\Mods\twinswap\LICENSE.txt` | ライセンス |

**`MixedNuts` フォルダ**をコピーします。ローダーの `MixedNuts` フォルダと中身が合流します。
起動すると `MixedNuts\Mods\twinswap\` の中にログ `twinswap.log` が作られます。
入れ替え用のデータは、ローダーが共有の `MixedNuts\cache\fdata_package\` に作ります。

### 他の Mod との併用

- **Native120FPSOption / MouseWheelCameraSpeed とは干渉しません。**
  この Mod（2.0.0）を含め、どれも同じローダーの上で動くので、DLL は 1 つを共有し、
  ぶつかることはありません
- **Yumia fdata tools で入れる Mod（衣装の改変など）と併用できます。** その時点の
  Mod 込みのデータを元に入れ替え用のデータを作るので、後から Mod を入れ直しても、
  次の起動で自動的に作り直されます
- 同じローダーの上で root.rdb / root.rdx を書き換える他の Mod とは、フォルダ名の順
  （アルファベット順）に順番に適用されます。この Mod は、それより前の Mod が作った結果を
  元にします
- ゲームのルートに **ローダー以外の `dinput8.dll` が既にある場合は、上書きしないでください。**
  ローダーの `dinput8.dll` は `version.dll` か `xinput1_4.dll` に名前を変えて使えます
  （ローダーの README を参照してください）

## 導入方法

1. ゲームを終了します

2. **MixedNutsModLoader を先に導入します。** 手順はローダーの README を参照してください

3. この Mod の `MixedNuts` フォルダを、ゲームのルートディレクトリ
   （`FatalFrameII.exe` と同じ場所）にそのままコピーします。
   ローダーの `MixedNuts` フォルダと中身が合流します

   ```
   ...\FatalFrameII\FatalFrameII.exe
   ...\FatalFrameII\dinput8.dll                                     ← ローダー
   ...\FatalFrameII\MixedNuts\MixedNutsLoader.dll                   ← ローダー
   ...\FatalFrameII\MixedNuts\Mods\twinswap\twinswap.dll            ← この Mod
   ...\FatalFrameII\MixedNuts\Mods\twinswap\twinswap.ini            ← この Mod
   ...\FatalFrameII\MixedNuts\Mods\twinswap\twinswap.log            ← 起動時に生成
   ...\FatalFrameII\MixedNuts\cache\fdata_package\root.rdb          ← ローダーが生成
   ...\FatalFrameII\MixedNuts\cache\fdata_package\root.rdx          ← ローダーが生成
   ...\FatalFrameII\MixedNuts\cache\fdata_package\0xfffe7510.fdata  ← ローダーが生成
   ```

   ゲームフォルダの開き方：Steam ライブラリでタイトルを右クリック →
   **管理** → **ローカルファイルを閲覧**

4. ゲームを起動し、セーブデータをロードしてください

### 1.x から更新する場合

導入の前に、ゲームのルートから古い **`xinput1_4.dll`** と **`Mods\twinswap\` フォルダ**
（`cache` を含む）を削除してください。1.x の `xinput1_4.dll` が残っていると、ローダーは
新しい Mod を読み込まず、`MixedNuts\loader.log` に `[!!]` で始まるメッセージを書きます。

Native120FPSOption（`dinput8.dll` + `Mods\native120fps\`）や MouseWheelCameraSpeed
（`version.dll` + `Mods\wheelspeed\`）も 1.x を入れている場合は、まとめて更新してください。
詳しくはローダーの README にあります。

`Main` / `Sub` の設定は、古い `twinswap.ini` から写してかまいません。

## 削除方法

`MixedNuts\Mods\twinswap\` フォルダを削除するだけです。ローダーは他の Mod のために
残しておいてかまいません。すべて外す場合は、ローダー（`dinput8.dll` と `MixedNuts`
フォルダ）も削除してください。
ゲームのファイルは一切変更していないため、完全に元に戻ります。

一時的に無効化したい場合は、`twinswap.ini` の `Enabled` を `0` にしてください。

## 設定ファイル

`twinswap.ini` で次の項目を変更できます。変更はゲームの再起動後に反映されます。

| 項目 | 意味 |
|---|---|
| `Main` | 操作キャラ（本編の澪）の見た目。`mio`（澪）/ `mayu`（繭）/ `sae`（紗重）/ `yae`（八重）/ `chitose`（千歳）。既定値 `mayu` |
| `Sub` | 同行キャラ（本編の繭）の見た目。値は `Main` と同じ。既定値 `mio` |
| `Rope` | 紗重・八重の赤い縄。`1` = 表示（既定）/ `0` = 非表示 |
| `Blindfold` | 澪の 2 着目（夏のカーディガン）の目隠し。`default` = ゲームのまま（既定）/ `show` = 常に表示 / `hide` = 常に非表示 |
| `ChitoseSkin` | 千歳の肌。`default` = ゲームのまま（幽霊の白い肌、既定）/ `human` = 生きている人の肌色 |
| `SaeYaeSkin` | 紗重・八重の肌。`default` = ゲームのまま（生前の肌色、既定）/ `ghost` = 幽霊の白い肌 |
| `Enabled` | `1` = 有効 / `0` = 無効 |
| `Log` | `1` = ログを出力 / `0` = 出力しない |

| `Main` | `Sub` | 結果 |
|---|---|---|
| `mayu` | `mio` | 姉妹を入れ替える（既定） |
| `mayu` | `mayu` | 二人とも繭 |
| `mio` | `mio` | 二人とも澪 |
| `mio` | `mayu` | 元のまま |
| `sae` | `yae` | 紗重を操作して八重を連れて歩く |
| `yae` | `sae` | 八重を操作して紗重を連れて歩く |
| `chitose` | `mayu` | 千歳を操作して繭を連れて歩く |

`sae` / `yae` / `chitose` は、`mio` / `mayu` と自由に組み合わせられます（例：`Main=mio` / `Sub=sae`）。
`mio` / `mayu` / `sae` / `yae` / `chitose` 以外の値を書いた場合は、そのキャラ本来の見た目のままになります。

## 衣装の対応

衣装は、衣装メニューの並び順で 1 対 1 に対応させています。例えば、澪に 2 着目の
衣装を着せると、操作キャラは繭の 2 着目の衣装の姿になります。

- **澪の 2 着目（夏のカーディガン）の目隠し**は、`Blindfold` で表示を選べます（2.3.0）。
  このモデルには白い目隠しが入っていて、普段は表示されません。姉妹を入れ替えてこの衣装を
  選ぶと、取り憑かれて敵として現れる場面で目隠しが表示されます。気になる場合は `hide` で
  消せます。`show` にすると、普段の操作中も目隠しを着けた姿になります。
  - 目隠しが入っているのは高精細モデルだけです。離れて見たときは、`show` でも表示されません
  - `hide` にすると、ゲームが本来目隠しを見せる場面でも表示されなくなります
  - 録画済みの動画で流れるムービーには反映されません
  - 入れ替えをしない設定（`Main=mio` / `Sub=mayu`）でも使えます
- **SILENT HILL f とのコラボ衣装・アイテムは対象外です。** 澪の 8 着目
  （ネイビーセーラー）は繭側に対になる衣装が無いため入れ替えず、選ぶと澪の姿のままになります
- アクセサリーは入れ替えの対象外です。入れ替えた姿にも、そのまま反映されます
- **紗重・八重は白い着物の 1 着だけです。** `sae` / `yae` を指定したキャラは、衣装メニューで
  どの衣装を選んでもその姿になります（澪の 8 着目を除く）
- **千歳も着物の 1 着だけです。** `chitose` を指定したキャラは、衣装メニューでどの衣装を
  選んでもその姿になります（澪の 8 着目を除く）

### ゲーム内での動作確認の範囲

ゲーム内で実際に確認できているのは、**初期衣装**と**和風ゴシックドレス**
（澪の左翅・繭の右翅）の 2 組だけです。`Main` / `Sub` の 3 通りの組み合わせ、
TAB メニュー、衣装画面、フォトモードを確認しています。

それ以外の衣装（2〜6 着目）は、同じ手順で入れ替えデータを作っていますが、
ゲーム内ではまだ確認していません。表示がおかしい衣装があれば、
GitHub の Issue で教えてください。

紗重・八重は、`Main=sae` / `Sub=yae` と `Main=yae` / `Sub=yae` で、歩いたときの表示
（赤い縄を含む）を確認しています。

千歳は、`Main=chitose` / `Sub=chitose` と `Main=chitose` / `Sub=mayu` で、歩く・手をつなぐ・
射影機を構える、を確認しています。

## 注意事項

- **衣装画面のプレビューも、入れ替えた後の姿で表示されます。** 衣装の名前と
  プレビューの姿が一致しないのは、この Mod の仕様です
- **ムービーにも入れ替えを反映するには、ゲームのオプションの「表示設定」→「ムービー中の衣装」を
  「現在の衣装」にしてください。**（英語表示では "Outfits During Movies" を "Current Outfit"）この設定では、ムービーがその場で描画されるので、入れ替えた
  姿で流れます（ED ムービーで確認）。「通常衣装」「DDX衣装」を選ぶと、その衣装で録画された
  動画が再生されるため、元の姿のまま流れます。「現在の衣装」でも、録画済みの動画しか無い
  ムービーがあれば、それは元の姿のままです
- 会話やイベントなど、もともとゲーム内で描画される場面には、設定にかかわらず入れ替えが反映されます
- **紗重・八重の赤い縄は、歩くと着物を少し突き抜けることがあります。** 紗重・八重本来の
  体の動きに合わせて作られた部品のためです。気になる場合は `Rope=0` で非表示にできます
- **既知の不具合：`Rope=0` にしても、カットシーンでは赤い縄が表示されます。** 縄が消えるのは
  操作中だけです。カットシーンでは、紗重・八重のモデル側の表示設定が使われるためと考えられます。
  カットシーンでも消すにはモデルのファイル自体を書き換える必要がありますが、そうすると
  イベントに登場する本来の紗重・八重の縄も一緒に消えてしまいます。入れ替えた姿だけ縄を消すのは、
  今の仕組みでは回避が難しいため、対応の予定は未定です
- **肌の色を選べます（2.4.0）。** `ChitoseSkin=human` で千歳を生きている人の肌色に、
  `SaeYaeSkin=ghost` で紗重・八重を幽霊の白い肌にできます。変わるのは顔と手足の色だけで、
  顔立ちは元のままです。千歳は、目の周りの隈と唇の色に元の灰色が少し残ります
- **既知の不具合：紗重・八重の着物の袖が、立ち止まっていてもなびき続けることがあります。**
  肌の色の設定によらず起き、原因は分かっていません
- 千歳の姿や、千歳・紗重・八重の肌の色を選んでも、本編に登場する千歳・紗重・八重には影響しません
  （Mod はモデルの写しを別に用意して使います）
- `sae` / `yae` を選び `Rope=1`、`SaeYaeSkin=default` のときは、イベントで登場する紗重・八重も、
  赤い縄が常に表示される状態になります。本来は縄の一部を出さない場面でも、縄が見えることがあります
- **千歳は双子より背が低く、モデルに足がありません。** 裾の下に何も描かれないのは、ゲームが
  持っている千歳のモデルの作りで、Mod の不具合ではありません
- 千歳の姿で手をつなぐと、つなぐ瞬間に一瞬だけ背が伸びて、すぐ戻ることがあります
- 2.1.0 では紗重と八重を取り違えていました。2.2.0 で直したので、`sae` / `yae` の見た目が
  2.1.0 とは逆になります（腰に縄を巻くだけの方が紗重、縄が長く垂れている方が八重）
- 入れ替え用のデータ（`MixedNuts\cache\fdata_package\`、最大で約 100 MB）は、ローダーが
  作ります。ゲームのファイル（Yumia fdata tools で入れた Mod を含む）、導入している Mod、
  その設定のいずれかが変わると、次の起動で自動的に作り直されます。
  それ以外の起動では、作ったものをそのまま使います。削除してもかまいません
  （次の起動で作り直されます）
- ゲームのアップデート後に動かなくなることがあります。その場合、Mod は何もせず
  ゲームは素の状態で動きます。ログに理由が記録されます
- セーブデータには何も書き込みません。Mod を外すと、元の姿に戻ります
- ウイルス対策ソフトが誤検知することがあります。ゲームがファイルを開く処理に
  割り込む仕組みのためです。この Mod はネットワーク通信を一切行わず、
  ファイルを書き込むのも `MixedNuts` フォルダの中だけです

## 免責事項

**この Mod は無保証で提供されます。使用によって生じたいかなる損害についても、
作者は一切の責任を負いません。** セーブデータの破損・消失、ゲームの動作不良、
その他の不具合を含みます。自己責任でご使用ください。

**導入前に、必ずセーブデータのバックアップを取ってください。**

```
%LOCALAPPDATA%\KoeiTecmo\FatalFrameII\Savedata\
```

## うまく動かないとき

1. ローダーが導入されているか。`dinput8.dll` がゲームのルート
   （`FatalFrameII.exe` と同じ場所）にあり（**`MixedNuts` フォルダの中ではありません**）、
   `MixedNuts\MixedNutsLoader.dll` があるか
2. `MixedNuts\loader.log` が生成され、`[OK] twinswap: loaded` の行があるか。
   `loader.log` が無ければ、ローダーが読み込まれていません。
   行が無ければ、フォルダ名・ファイル名が `MixedNuts\Mods\twinswap\twinswap.dll` の
   とおりか確認し、`loader.log` に `[!!]` や `[NG]` の行が無いか見てください
3. `MixedNuts\Mods\twinswap\` の中に `twinswap.ini` があるか
4. `MixedNuts\Mods\twinswap\` に `twinswap.log` が生成されているか

ログに次のような行が出ていれば正常に適用されています（ログは英語で出力されます）。

`twinswap.log`：

```
TwinSwap 2.4.0  Main=mayu Sub=mio Rope=1 Blindfold=default
[OK] Registered with the loader
[OK] Generated the swap data (...)
```

`MixedNuts\loader.log`：

```
[OK] twinswap: loaded (1 file patches)
[OK] File hook installed (...)
[OK] twinswap: Main=mayu Sub=mio Rope=1 Blindfold=default (3 files)
```

`[OK] Generated the swap data (...)` と `[OK] twinswap: Main=mayu Sub=mio Rope=1 Blindfold=default (3 files)` は、
入れ替え用のデータを作った起動でだけ出ます。2 回目以降の起動では、`loader.log` の
最後の行が `[OK] Using the cached files (N)` になります。

`Enabled=0` のときと、`Main=mio` / `Sub=mayu`（元のまま）で `Blindfold=default` のときは、
`[OK] Nothing to do (disabled, or Main=mio / Sub=mayu with Blindfold=default)` と出て、
何も登録されません。

不具合を報告するときは、GitHub の Issue で `twinswap.log` と `loader.log` の
2 つを添付してください。

https://github.com/MixedNutsJP/fatal-frame2-remake-twin-swap/issues

## 仕組み

ゲームのファイルは変更しません。

キャラの衣装は、ゲームのデータベース（kidsobjdb）の中で「どのモデルを使うか」を
参照しています。Mod はこの参照を、設定に合わせて相手のキャラのモデルに書き換えます。
モデルに付随する材質・骨・揺れものの設定も、一緒に移ります。

澪のモデルは、顔・歯・目まわりを専用の表示グループに置いていて、同行キャラ（繭）は
このグループを表示しません。澪のモデルをそのまま同行キャラに付けると、目玉以外の
顔が消えます。Mod は繭のモデルと同じ構造になるよう、顔を常時表示のグループへ移します
（ファイルのサイズは変わりません）。

紗重・八重のモデルは、赤い縄を澪・繭に無い表示グループに置いていて、澪・繭のキャラは
このグループを表示しません。顔と同じ方法で、縄を常時表示のグループへ移します。

千歳と、肌の色を変えた紗重・八重は、本編にも登場するので、モデルの写しを作って使います。
写しは、見た目として使っていない方の双子の初期衣装のファイルに置きます。千歳は体が小さいので、
写しの骨格を双子の動きに合わせて調整します。肌の色を変える設定では、顔と手足のテクスチャに、
もう一方の肌の色味を移したものを起動時に作ります（千歳には紗重の、紗重・八重には千歳の色味）。

この Mod は索引ファイル（root.rdb / root.rdx）をローダーに登録します。ゲームが索引
ファイルを最初に開くとき、ローダーはその時点の索引ファイル（Yumia fdata tools で入れた
Mod や、先に適用されたローダーの Mod の変更を含む）をこの Mod に渡します。Mod は
書き換えたデータを Mod 専用のデータファイルにまとめ、それを指すように書き換えた索引
ファイルを作ります。ローダーはそれらを `MixedNuts\cache\fdata_package\` に保存し、
ゲームにはそちらを開かせます。ファイルの差し替えそのものはローダーが行います
（詳しくはローダーの README を参照してください）。

---

# English

## What this does

**Swaps the looks of the player character (Mio) and the companion (Mayu).**
In the config file you can choose between playing as Mayu with Mio at your side,
two Mayus, or two Mios. From 2.1.0, **Sae and Yae Kurosawa** can be chosen too
(for example, play as Sae with Yae at your side). From 2.4.0, so can **Chitose Tachibana**.

Only the models (face, hair, body and costume) change. Animations, voices, subtitles
and the story stay as they are. For example, Mayu's limp remains on the companion
even when she looks like Mio.

2.0.0 is a **plugin for MixedNutsModLoader**. 1.x ran on its own via
`xinput1_4.dll`; from 2.0.0 the loader must be installed separately.

## Requirements

- FATAL FRAME II: Crimson Butterfly REMAKE (Steam)
- **MixedNutsModLoader 1.0.0 or later** (installed separately)
  Nexus Mods: https://www.nexusmods.com/fatalframe2crimsonbutterflyremake/mods/26
  GitHub: https://github.com/MixedNutsJP/fatal-frame2-remake-mod-loader
  (download it from its Releases)

No game files are modified, so this will not trip Steam's file integrity verification.

## What's included

| File | Role |
|---|---|
| `MixedNuts\Mods\twinswap\twinswap.dll` | the mod itself |
| `MixedNuts\Mods\twinswap\twinswap.ini` | configuration |
| `MixedNuts\Mods\twinswap\README.md` | this file |
| `MixedNuts\Mods\twinswap\LICENSE.txt` | license |

You copy **the `MixedNuts` folder**; it merges into the loader's `MixedNuts` folder.
When the game runs, a log (`twinswap.log`) is created in `MixedNuts\Mods\twinswap\`.
The swap data is generated by the loader in the shared
`MixedNuts\cache\fdata_package\` folder.

### Using it with other mods

- **It does not interfere with Native120FPSOption or MouseWheelCameraSpeed.**
  They and this mod (2.0.0) all run on the same loader, so they share one DLL and
  never conflict
- **It works together with mods installed with Yumia fdata tools** (costume edits
  and so on). The swap data is built from the game data as it currently is,
  including those mods, so if you reinstall them it is rebuilt on the next launch
- Other mods on the same loader that also modify root.rdb / root.rdx are applied
  one after another in folder-name (alphabetical) order; this mod starts from
  whatever the previous mods produced
- If another mod **already uses `dinput8.dll`, do not overwrite it.** The loader's
  `dinput8.dll` can be renamed to `version.dll` or `xinput1_4.dll` (see the
  loader's README)

## Installation

1. Close the game.

2. **Install MixedNutsModLoader first.** See the loader's README for the steps.

3. Copy this mod's `MixedNuts` folder into the game's root directory (the folder
   containing `FatalFrameII.exe`). It merges into the loader's `MixedNuts` folder.

   ```
   ...\FatalFrameII\FatalFrameII.exe
   ...\FatalFrameII\dinput8.dll                                     <- loader
   ...\FatalFrameII\MixedNuts\MixedNutsLoader.dll                   <- loader
   ...\FatalFrameII\MixedNuts\Mods\twinswap\twinswap.dll            <- this mod
   ...\FatalFrameII\MixedNuts\Mods\twinswap\twinswap.ini            <- this mod
   ...\FatalFrameII\MixedNuts\Mods\twinswap\twinswap.log            <- generated at launch
   ...\FatalFrameII\MixedNuts\cache\fdata_package\root.rdb          <- generated by the loader
   ...\FatalFrameII\MixedNuts\cache\fdata_package\root.rdx          <- generated by the loader
   ...\FatalFrameII\MixedNuts\cache\fdata_package\0xfffe7510.fdata  <- generated by the loader
   ```

   To open the game folder: right-click the title in your Steam library →
   **Manage** → **Browse local files**

4. Launch the game and load a save.

### Upgrading from 1.x

Before installing, delete the old **`xinput1_4.dll`** and the old
**`Mods\twinswap\` folder** (including its `cache`) from the game root. If
`xinput1_4.dll` from 1.x is still there, the loader does not load the new mod and
writes a message starting with `[!!]` to `MixedNuts\loader.log`.

If you also have Native120FPSOption (`dinput8.dll` + `Mods\native120fps\`) or
MouseWheelCameraSpeed (`version.dll` + `Mods\wheelspeed\`) at 1.x, update them
all at once. The loader's README has the details.

You may copy your `Main` / `Sub` settings from the old `twinswap.ini`.

## Uninstallation

Delete the `MixedNuts\Mods\twinswap\` folder. The loader can stay for other mods;
to remove everything, delete the loader too (`dinput8.dll` and the `MixedNuts`
folder). No game files are modified, so removal restores the original state
completely.

To disable temporarily, set `Enabled` to `0` in `twinswap.ini`.

## Configuration

`twinswap.ini` exposes the following. Changes take effect after restarting the game.

| Key | Meaning |
|---|---|
| `Main` | Look of the player character (Mio in the story). `mio` / `mayu` / `sae` / `yae` / `chitose`. Default `mayu` |
| `Sub` | Look of the companion (Mayu in the story). Same values as `Main`. Default `mio` |
| `Rope` | Sae's and Yae's red rope. `1` = show (default) / `0` = hide |
| `Blindfold` | The blindfold on Mio's 2nd costume (summer cardigan). `default` = as the game does (default) / `show` = always shown / `hide` = never shown |
| `ChitoseSkin` | Chitose's skin. `default` = as the game does (a ghost's pale skin, default) / `human` = a living skin tone |
| `SaeYaeSkin` | Sae's and Yae's skin. `default` = as the game does (alive, default) / `ghost` = a ghost's pale skin |
| `Enabled` | `1` = on / `0` = off |
| `Log` | `1` = write a log file / `0` = no log |

| `Main` | `Sub` | Result |
|---|---|---|
| `mayu` | `mio` | the twins swapped (default) |
| `mayu` | `mayu` | two Mayus |
| `mio` | `mio` | two Mios |
| `mio` | `mayu` | vanilla |
| `sae` | `yae` | play as Sae with Yae at your side |
| `yae` | `sae` | play as Yae with Sae at your side |
| `chitose` | `mayu` | play as Chitose with Mayu at your side |

`sae` / `yae` / `chitose` can be combined freely with `mio` / `mayu` (for example `Main=mio` / `Sub=sae`).
Any value other than `mio` / `mayu` / `sae` / `yae` / `chitose` leaves that character's original look.

## Costume pairing

Costumes are paired one-to-one in costume menu order. For example, when Mio wears
her 2nd costume, the player character appears as Mayu in Mayu's 2nd costume.

- **The blindfold on Mio's 2nd costume (summer cardigan)** can be controlled with
  `Blindfold` (2.3.0). The model contains a white blindfold that is normally hidden. With
  the twins swapped and this costume selected, it shows up in the scene where she appears
  as a possessed enemy. Set `hide` to remove it, or `show` to have her wear it all the time.
  - Only the high-detail model has the blindfold, so from a distance it is not shown even
    with `show`
  - With `hide`, it is also hidden in scenes where the game itself would show it
  - Movies that play as pre-recorded video are not affected
  - It also works without swapping (`Main=mio` / `Sub=mayu`)
- **The SILENT HILL f collaboration costume and items are out of scope.** Mio's
  8th costume (the navy sailor outfit) has no counterpart on Mayu's side and is not
  swapped; with it selected, the player character keeps Mio's look
- Accessories are not part of the swap. They show up on the swapped look as usual
- **Sae and Yae have a single outfit (the white kimono).** A character set to `sae` /
  `yae` looks like her whichever costume is selected (except Mio's 8th)
- **Chitose also has a single outfit (her kimono).** A character set to `chitose` looks
  like her whichever costume is selected (except Mio's 8th)

### What has been checked in game

Only two pairs have actually been checked in game: **the default costumes** and
**the 7th costumes** (the Japanese-style gothic dresses; left wing for Mio, right
wing for Mayu). All three `Main` / `Sub` combinations, the TAB menu, the costume
menu and photo mode were checked with them.

The other costumes (2nd to 6th) go through exactly the same process, but have not
been checked in game yet. If one of them looks wrong, please let me know in a
GitHub Issue.

Sae and Yae were checked walking around (including the red rope) with
`Main=sae` / `Sub=yae` and `Main=yae` / `Sub=yae`.

Chitose was checked walking, holding hands and aiming the camera with
`Main=chitose` / `Sub=chitose` and `Main=chitose` / `Sub=mayu`.

## Notes

- **The preview in the costume menu also shows the swapped look.** The costume name
  and the preview not matching is expected with this mod
- **For movies to show the swap as well, set "Outfits During Movies" to "Current
  Outfit"** (in Options, under the display settings; in Japanese,
  "ムービー中の衣装" = "現在の衣装"). With that setting movies are rendered in the game,
  so they play with the swapped looks (checked with an ending movie). The other two
  choices, "Default Outfit" and "DDX Outfit", play videos pre-recorded in those outfits,
  so the original looks are shown. Even with "Current Outfit", a movie that only exists
  as a pre-recorded video would still show the original looks
- Scenes that are always rendered in the game (conversations, event scenes and so on)
  show the swap whatever the setting
- **Sae's and Yae's red rope may clip slightly through the kimono while walking.** It
  was made for their own body movement. Set `Rope=0` to hide it
- **Known issue: with `Rope=0` the red rope still appears in cutscenes.** It is hidden
  only while you are controlling the character. Cutscenes appear to use the display
  settings of Sae's and Yae's own models. Hiding it there as well would mean rewriting
  the model files themselves, which would also remove the rope from the real Sae and
  Yae who appear in events. Hiding it only on the swapped characters is hard to do with
  the way this mod works, so there is no fix planned for now
- **The skin tone can be chosen (2.4.0).** `ChitoseSkin=human` gives Chitose a living
  skin tone, and `SaeYaeSkin=ghost` gives Sae and Yae a ghost's pale skin. Only the colour
  of the face, hands and feet changes; the features stay as they are. On Chitose a little
  of the original grey remains around the eyes and on the lips
- **Known issue: the sleeves of Sae's and Yae's kimono may keep swaying even while
  standing still.** It happens whatever the skin setting, and the cause is not known
- Choosing Chitose's look, or the skin tones of Chitose, Sae and Yae, does not affect the
  Chitose, Sae and Yae who appear in the story (the mod works on its own copies of their
  models)
- With `sae` / `yae` selected, `Rope=1` and `SaeYaeSkin=default`, the Sae and Yae who
  appear in events also always show the whole red rope, even in scenes where part of it
  would normally be hidden
- **Chitose is shorter than the twins and her model has no feet.** Nothing being drawn
  below the hem is how the game's own model of her is made, not a fault of the mod
- With Chitose's look, she may appear taller for a moment when the two take hands, then
  goes back
- 2.1.0 had Sae and Yae the wrong way round. This is fixed in 2.2.0, so `sae` / `yae`
  look the other way round compared with 2.1.0 (Sae only has the rope tied around her
  waist; Yae's rope hangs down)
- The swap data (`MixedNuts\cache\fdata_package\`, up to about 100 MB) is generated
  by the loader. It is rebuilt automatically on the next launch when the game's
  files (including mods installed with Yumia fdata tools), the installed mods or
  their settings change; other launches reuse it. It is safe to delete (it is
  rebuilt on the next launch)
- A game update may break this mod. In that case the mod does nothing and the game
  runs unmodified; the reason is written to the log
- Nothing is written to your save data. Removing the mod restores the original looks
- Antivirus software may flag this mod because it intercepts the game opening its
  files. It performs no network activity, and the only files it writes are inside
  the `MixedNuts` folder

## Disclaimer

**This mod is provided as-is, without any warranty. The author accepts no liability
for any damage arising from its use,** including but not limited to corruption or
loss of save data, game malfunction, or any other problem. Use it at your own risk.

**Always back up your save data before installing.**

```
%LOCALAPPDATA%\KoeiTecmo\FatalFrameII\Savedata\
```

## If it doesn't work

1. Is the loader installed? `dinput8.dll` must be in the game's root folder (next
   to `FatalFrameII.exe`; **not inside the `MixedNuts` folder**), and
   `MixedNuts\MixedNutsLoader.dll` must be present
2. Has `MixedNuts\loader.log` been created, and does it contain
   `[OK] twinswap: loaded`? If `loader.log` is missing, the loader is not being
   loaded. If the line is missing, check that the folder and file names match
   `MixedNuts\Mods\twinswap\twinswap.dll`, and look for `[!!]` or `[NG]` lines in
   `loader.log`
3. Is `twinswap.ini` present in `MixedNuts\Mods\twinswap\`?
4. Has `twinswap.log` been created in `MixedNuts\Mods\twinswap\`?

If the logs contain lines like these, the mod is working:

`twinswap.log`:

```
TwinSwap 2.4.0  Main=mayu Sub=mio Rope=1 Blindfold=default
[OK] Registered with the loader
[OK] Generated the swap data (...)
```

`MixedNuts\loader.log`:

```
[OK] twinswap: loaded (1 file patches)
[OK] File hook installed (...)
[OK] twinswap: Main=mayu Sub=mio Rope=1 Blindfold=default (3 files)
```

`[OK] Generated the swap data (...)` and `[OK] twinswap: Main=mayu Sub=mio Rope=1 Blindfold=default (3 files)`
appear only on a launch where the swap data is built. On later launches the last
line of `loader.log` reads `[OK] Using the cached files (N)`.

With `Enabled=0`, or with `Main=mio` / `Sub=mayu` (vanilla) and `Blindfold=default`, the
log says `[OK] Nothing to do (disabled, or Main=mio / Sub=mayu with Blindfold=default)`
and nothing is registered.

When reporting a problem, please open a GitHub Issue and attach both `twinswap.log`
and `loader.log`.

https://github.com/MixedNutsJP/fatal-frame2-remake-twin-swap/issues

## How it works

No game files are modified.

Each costume refers to the model it uses from inside the game's database
(kidsobjdb). The mod rewrites those references to point at the other twin's models,
according to the settings. The material, skeleton and cloth settings attached to
each model move along with it.

Mio's models keep the face, teeth and eye area in a display group of their own,
which the companion (Mayu) never shows. Put on the companion as they are, Mio's
models lose everything of the face but the eyeballs. The mod moves the face into
the always-visible group, the same layout Mayu's models use (file sizes do not
change).

Sae's and Yae's models keep the red rope in display groups that Mio's and Mayu's
models do not have, so the twins never show them. The mod moves the rope into the
always-visible group in the same way as the face.

Chitose, and Sae and Yae with a changed skin tone, also appear in the story, so the mod
works on copies of their models, kept in the default costume files of whichever twin's
look is not in use.
Chitose is small, so the skeleton of her copy is adjusted to the twins' animations.
With a skin setting, the face and hand/foot textures are rebuilt at startup with the
other's skin tone transferred onto them (Sae's for Chitose, Chitose's for Sae and Yae).

The mod registers the index files (root.rdb / root.rdx) with the loader. When the
game first opens either of them, the loader hands the mod the current index files
(including mods installed with Yumia fdata tools and the changes of earlier loader
mods). The mod puts the rewritten data into a data file of its own and builds index
files rewritten to point at it; the loader stores them in
`MixedNuts\cache\fdata_package\` and has the game open those instead. The file
redirect itself is done by the loader (details in the loader's README).

---

## クレジット / Credits

- **SyobonAction** — 紗重・八重のスワップ Mod「Yae x Sae」を作っていただきました。紗重・八重の
  モデルの特定と、赤い縄の問題に気付く手がかりになりました。2.1.0 で紗重と八重を取り違えていた
  ことの指摘と、縄の表示を切り替える案、夏のカーディガンの目隠しの情報と切り替えの案も
  いただきました（ファイルやコードは使っていません）。
  Made the "Yae x Sae" swap mod, which helped identify Sae's and Yae's models and
  pointed out the missing red rope; also pointed out that 2.1.0 had Sae and Yae the
  wrong way round, suggested the rope toggle, and reported the blindfold on the summer
  cardigan model and suggested its switch (none of its files or code are used here).

## License

MIT License — Copyright (c) 2026 MixedNuts

本ソフトウェアは MIT ライセンスで提供されます。再配布・改変は自由ですが、
著作権表示とライセンス文を必ず残してください。

This software is provided under the MIT License. You are free to redistribute and
modify it, but the copyright notice and the license text must be retained.

https://github.com/MixedNutsJP/fatal-frame2-remake-twin-swap
