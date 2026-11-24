// コート面とボールの校正の試験（DD-01 §4.2、RES-02 §3.4、ITF CS 01/02・Appendix I）。物理と同じ関数で模擬する。
// 校正のツール（W-1.9）と自動テストが、同じ計算を使う（DRY）。
#pragma once

#include "CoreMinimal.h"
#include "TennisSimTypes.h"

struct FCprResult
{
	double Cpr = 0.0;                  // Court Pace Rating = 100 (1 − μ_app) + 150 (0.81 − e_app)
	double ApparentFriction = 0.0;     // μ_app
	double ApparentRestitution = 0.0;  // e_app
	int32  Category = 0;               // ITF の区分（1：遅い 〜 5：速い）
};

namespace TennisCourtCalibration
{
	inline constexpr double CprSpeed = 30.0;         // m/s（ITF の CPR 試験）
	inline constexpr double CprAngleDeg = 16.0;      // 度（入射角）
	inline constexpr double DropHeightM = 2.54;      // m（ボールの底の高さ：Appendix I）

	/** 仮想 CPR 試験：30 m/s・16°・回転なしの球を、基準の気温で 1 回弾ませる。 */
	TENNISSIM_API FCprResult VirtualCprTest(const FSurfaceParams& Surface, const FBallParams& Ball);

	/** ITF の区分（30 未満が 1、35 未満が 2、40 未満が 3、45 未満が 4、それ以上が 5）。 */
	TENNISSIM_API int32 CprCategory(double Cpr);

	/**
	 * ドロップテスト：硬い面（e = e_ball）に 254 cm から落とし、最初の跳ね返りの頂点（ボールの底、cm）を返す。
	 * 本番と同じ Step で、ネットから離れた位置で進める。
	 */
	TENNISSIM_API double DropTestReboundCm(const FBallParams& Ball, const FAirParams& Air);
}
