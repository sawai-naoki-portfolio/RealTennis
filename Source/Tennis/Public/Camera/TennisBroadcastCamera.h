// 中継カメラの計算（DD-07 §1.1、FR-CAM-001）：目標の位置と注視点、ばね、画面への投影、向きと FOV の制約（相手のベースラインと
// 自分の選手を画面に入れる）。座標はコート局所（m）。
// 演出の値なのでフレームの DeltaTime を使う（ゲームの結果に関わらない：ARCH-03 D-3 の対象外）。
#pragma once

#include "CoreMinimal.h"
#include "TennisBroadcastCamera.generated.h"

/** 中継カメラの調整の値（DATA-02 §8） */
USTRUCT()
struct TENNIS_API FTennisBroadcastCameraParams
{
	GENERATED_BODY()

	/** ベースラインの後ろの距離（m） */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (Units = "m"))
	double BehindBaseline = 15.0;

	/** 高さ（m） */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (Units = "m"))
	double Height = 6.0;

	/** 位置の横：注視点の Y の割合 */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "0", ClampMax = "1"))
	double FollowY = 0.35;

	/** 注視点の Y：選手 → ボールの内分 */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "0", ClampMax = "1"))
	double BallWeight = 0.4;

	/** 注視点の X：ネットから自分の側へ（m） */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (Units = "m"))
	double LookAtX = 2.0;

	/** 注視点の高さ（m） */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (Units = "m"))
	double LookAtZ = 0.8;

	/** 水平の FOV（度。基準の縦横比での値。縦の視野角はこれと基準の縦横比で決まる：Maintain Y-Axis FOV） */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "10", ClampMax = "120"))
	double FovDeg = 55.0;

	/** 基準の縦横比（FMinimalViewInfo::AspectRatio） */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "1"))
	double ReferenceAspect = 16.0 / 9.0;

	/** ばねの固有角振動数（rad/s、臨界減衰）：位置・注視点 */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "0.1"))
	double LocationOmega = 2.5;

	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "0.1"))
	double LookAtOmega = 4.0;

	/** 相手のベースラインと自分の選手を、画面の端からこの割合（画面の高さ・幅に対して）以上内側に入れる */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "0", ClampMax = "0.4"))
	double EdgeMargin = 0.08;

	/** 選手が入らないときに広げる FOV の上限（度。FR-CAM-001 の上限） */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "10", ClampMax = "120"))
	double MaxFovDeg = 60.0;

	/** 画面に入れる選手の高さ（m。足 Z = 0 から頭まで） */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (Units = "m"))
	double PlayerHeight = 1.8;
};

/** 臨界減衰のばね（閉じた形の式。長いフレームでも行き過ぎない） */
struct TENNIS_API FTennisCameraSpring
{
	FVector Value = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;

	/** 目標にそろえて止める */
	void Reset(const FVector& Target);
	/** Dt 秒だけ目標へ進める。Dt が正の有限の数でなければ何もしない */
	void Step(const FVector& Target, double Omega, double Dt);
};

namespace TennisBroadcastCamera
{
	/** 目標の位置と注視点。Side は自分の側（−1：X < 0、+1：X > 0） */
	TENNIS_API void Desired(const FTennisBroadcastCameraParams& Params, double Side, const FVector& Player, const FVector& Ball,
		FVector& OutLocation, FVector& OutLookAt);

	/** 相手のベースラインの両端（ダブルスのサイドライン） */
	TENNIS_API void OpponentBaselineEnds(double Side, FVector& OutA, FVector& OutB);

	/** 点 Point の画面の位置（端が ±1、上が +）。Maintain Y-Axis FOV：縦の視野角は FovDeg と ReferenceAspect、横は Aspect で広がる。
	 *  カメラの前になければ値なし */
	TENNIS_API TOptional<FVector2D> ProjectToScreen(const FVector& Location, const FRotator& Rotation, double FovDeg,
		double ReferenceAspect, double Aspect, const FVector& Point);

	/** 最後の向きと FOV（DD-07 §1.1 の制約）。相手のベースラインの両端は必ず、自分の選手（足と頭）は守れる範囲で、画面の端の内側に入れる：
	 *  縦は選手の足が入るまで下を向き（相手が上へ出ない範囲で）、横は選手の側へ向く（相手の両端が入る範囲で）。足りなければ FOV を
	 *  MaxFovDeg まで広げて解き直す */
	TENNIS_API FRotator ConstrainedRotation(const FTennisBroadcastCameraParams& Params, double Side, double Aspect,
		const FVector& Location, const FVector& LookAt, const FVector& Player, double& OutFovDeg);
}
