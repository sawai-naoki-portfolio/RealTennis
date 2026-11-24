# Tools/Reference — 参照シミュレーション

ボール物理のモデル（既存設計 §3：RK4・240 Hz・抗力・マグヌス・撃力バウンド）を Python で再実装したものです。調査報告 [RES-02](../../Docs/00_Research/02_Ball_Physics_Research.md) の数値実験を再現し、C++ の自動テスト（T-BALL）の期待値の出どころになります。

| ファイル | 内容 |
|---|---|
| `ballsim_reference.py` | モデルと、報告書の表の数値を出力する処理（標準ライブラリのみ） |
| `tests/test_ballsim_reference.py` | 報告書に書いた数値と一致するかの単体テスト（CI で実行） |

```powershell
py -I Tools/Reference/ballsim_reference.py                                         # 表の数値を出力
py -I -m unittest discover -s Tools/Reference/tests -p "test_*.py"                 # 照合
```

- 数値を変えるときは、[RES-02](../../Docs/00_Research/02_Ball_Physics_Research.md) と [パラメータ表](../../Docs/05_Data/02_Parameter_Tables.md) を同じ PR で更新する
- これは UE の実装の「正解」ではありません。C++ の実装と食い違ったら、どちらの誤りかを調べてから直します（[コーディング規約 T-3](../../Docs/02_Architecture/03_Coding_Standards.md)）
