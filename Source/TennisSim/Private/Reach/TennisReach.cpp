#include "Reach/TennisReach.h"

#include "Stroke/TennisStrokeImpact.h"
#include "Stroke/TennisStrokeTiming.h"

#include "TennisSimLog.h"

namespace
{
	/** 目標の向きの 1 次元の動きの区間：速さ Speed から加速 Accel（負なら減速）で Duration 秒。最後の区間は最高速度で限りなく続く */
	struct FPhase
	{
		double Duration = 0.0;
		double Speed = 0.0;
		double Accel = 0.0;
	};
	using FPhases = TArray<FPhase, TInlineAllocator<4>>;

	/** Q の選手が向き Dir に走るときの区間（TennisMovement::Step と同じ規則） */
	FPhases BuildPhases(const FReachQuery& Q, const FVector2D& Dir, const FMovementParams& M)
	{
		const bool bBackpedal = TennisMovement::IsBackpedal(Dir, Q.SideSign);
		const double MaxSpeed = M.MaxSpeed * (bBackpedal ? M.BackpedalSpeedFactor : 1.0);
		const double Accel = M.Accel * (bBackpedal ? M.BackpedalAccelFactor : 1.0);
		double Speed = FVector2D::DotProduct(Q.Velocity, Dir);
		Speed *= Speed < 0.0 ? M.TurnKeep : 1.0;  // 向きを変える瞬間の損失（逆向きの間は加速で向きを変える）
		double BoostLeft = FMath::Max(Q.BoostTimeLeft, 0.0);

		FPhases Phases;
		// 速さ（向きを問わない）が最高速度を超えていれば、Brake で最高速度まで変える（目標の速さより速いときは減速の割合）
		if (FMath::Abs(Speed) > MaxSpeed)
		{
			const double Edge = Speed > 0.0 ? MaxSpeed : -MaxSpeed;
			const double Rate = Speed > 0.0 ? -M.Brake : M.Brake;
			const double Dt = (Edge - Speed) / Rate;
			Phases.Add({ Dt, Speed, Rate });
			Speed = Edge;
			BoostLeft = FMath::Max(BoostLeft - Dt, 0.0);
		}
		// 加速：初動ブーストの区間と、その後の区間
		for (const bool bBoosted : { true, false })
		{
			const double A = Accel * (bBoosted ? M.BoostAccelFactor : 1.0);
			const double ToMax = Speed < MaxSpeed ? (MaxSpeed - Speed) / A : 0.0;
			const double Dt = bBoosted ? FMath::Min(ToMax, BoostLeft) : ToMax;
			if (Dt > 0.0)
			{
				Phases.Add({ Dt, Speed, A });
				Speed += A * Dt;
			}
		}
		Phases.Add({ TNumericLimits<double>::Max(), MaxSpeed, 0.0 });
		return Phases;
	}

	/** 速さ S・加速 A（負なら減速）で、距離 Remaining（> 0）を進む最初の時間（解がある範囲で呼ぶ） */
	double SolveTime(double S, double A, double Remaining)
	{
		return A == 0.0 ? Remaining / S : (-S + FMath::Sqrt(S * S + 2.0 * A * Remaining)) / A;
	}

	double TimeToCover(const FPhases& Phases, double Distance)
	{
		double Time = 0.0;
		double Covered = 0.0;
		for (int32 Index = 0; Index < Phases.Num(); ++Index)
		{
			const FPhase& Phase = Phases[Index];
			const bool bLast = Index == Phases.Num() - 1;
			const double Dx = bLast ? TNumericLimits<double>::Max() : Phase.Speed * Phase.Duration + 0.5 * Phase.Accel * Phase.Duration * Phase.Duration;
			if (bLast || Covered + Dx >= Distance)
			{
				return Time + SolveTime(Phase.Speed, Phase.Accel, Distance - Covered);
			}
			Time += Phase.Duration;
			Covered += Dx;
		}
		return TNumericLimits<double>::Max();
	}

