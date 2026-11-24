// ITF Rules of Tennis 2026 の規格値と、物理の定数。調整しない値だけを置く（調整の値はデータへ。ARCH-03 S-5）。
// 単位は SI（m・s）。出典：Docs/00_Research/01_Tennis_Rules_Research.md §1、DD-01 §1。
#pragma once

namespace TennisConst
{
	// ---- コート（Rule 1） ----
	inline constexpr double HalfLength       = 23.77 / 2.0;  // m（全長 23.77 m）
	inline constexpr double HalfWidthSingles = 8.23 / 2.0;   // m（シングルス幅 8.23 m）
	inline constexpr double HalfWidthDoubles = 10.97 / 2.0;  // m（ダブルス幅 10.97 m）
	inline constexpr double ServiceLineX     = 6.40;         // m（ネットから）
	inline constexpr double LineWidth        = 0.05;         // m
	inline constexpr double BaseLineWidth    = 0.10;         // m（最大）
	inline constexpr double CenterMarkLength = 0.10;         // m

	// ---- ネット（Rule 1） ----
	inline constexpr double NetHeightCenter  = 0.914;        // m
	inline constexpr double NetHeightPost    = 1.07;         // m
	inline constexpr double PostOffset       = 0.914;        // m（サイドラインからポスト・スティックの中心まで）
	inline constexpr double SinglesStickY    = HalfWidthSingles + PostOffset;  // 5.029 m
	inline constexpr double NetPostY         = HalfWidthDoubles + PostOffset;  // 6.399 m
	inline constexpr double PostRadius       = 0.15 / 2.0;   // m（直径 15 cm 以下）
	inline constexpr double StickRadius      = 0.075 / 2.0;  // m（直径 7.5 cm 以下）
	inline constexpr double PostTopAboveCord = 0.025;        // m（ポストの上端は白帯の上端から 2.5 cm 以下）
	inline constexpr double PostHeight       = NetHeightPost + PostTopAboveCord;  // 1.095 m

	// ---- 物理 ----
	inline constexpr double Gravity          = 9.81;         // m/s²
	inline constexpr double SimHz            = 240.0;        // 固定ステップの周期（ARCH-03 D-3）
	inline constexpr double SimDt            = 1.0 / SimHz;  // s
}
