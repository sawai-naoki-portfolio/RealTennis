// 単位の変換（ARCH-03 U-3・U-4）。変換の式はここにだけ書く。物理（TennisSim）の値は SI 単位で持つ。
#pragma once

#include "CoreMinimal.h"

namespace TennisUnits
{
	/** 回転数（rpm）を角速度（rad/s）にする。rpm はデータの入力と表示にだけ使う */
	constexpr double RpmToRadPerSec(double Rpm)
	{
		return Rpm * UE_DOUBLE_TWO_PI / 60.0;
	}
}