	/** 区間に沿って Time 秒で進む距離 */
	double DistanceIn(const FPhases& Phases, double Time)
	{
		double Elapsed = 0.0;
		double Covered = 0.0;
		for (const FPhase& Phase : Phases)
		{
			const double Dt = FMath::Min(Phase.Duration, Time - Elapsed);
			Covered += Phase.Speed * Dt + 0.5 * Phase.Accel * Dt * Dt;
			Elapsed += Dt;
			if (Elapsed >= Time)
			{
				break;
			}
		}
		return Covered;
	}

	bool IsFinite(const FVector2D& V)
	{
		return FMath::IsFinite(V.X) && FMath::IsFinite(V.Y);
	}

	bool IsValid(const FReachQuery& Q)
	{
		return IsFinite(Q.Position) && IsFinite(Q.Velocity) && FMath::IsFinite(Q.BoostTimeLeft) && FMath::IsFinite(Q.Now)
			&& FMath::IsFinite(Q.ReactionTime) && FMath::IsFinite(Q.PrepTime);
	}

	/** サンプルの種類（DD-03 §4.5）。どの帯にも入らなければ false */
	bool Classify(const FBallState& S, double FirstBounceTime, const FReachParams& P, EHitKind& OutKind)
	{
		const double Z = S.Pos.Z;
		if (Z >= P.SmashMinZ && Z <= P.SmashMaxZ)
		{
			OutKind = EHitKind::Smash;
			return true;
		}
		if (S.NumBounces == 0)
		{
			OutKind = EHitKind::Volley;
			return Z >= P.VolleyMinZ && Z <= P.VolleyMaxZ;
		}
		if (S.Time - FirstBounceTime <= P.HalfVolleyWindow && Z <= P.HalfVolleyMaxZ)
		{
			OutKind = EHitKind::HalfVolley;
			return true;
		}
		OutKind = EHitKind::Groundstroke;
		return Z >= P.GroundMinZ && Z <= P.GroundMaxZ;
	}

	double SideDistance(EHitKind Kind, EStrokeSide Side, EBackhandStyle Backhand, const FReachParams& P)
	{
		switch (Kind)
		{
		case EHitKind::Volley:
			return P.VolleySide;
		case EHitKind::Smash:
			return P.SmashSide;
		default:
			if (Side == EStrokeSide::Forehand)
			{
				return P.ForehandSide;
			}
			return Backhand == EBackhandStyle::OneHanded ? P.OneHandedBackhandSide : P.TwoHandedBackhandSide;
		}
	}

	/** 高さの点：打ちやすい高さで 1、種類の帯の端で 0 まで線形（Smash は帯の中で 1） */
	double HeightScore(EHitKind Kind, double Z, const FReachParams& P)
	{
		if (Kind == EHitKind::Smash)
		{
			return 1.0;
		}
		const double Low = Kind == EHitKind::Volley ? P.VolleyMinZ : (Kind == EHitKind::HalfVolley ? 0.0 : P.GroundMinZ);
		const double High = Kind == EHitKind::Volley ? P.VolleyMaxZ : P.GroundMaxZ;
		if (Z < P.IdealMinZ)
		{
			return FMath::Clamp((Z - Low) / FMath::Max(P.IdealMinZ - Low, UE_DOUBLE_SMALL_NUMBER), 0.0, 1.0);
		}
		if (Z > P.IdealMaxZ)
		{
			return FMath::Clamp((High - Z) / FMath::Max(High - P.IdealMaxZ, UE_DOUBLE_SMALL_NUMBER), 0.0, 1.0);
		}
		return 1.0;
	}

	/** サンプル S を打つのに使える時間（s）：今・反応・準備を引いた残り（DD-03 §4.1） */
	double AvailableTime(const FBallState& S, const FReachQuery& Q)
	{
		return S.Time - Q.Now - Q.ReactionTime - Q.PrepTime;
	}

	/** 体の位置 Body が、選手の側の移動の範囲（DD-03 §2.4）の中か */
	bool IsInsideArea(const FVector2D& Body, double SideSign)
	{
		FVector2D Clamped = Body;
		FVector2D IgnoredVelocity = FVector2D::ZeroVector;
		TennisMovement::ClampToArea(Clamped, IgnoredVelocity, SideSign);
		return Clamped.Equals(Body, 0.01);
	}

