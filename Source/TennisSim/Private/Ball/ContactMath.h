// 円柱との衝突に共通の計算（白帯・ポスト・スティック）。Step の衝突だけが使う（モジュールの外に出さない）。
#pragma once

#include "CoreMinimal.h"

namespace TennisContactMath
{
	/**
	 * 平面の上で、A から A + B へ直線に動く点が、原点を中心とする半径 D の円に入る割合（0〜1）。
	 * 始めから円の中にいて近づいているなら 0。入らない・離れていくなら値なし。
	 */
	TOptional<double> SweptCircleEntry(const FVector2D& A, const FVector2D& B, double D);

	/** 法線 Normal（単位ベクトル、ボールの側を向く）の面に当たった速度を、反発 e と摩擦 μ の撃力で変える。離れていくなら変えない。 */
	void ApplyImpulse(FVector& Vel, const FVector& Normal, double Restitution, double Friction);
}
