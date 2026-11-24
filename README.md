# RealTennis

> **ポートフォリオ作品**です（作品一覧：<https://github.com/sawai-naoki-portfolio>）。
> Unreal Engine 5.8 で制作する、物理ベースのリアルなテニスゲームです。**未公開・開発中**です。

このリポジトリは、作品を理解・評価するために必要なもの（ソースコード・設計書・設定・開発用ツール・CI）だけを載せた**公開版**です。履歴は移していません。

## 開発の進め方

個人開発です。**企画・仕様・設計判断・レビュー・テストでの検証は自分**で行い、**実装とドキュメントの作成には生成 AI（Claude Code）を活用**しています。

- 元のリポジトリ [dorachan121/RealTennis](https://github.com/dorachan121/RealTennis)（公開）では、`main` へは PR 経由でだけ変更を入れ、対応内容ごとにブランチを切って運用しています。PR は #191 まで進んでいます（2026-11-04〜2026-11-22 の 192 コミット）
- 設計書を先に更新してから実装する流れで、仕様・設計・テスト計画は `Docs/` にあります

## 公開状況

| 項目 | 内容 |
|---|---|
| 状態 | 未公開（ゲームとしての配布前）・開発中 |
| 技術 | Unreal Engine 5.8.4、C++（ブループリントも併用） |
| プラットフォーム | Windows PC（主）、iOS／Android（v1.1） |

## 中身

2026-11-24 時点で数えた値です。

| 場所 | 内容 | 数 |
|---|---|---|
| `Source/` | C++ のモジュール 4 つ：`TennisSim`（ボール物理・スコアなどのシミュレーション）、`Tennis`（ゲーム）、`TennisEditor`、`TennisTests` | `.h`・`.cpp` 167 ファイル（約 16,000 行）。うち `TennisTests` に自動テスト（CQTest）の `TEST_CLASS` 56・`TEST_METHOD` 307 |
| `Docs/` | 調査・仕様・設計・UML・詳細設計・データ・UI/UX・アート・QA・プロジェクト管理。索引は [`Docs/README.md`](Docs/README.md) | Markdown 68 ファイル（`Docs/Screenshots/` の説明 26 を含む） |
| `Config/` | エンジン・入力・ゲームの設定 | `.ini` 4 |
| `Tools/` | CI の検査（`Tools/CI/`）、参照シミュレーション（`Tools/Reference/`）、Blender／Unreal の制作スクリプト | Python 91 ファイル |
| `.github/` | CI（`ci.yml`）、Dependabot、PR テンプレート | |

## CI

[`.github/workflows/ci.yml`](.github/workflows/ci.yml) が、PR と `main` への push で次の検査を実行します（GitHub のホストのランナー・秘密情報なし）。

- 秘密情報の混入の検査、`Tennis.uproject` の検査
- Markdown の相対リンクの検査（strict）
- Python の単体テスト（`Tools/*/tests`）
- Markdown 内の Mermaid 図の描画の検査

詳細とローカルでの実行方法は [`Tools/CI/README.md`](Tools/CI/README.md) にあります。

**C++ のビルドと自動テストは、自分の環境（UE 5.8 を入れたセルフホストのランナー）で実行しており、公開版には含めていません。** 公開リポジトリでは、フォークからの PR によって自分の PC で任意のコードが動くのを避けるためです。

## 含めないもの

- **バイナリ**：`Content/` 以下の `.uasset`・`.umap`、画像（`.png`・`.jpg`。`Docs/Screenshots/` の撮影記録の画像 65 枚を含む）、音、`.fbx`、`.blend` は含めていません。このため、このリポジトリだけでは Unreal Engine のエディタで開いてもマップやキャラクターは揃いません。`.gitattributes` の Git LFS の指定は元のリポジトリと同じ設定を保つためのもので、この公開版には LFS で管理するファイルはありません
- **開発ルール集・エージェントの設定・MCP の設定**：開発支援用の個人設定のため含めていません。文書中でこれらに触れている箇所は「非公開」と表記しています
- 第三者のアセット、秘密情報、個人情報

## ビルド（参考）

Unreal Engine 5.8.4 と Visual Studio 2026 が必要です（上記のとおり `Content/` は含まれないため、ソースの確認・参考用です）。

1. `Tennis.uproject` を右クリック →「Generate Visual Studio project files」で `Tennis.sln` を作る（生成物なので Git には入れません）
2. `Tennis.sln` を開く。足りないコンポーネントは [`.vsconfig`](.vsconfig) を見て Visual Studio が勧めます
3. 構成は `Development Editor`・`Win64`、スタートアップ プロジェクトは `Tennis`

## ブランチと PR

元のリポジトリのルールは [CONTRIBUTING.md](CONTRIBUTING.md) にあります。