	/** 点数を付ける前の候補 */
	struct FRawCandidate
	{
		FHitCandidate Candidate;
		double Margin = 0.0;  // s（間に合う余裕。ストレッチは 0）
		double Travel = 0.0;  // m（走る距離）
	};

	/** サンプル S をフォアかバックで打つ候補。間に合わず、ストレッチでも届かず、または体が範囲の外に出るなら unset */
	TOptional<FRawCandidate> Evaluate(const FBallState& S, EHitKind Kind, EStrokeSide Side, const FReachQuery& Q, const FMovementParams& M,
		const FReachParams& P)
	{
		const double Facing = Q.SideSign < 0.0 ? 1.0 : -1.0;  // ネットへの向き（X）
		const FVector2D Forward(Facing, 0.0);
		const FVector2D Right(0.0, Facing);                    // ネットの方を向いて右（UE は左手系：前 X・右 Y）
		const double Hand = Q.Handedness == ETennisHandedness::Right ? 1.0 : -1.0;
		const FVector2D TowardBall = Right * Hand * (Side == EStrokeSide::Forehand ? 1.0 : -1.0);  // 体から見たボールの横の向き

		const FVector2D Body = FVector2D(S.Pos.X, S.Pos.Y) - TowardBall * SideDistance(Kind, Side, Q.Backhand, P) - Forward * P.Forward;
		if (!IsInsideArea(Body, Q.SideSign))
		{
			return {};
		}

		const double Available = AvailableTime(S, Q);
		const FVector2D Delta = Body - Q.Position;
		const double Travel = Delta.Size();
		const bool bMoving = Travel > UE_DOUBLE_KINDA_SMALL_NUMBER;
		const FPhases Phases = BuildPhases(Q, bMoving ? Delta / Travel : Forward, M);
		const double Needed = bMoving ? TimeToCover(Phases, Travel) : 0.0;

		FRawCandidate Raw;
		Raw.Candidate.Time = S.Time;
		Raw.Candidate.BallPos = S.Pos;
		Raw.Candidate.BallVel = S.Vel;
		Raw.Candidate.BodyTarget = Body;
		Raw.Candidate.Side = Side;
		Raw.Candidate.Kind = Kind;
		Raw.Candidate.TimeToReach = Needed;
		Raw.Travel = Travel;
		if (Available >= 0.0 && Needed <= Available)
		{
			Raw.Margin = Available - Needed;
			return Raw;
		}
		if (Available > 0.0 && Travel - DistanceIn(Phases, Available) <= P.StretchReach)
		{
			Raw.Candidate.bStretch = true;
			return Raw;
		}
		return {};
	}

	/** 前で取る・移動の少なさを候補の中で正規化し、点数を付けて並べる（DD-03 §4.3・§4.5） */
	void ScoreAndSort(TArray<FRawCandidate>& Raws, const FReachParams& P, TArray<FHitCandidate>& Out)
	{
		double FirstTime = TNumericLimits<double>::Max();
		double LastTime = -TNumericLimits<double>::Max();
		double MinTravel = TNumericLimits<double>::Max();
		double MaxTravel = 0.0;
		for (const FRawCandidate& Raw : Raws)
		{
			FirstTime = FMath::Min(FirstTime, Raw.Candidate.Time);
			LastTime = FMath::Max(LastTime, Raw.Candidate.Time);
			MinTravel = FMath::Min(MinTravel, Raw.Travel);
			MaxTravel = FMath::Max(MaxTravel, Raw.Travel);
		}
		Out.Reset(Raws.Num());
		for (FRawCandidate& Raw : Raws)
		{
			FHitCandidate& C = Raw.Candidate;
			const double Early = LastTime > FirstTime ? (LastTime - C.Time) / (LastTime - FirstTime) : 1.0;
			const double Travel = MaxTravel > MinTravel ? (MaxTravel - Raw.Travel) / (MaxTravel - MinTravel) : 1.0;
			C.Score = P.HeightWeight * HeightScore(C.Kind, C.BallPos.Z, P)
				+ P.MarginWeight * FMath::Min(Raw.Margin / P.FullMargin, 1.0)
				+ P.EarlyWeight * Early
				+ P.TravelWeight * Travel
				+ P.PostureWeight * (C.bStretch ? 0.0 : 1.0);
			C.Score *= C.bStretch ? P.StretchScoreFactor : 1.0;
			Out.Add(C);
		}
		Out.Sort([](const FHitCandidate& A, const FHitCandidate& B) { return A.Score != B.Score ? A.Score > B.Score : A.Time < B.Time; });
	}
}

