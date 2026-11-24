// ショットソルバ v2（DD-02 §7）：狙った点に落ちる打ち出しの状態を、本番と同じ物理（TennisBallPhysics::Step）で解く。
#pragma once

#include "CoreMinimal.h"
#include "Shot/ShotTypes.h"
#include "TennisSimTypes.h"

class TENNISSIM_API FTennisShotSolver
{
public:
	/** 最初のバウンドまでの試行の結果（DD-02 §7.3）。 */
	struct FFlight
	{
		bool    bLanded = false;
		FVector LandPos = FVector::ZeroVector;  // m（最初のバウンド。なければ最後の位置）
		bool    bReachedNet = false;
		double  NetClearance = 0.0;  // m（ネットの面で Z − R − h(Y)。白帯・網に触れた球も同じ式）
		double  Apex = 0.0;          // m（ボールの中心の最高点）
		double  FlightTime = 0.0;    // s
	};

	/** 物理の条件を写す。ソルバが知る風は平均だけなので、突風は外す（FR-ENV-004）。 */
	explicit FTennisShotSolver(const FBallSimContext& InContext);

	/** 解く。収束しなければ、残差が最小の解を Fallback = Nearest で返す。 */
	FShotSolveResult Solve(const FShotRequest& Request) const;

	/** 打ち出しから最初のバウンドまで（最長 4 s）進める。体の判定はしない。 */
	FFlight SimulateToFirstBounce(const FBallState& Launch) const;

	/** 方位 Azimuth（rad）・速さ（m/s）・仰角（rad）と、リクエストの回転から、打ち出しの状態を作る（DD-02 §3.1）。 */
	static FBallState MakeLaunch(const FShotRequest& Request, double Azimuth, double Speed, double Elevation);

private:
	FBallSimContext Context;
};
