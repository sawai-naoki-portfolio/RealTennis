// コートの幾何（DD-01 §5.1）。規格値は TennisConstants.h にだけ置き、ここはそれを使う計算だけ。
#pragma once

#include "CoreMinimal.h"

namespace TennisCourtGeometry
{
	/**
	 * ネットの上端（白帯の軸）の高さ（m）。|Y| ≤ シングルススティックでは中央 0.914 m からスティック 1.07 m まで直線、
	 * スティックからポストまでは 1.07 m（白帯だけ）。ポストの外にネットはない（呼ぶ側が HasNetAt で確かめる）。
	 */
	TENNISSIM_API double NetHeightAt(double Y);

	/** Y の位置にネットの面（本体か白帯）があるか（|Y| ≤ ポスト。外を回る球にはネットの判定をしない：Rule 25 c）。 */
	TENNISSIM_API bool HasNetAt(double Y);

	/** Y の位置にネットの本体（網）があるか（|Y| ≤ シングルススティック。スティックとポストの間は白帯だけ：Rule 25 d）。 */
	TENNISSIM_API bool HasNetBodyAt(double Y);
}
