#include "Camera/TennisBroadcastCamera.h"

#include "TennisConstants.h"

namespace
{
	constexpr int32 SearchSteps = 30;          // 二分探索の回数（角度の幅 115° を 1e-7° まで）
	constexpr double LowestPitchDeg = -85.0;    // 下を向く限り
	constexpr double HighestPitchDeg = 30.0;
	constexpr int32 AlternateRounds = 3;       // 縦と横を交互に解く回数

	/** Good で真・Bad で偽の条件の、真の側の境界（Good と Bad の大小は問わない） */
	double Boundary(TFunctionRef<bool(double)> Ok, double Bad, double Good)
	{
		for (int32 Step = 0; Step < SearchSteps; ++Step)
		{
			const double Mid = (Bad + Good) / 2.0;
			if (Ok(Mid))
			{
				Good = Mid;
			}
			else
			{
				Bad = Mid;
			}
		}
		return Good;
	}

	/** 制約を確かめるための視点（Location と FOV は固定。向きだけを変えて調べる） */
	struct FCheck
	{
		const FTennisBroadcastCameraParams& Params;
		FVector Location;
		double Aspect;
		double FovDeg;
		double Limit;  // 端から EdgeMargin だけ内側（画面の位置で）

		TOptional<FVector2D> Screen(double Pitch, double Yaw, const FVector& Point) const
		{
			return TennisBroadcastCamera::ProjectToScreen(Location, FRotator(Pitch, Yaw, 0.0), FovDeg, Params.ReferenceAspect, Aspect, Point);
		}
		bool BelowTop(double Pitch, double Yaw, const FVector& Point) const
		{
			const TOptional<FVector2D> S = Screen(Pitch, Yaw, Point);
			return S.IsSet() && S->Y <= Limit;
		}
		bool AboveBottom(double Pitch, double Yaw, const FVector& Point) const
		{
			const TOptional<FVector2D> S = Screen(Pitch, Yaw, Point);
			return S.IsSet() && S->Y >= -Limit;
		}
		bool InsideSides(double Pitch, double Yaw, const FVector& Point) const
		{
			const TOptional<FVector2D> S = Screen(Pitch, Yaw, Point);
			return S.IsSet() && FMath::Abs(S->X) <= Limit;
		}
	};

	/** FOV を決めた視点で、向きを解く（DD-07 §1.1 の制約） */
	FRotator Solve(const FCheck& Check, double Side, const FVector& LookAt, const FVector& Player)
	{
		const FRotator Base = (LookAt - Check.Location).Rotation();
		const double YawToPlayer = (FVector(Player.X, Player.Y, 0.0) - Check.Location).Rotation().Yaw;
		const double YawToCenter = (FVector(LookAt.X, 0.0, LookAt.Z) - Check.Location).Rotation().Yaw;
		FVector Ends[2];
		TennisBroadcastCamera::OpponentBaselineEnds(Side, Ends[0], Ends[1]);
		const FVector Feet(Player.X, Player.Y, 0.0);
		const FVector Head(Player.X, Player.Y, Check.Params.PlayerHeight);

		double Pitch = Base.Pitch;
		double Yaw = Base.Yaw;
		for (int32 Round = 0; Round < AlternateRounds; ++Round)
		{
			// 縦：相手のベースラインが上の端を越えない最も下の向きと、選手の足が入る最も上の向きの間で、元の向きに最も近いもの
			const auto FarInside = [&](double P) { return Check.BelowTop(P, Yaw, Ends[0]) && Check.BelowTop(P, Yaw, Ends[1]); };
			const double LowestForFar = FarInside(LowestPitchDeg) ? LowestPitchDeg : Boundary(FarInside, LowestPitchDeg, HighestPitchDeg);
			Pitch = Base.Pitch;
			if (!Check.AboveBottom(Pitch, Yaw, Feet))
			{
				const auto FeetInside = [&](double P) { return Check.AboveBottom(P, Yaw, Feet); };
				Pitch = FeetInside(LowestPitchDeg) ? Boundary(FeetInside, Base.Pitch, LowestPitchDeg) : LowestPitchDeg;
			}
			Pitch = FMath::Max(Pitch, LowestForFar);  // 両方は入らなければ、相手のベースラインを優先

			// 横：選手の側へ向き（入るまで）、相手の両端が入る範囲に戻す
			const auto Blend = [](double From, double To, double Amount) { return From + FRotator::NormalizeAxis(To - From) * Amount; };
			const auto PlayerInside = [&](double Y) { return Check.InsideSides(Pitch, Y, Feet) && Check.InsideSides(Pitch, Y, Head); };
			const auto EndsInside = [&](double Y) { return Check.InsideSides(Pitch, Y, Ends[0]) && Check.InsideSides(Pitch, Y, Ends[1]); };
			Yaw = Base.Yaw;
			if (!PlayerInside(Yaw))
			{
				const double From = Yaw;
				const double Amount = PlayerInside(YawToPlayer)
					? Boundary([&](double A) { return PlayerInside(Blend(From, YawToPlayer, A)); }, 0.0, 1.0) : 1.0;
				Yaw = Blend(From, YawToPlayer, Amount);
			}
			if (!EndsInside(Yaw))
			{
				const double From = Yaw;
				Yaw = Blend(From, YawToCenter, Boundary([&](double A) { return EndsInside(Blend(From, YawToCenter, A)); }, 0.0, 1.0));
			}
		}
		return FRotator(Pitch, Yaw, 0.0);
	}
}