double TennisReach::TimeToReach(const FReachQuery& Q, const FVector2D& Target, const FMovementParams& M)
{
	if (!IsFinite(Q.Position) || !IsFinite(Q.Velocity) || !FMath::IsFinite(Q.BoostTimeLeft) || !IsFinite(Target))
	{
		UE_LOG(LogTennisStroke, Warning, TEXT("[Reach] TimeToReach: invalid input (treated as unreachable)"));
		return TNumericLimits<double>::Max();
	}
	const FVector2D Delta = Target - Q.Position;
	const double Distance = Delta.Size();
	if (Distance <= UE_DOUBLE_KINDA_SMALL_NUMBER)
	{
		return 0.0;
	}
	return TimeToCover(BuildPhases(Q, Delta / Distance, M), Distance);
}

void TennisReach::FindCandidates(const FTrajectory& Trajectory, const FReachQuery& Q, const FMovementParams& M, const FReachParams& P,
	TArray<FHitCandidate>& Out)
{
	Out.Reset();
	if (!IsValid(Q))
	{
		UE_LOG(LogTennisStroke, Warning, TEXT("[Reach] FindCandidates: invalid query (no candidates)"));
		return;
	}
	const FBallEvent* FirstBounce = Trajectory.FirstBounce();
	const double FirstBounceTime = FirstBounce != nullptr ? FirstBounce->Time : TNumericLimits<double>::Max();
	const double Facing = Q.SideSign < 0.0 ? 1.0 : -1.0;
	const double Hand = Q.Handedness == ETennisHandedness::Right ? 1.0 : -1.0;

	TArray<FRawCandidate> Raws;
	for (const FBallState& S : Trajectory.States)
	{
		if (S.NumBounces >= 2)
		{
			break;
		}
		EHitKind Kind;
		if (S.Time < Q.Now || S.Pos.X * Q.SideSign <= 0.0 || !Classify(S, FirstBounceTime, P, Kind))
		{
			continue;  // 過去・相手の側・帯の外
		}
		// 選手から見たボールの横の差（フォアの側が正）。中央に近ければ、まずフォア（回り込み）、間に合わなければバック
		const double ForeLateral = (S.Pos.Y - Q.Position.Y) * Facing * Hand;
		const bool bCenter = FMath::Abs(ForeLateral) < P.CenterBand;
		const EStrokeSide First = (bCenter || ForeLateral > 0.0) ? EStrokeSide::Forehand : EStrokeSide::Backhand;
		TOptional<FRawCandidate> Raw = Evaluate(S, Kind, First, Q, M, P);
		if (bCenter && (!Raw.IsSet() || Raw->Candidate.bStretch))
		{
			TOptional<FRawCandidate> Back = Evaluate(S, Kind, EStrokeSide::Backhand, Q, M, P);
			if (Back.IsSet() && (!Raw.IsSet() || !Back->Candidate.bStretch))
			{
				Raw = MoveTemp(Back);
			}
		}
		if (Raw.IsSet())
		{
			Raws.Add(MoveTemp(Raw.GetValue()));
		}
	}
	ScoreAndSort(Raws, P, Out);
}

