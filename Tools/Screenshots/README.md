# Tools/Screenshots — 画面の記録の撮影と操作

対応ごとのスクリーンショット（[Docs/Screenshots](../../Docs/Screenshots/README.md)）を撮るためのスクリプトです。

| ファイル | 内容 |
|---|---|
| `Capture-Window.ps1` | 指定したアプリ（既定は `UnrealEditor`）の、いちばん大きい表示中のウィンドウを前面に出し、その範囲だけを撮って、幅 1280 px 以下の PNG で保存する |
| `Send-WindowInput.ps1` | 同じウィンドウを前面に出して、クリック（`Capture-Window.ps1` で撮った画像の座標）かキー（`-HoldMs` の間押し続ける）を送る。PIE でコートをクリックする・キーで選手を動かすなどの確認に使う |

## 使い方

```bash
powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Screenshots/Capture-Window.ps1 \
  -Process UnrealEditor -Out Docs/Screenshots/2026-11-04/<対応内容>/court_top.png
```

- `-Process`：`UnrealEditor` か `blender` など（プロセス名）
- `-MaxWidth`：縮小の上限（既定 1280、320〜3840）
- 先にカメラを置く（Unreal MCP の `EditorAppToolset.SetCameraTransform` など）。浮いたウィンドウ（メッセージログなど）が重なっていたら閉じておく

### 操作を送る

```bash
powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Screenshots/Send-WindowInput.ps1 -X 548 -Y 355   # 撮った画像の (548, 355) をクリック
powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Screenshots/Send-WindowInput.ps1 -Key D -HoldMs 1000   # D を 1 秒押す
```

- 先に `Capture-Window.ps1` で撮って、押す位置を画像で決める（縮小の倍率は同じ `-MaxWidth` で合わせる）
- 前面に出せなければ送らない（ほかのアプリに入力を送らない）
- PIE でキーを送る前に、ビューポートの上の端の、地平線より上の空を 1 回クリックして、ビューポートにフォーカスを与える。地平線の位置は中継カメラの向きで変わるので、撮った画像で決める（決まった座標にしない）。地面をクリックすると打球ツールが打つ。Tab は球種を切り替えるので、フォーカスには使わない
- 球を打ってからキーを押す確認（例：球が来ている間に球種のボタンを押す）は、キーを送る実行をバックグラウンドで始めてから、すぐに MCP の `shoot_ball` を呼ぶ。キーは打ち出しの約 1.3 秒後に届く（スクリプトの起動と前面化の時間。2026-11-11 の実測）。届くまでに球が過ぎないように、滞空の長い球を使う（例：`DA_Shot_Lob` を (−5.5, 0.5) へ。バウンドの後、選手の前に落ちてくる）
- エディタが裏にあると、描画が毎秒 3 回ほどに落ち、シミュレーションの時間も遅く進む。時間に関わる確認は、ウィンドウを前面に出してから行う（クリックかキーを 1 回送れば前面になる）
- 1 回の実行で送れるキーは 1 つ。2 つを同時に押すとき（例：W と D で斜めに動く）は、2 つの実行を並行に走らせる

## 注意

- デスクトップ全体は撮らない（タスクバー・通知・ほかのアプリを写さない）
- Windows は裏のプロセスからの前面化を止めるので、Alt キーを一度押してから前面に出す。前面にできなければ、撮らずにエラーで止まる
- `MainWindowHandle` は浮いた小さなウィンドウを指すことがあるので、使わない（いちばん大きいウィンドウを選ぶ）
- ウィンドウの外側の見えない枠（Windows 10 以降の、サイズ変更用の数画素）は撮らない。`GetWindowRect` は枠まで含み、最大化したウィンドウでは端にタスクバーやデスクトップが写る（2026-11-18 に見つけ、それまでの 21 枚の端を切り落とした）。`DwmGetWindowAttribute`（`DWMWA_EXTENDED_FRAME_BOUNDS`）の見えている範囲を、画面の作業領域（タスクバーを除く）に切り詰めて撮る
- Windows PowerShell 5.1 で動かすため、ファイルは BOM 付きの UTF-8 で保存する。中の C# は英語だけで書く（文字コードを取り違えると、コメントが改行を巻き込んで壊れる）
- Blender のウィンドウの上部には、`.blend` のファイルのパスが写る。個人のフォルダのパスが入るので、Blender の画像は格納の前に確かめる（[Docs/Screenshots](../../Docs/Screenshots/README.md) の「載せてはいけないもの」）
