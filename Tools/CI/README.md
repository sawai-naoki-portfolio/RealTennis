# CI のスクリプト

GitHub Actions（[`.github/workflows/ci.yml`](../../.github/workflows/ci.yml)）から呼ばれる検査です。ローカルでも実行できます。CI はこのほかに、`Tools/*/tests` の Python の単体テスト（unittest）をすべて実行します。

| スクリプト | 内容 |
|---|---|
| `check_secrets.py` | 追跡しているファイルに秘密情報（トークン・鍵・Config の資格情報）がないか。見つけた値は伏せ字で出す |
| `check_project.py` | `.uproject` の JSON、エンジンの版、プラグインとモジュールの記述、`Config/DefaultEngine.ini` の有無 |
| `check_links.py` | Markdown の相対リンク。CI は `LINK_CHECK_STRICT=1`（strict）で実行し、未作成の文書へのリンクもエラーにする。新しい文書を予定として索引に先に載せたい場合は、その PR で一時的に `0` にせず、文書と索引を同じ PR に入れる |
| `check_automation_report.py` | Unreal の自動テストの報告（`index.json`）を読み、`Tennis.` のテストに失敗・未実行があるか、1 件もないなら止める。失敗したテストの名前とエラーを出す（C++ ビルド用ワークフロー（セルフホスト。公開版には未収録） が使う） |
| `check_mermaid.mjs` | Markdown 内の Mermaid 図を mermaid-cli で描画し、構文エラーを検出する |
| `package.json`・`package-lock.json` | Node のツール（mermaid-cli）の版の固定。`npm ci` で入れる。更新は Dependabot の PR で行う |

## ローカルでの実行（Windows・PowerShell）

```powershell
py Tools/CI/check_secrets.py
py Tools/CI/check_project.py
$env:LINK_CHECK_STRICT = "1"            # CI と同じ strict（未作成の文書へのリンクもエラー）
py Tools/CI/check_links.py
Get-ChildItem Tools/*/tests -Directory | ForEach-Object { py -I -m unittest discover -s $_.FullName -p "test_*.py" }
$env:PUPPETEER_SKIP_DOWNLOAD = "true"   # 既存の Chrome を使う場合（Chrome をダウンロードしない）
npm ci --prefix Tools/CI --no-audit --no-fund     # package-lock.json の版で入れる
$env:CHROME_PATH = "C:\Program Files\Google\Chrome\Application\chrome.exe"
node Tools/CI/check_mermaid.mjs
```

## セルフホストのランナー（C++ のビルドと自動テスト：C++ ビルド用ワークフロー（セルフホスト。公開版には未収録））

GitHub のサーバーには Unreal Engine がないので、C++ のビルドと `Tennis.` の自動テストは、UE 5.8 を入れた PC のランナーで動かす（W-1.10、QA-01 §8）。

| 項目 | 内容 |
|---|---|
| ランナー | 公式の `actions/runner`（`C:\actions-runner`）。取得したら、リリースのノートの SHA256 と照らし合わせてから展開する |
| ラベル | `self-hosted`・`windows`・`unreal`（C++ ビルド用ワークフロー（非公開） の `runs-on`） |
| 登録 | リポジトリの Settings → Actions → Runners → New self-hosted runner に出る `config.cmd --url … --token …`。**トークンは秘密情報なので、会話やリポジトリに貼らず、人が自分の端末で実行する** |
| 起動 | `C:\actions-runner\run.cmd`（開いている間だけジョブを受ける） |
| エンジンの場所 | C++ ビルド用ワークフロー（非公開） の `UE_ROOT`（既定 `C:\Program Files\Epic Games\UE_5.8`） |

注意：

- 非公開のリポジトリで、PR を作るのは開発者だけという前提で使う（PR のコードがこの PC で動くため）。公開する・外部の人を加えるときは、フォークからの PR でランナーが動かない設定を確かめてから
- ワークフローの権限は `contents: read` だけ。checkout は `persist-credentials: false`
- ランナーは別のフォルダ（`C:\actions-runner\_work`）にリポジトリを取り出してビルドする。作業フォルダ（エディタが開いている場所）には触れない
- この PC でエディタが（Live Coding が有効なまま）開いていても、C++ ビルド用ワークフロー（非公開） は UBT に `-NoHotReloadFromIDE` を渡すので止まらない（Live Coding の確認はエンジンの実行ファイルごとで、別のフォルダのビルドまで止めるため。2026-11-05 の初回の実行で確かめた）