FHitCandidate TennisReach::RefineForStroke(const FTrajectory& Trajectory, const FHitCandidate& Candidate, const FVector& SweetSpotOffset,
	const FVector2D& AimPoint, const FReachQuery& Q, const FMovementParams& M, const FReachParams& P, const FStrokeSyncParams& Sync)
{
	const FVector Handed = TennisStrokeImpact::HandedOffset(SweetSpotOffset, Q.Handedness);
	const FVector2D Offset(Handed.X, Handed.Y);
	const auto BodyFor = [&](const FVector& BallPos)
	{
		const FVector2D Ball(BallPos.X, BallPos.Y);
		return TennisStrokeTiming::BodyTarget(Ball, TennisStrokeTiming::FacingYaw(Ball, AimPoint, Q.SideSign, Sync), Offset);
	};

	FHitCandidate Best = Candidate;
	Best.BodyTarget = BodyFor(Candidate.BallPos);
	if (!IsValid(Q) || !FMath::IsFinite(SweetSpotOffset.X) || !FMath::IsFinite(SweetSpotOffset.Y) || !FMath::IsFinite(SweetSpotOffset.Z)
		|| !FMath::IsFinite(Candidate.Time) || !FMath::IsFinite(Candidate.BallPos.Z) || !IsFinite(Best.BodyTarget))
	{
		UE_LOG(LogTennisStroke, Warning, TEXT("[Reach] RefineForStroke: invalid input (keeping the candidate)"));
		return Candidate;
	}
	Best.TimeToReach = TimeToReach(Q, Best.BodyTarget, M);

	const FBallEvent* FirstBounce = Trajectory.FirstBounce();
	const double FirstBounceTime = FirstBounce != nullptr ? FirstBounce->Time : TNumericLimits<double>::Max();
	// 候補と同じバウンドの間だけを見る。軌道が打ち出しの状態から予測されている（サンプルの NumBounces を打ち出しから数える）前提。
	// 途中の状態から予測し直す形にするときは、候補にバウンドの数を持たせる
	const int32 CandidateBounces = Candidate.Time >= FirstBounceTime ? 1 : 0;
	const double Window = FMath::Max(Sync.HitRefineWindow, 0.0);
	const double CandidateGap = FMath::Abs(Candidate.BallPos.Z - SweetSpotOffset.Z);  // これより悪いサンプルは選ばない
	constexpr double SameGap = 1e-9;  // m（これより差が小さければ、同じ近さとして候補の時刻に近いほうを選ぶ）
	double BestGap = CandidateGap;
	for (const FBallState& S : Trajectory.States)
	{
		if (S.NumBounces >= 2 || S.Time > Candidate.Time + Window)
		{
			break;
		}
		EHitKind Kind;
		if (S.Time < Candidate.Time - Window || S.Time < Q.Now || S.Pos.X * Q.SideSign <= 0.0 || S.NumBounces != CandidateBounces
			|| !Classify(S, FirstBounceTime, P, Kind) || Kind != Candidate.Kind)
		{
			continue;  // 範囲の外・過去・相手の側・バウンドの前後が違う・別の種類
		}
		const double Gap = FMath::Abs(S.Pos.Z - SweetSpotOffset.Z);
		const bool bCloser = Gap < BestGap - SameGap
			|| (Gap <= BestGap + SameGap && FMath::Abs(S.Time - Candidate.Time) < FMath::Abs(Best.Time - Candidate.Time));
		if (!bCloser || Gap > CandidateGap)  // 後の条件は、同点の幅の分だけ候補より遠いサンプルへ移る（同点が続く）のを防ぐ守り
		{
			continue;
		}
		const FVector2D Body = BodyFor(S.Pos);
		const double Needed = TimeToReach(Q, Body, M);
		if (!IsInsideArea(Body, Q.SideSign) || Needed > AvailableTime(S, Q))
		{
			continue;  // 体が範囲の外に出る・間に合わない
		}
		BestGap = Gap;
		Best.Time = S.Time;
		Best.BallPos = S.Pos;
		Best.BallVel = S.Vel;
		Best.BodyTarget = Body;
		Best.TimeToReach = Needed;
		Best.bStretch = false;
	}
	return Best;
}