void FTennisCameraSpring::Reset(const FVector& Target)
{
	Value = Target;
	Velocity = FVector::ZeroVector;
}

void FTennisCameraSpring::Step(const FVector& Target, double Omega, double Dt)
{
	if (!(Dt > 0.0) || !FMath::IsFinite(Dt) || !(Omega > 0.0))
	{
		return;
	}
	// 臨界減衰：y = x − 目標、y(t) = (y0 + (v0 + ω y0) t) e^(−ωt)、v(t) = (v0 − ω (v0 + ω y0) t) e^(−ωt)
	const FVector Offset = Value - Target;
	const FVector Temp = (Velocity + Offset * Omega) * Dt;
	const double Decay = FMath::Exp(-Omega * Dt);
	Value = Target + (Offset + Temp) * Decay;
	Velocity = (Velocity - Temp * Omega) * Decay;
}

void TennisBroadcastCamera::Desired(const FTennisBroadcastCameraParams& Params, double Side, const FVector& Player, const FVector& Ball,
	FVector& OutLocation, FVector& OutLookAt)
{
	const double FocusY = FMath::Lerp(Player.Y, Ball.Y, Params.BallWeight);
	OutLocation = FVector(Side * (TennisConst::HalfLength + Params.BehindBaseline), Params.FollowY * FocusY, Params.Height);
	OutLookAt = FVector(Side * Params.LookAtX, FocusY, Params.LookAtZ);
}

void TennisBroadcastCamera::OpponentBaselineEnds(double Side, FVector& OutA, FVector& OutB)
{
	const double X = -Side * TennisConst::HalfLength;
	OutA = FVector(X, -TennisConst::HalfWidthDoubles, 0.0);
	OutB = FVector(X, TennisConst::HalfWidthDoubles, 0.0);
}

TOptional<FVector2D> TennisBroadcastCamera::ProjectToScreen(const FVector& Location, const FRotator& Rotation, double FovDeg,
	double ReferenceAspect, double Aspect, const FVector& Point)
{
	const FVector Local = Rotation.UnrotateVector(Point - Location);  // X：前、Y：右、Z：上
	if (Local.X <= UE_KINDA_SMALL_NUMBER || !(ReferenceAspect > 0.0) || !(Aspect > 0.0))
	{
		return {};
	}
	const double TanHalfY = FMath::Tan(FMath::DegreesToRadians(FovDeg) / 2.0) / ReferenceAspect;  // 縦は一定
	const double TanHalfX = TanHalfY * Aspect;
	return FVector2D(Local.Y / (Local.X * TanHalfX), Local.Z / (Local.X * TanHalfY));
}

FRotator TennisBroadcastCamera::ConstrainedRotation(const FTennisBroadcastCameraParams& Params, double Side, double Aspect,
	const FVector& Location, const FVector& LookAt, const FVector& Player, double& OutFovDeg)
{
	const double Limit = 1.0 - 2.0 * Params.EdgeMargin;
	FRotator Rotation;
	for (const double FovDeg : { Params.FovDeg, FMath::Max(Params.FovDeg, Params.MaxFovDeg) })
	{
		const FCheck Check{ Params, Location, Aspect, FovDeg, Limit };
		Rotation = Solve(Check, Side, LookAt, Player);
		OutFovDeg = FovDeg;
		const FVector Feet(Player.X, Player.Y, 0.0);
		const FVector Head(Player.X, Player.Y, Params.PlayerHeight);
		if (Check.AboveBottom(Rotation.Pitch, Rotation.Yaw, Feet) && Check.InsideSides(Rotation.Pitch, Rotation.Yaw, Feet)
			&& Check.InsideSides(Rotation.Pitch, Rotation.Yaw, Head))
		{
			break;  // 選手が入った：FOV は広げない
		}
	}
	return Rotation;
}
