#include "Movement/TennisMovement.h"

#include "TennisConstants.h"

namespace
{
	// 能力値 0 → 100 の写像（DD-03 §2.1）
	constexpr double MaxSpeedAt0 = 4.8, MaxSpeedAt100 = 6.2;  // m/s
	constexpr double AccelAt0 = 6.5, AccelAt100 = 9.5;        // m/s²
	constexpr double BrakeAt0 = 8.5, BrakeAt100 = 11.5;       // m/s²

	FVector2D SanitizeInput(FVector2D Input)
	{
		if (!FMath::IsFinite(Input.X) || !FMath::IsFinite(Input.Y))
		{
			return FVector2D::ZeroVector;
		}
		return Input.SizeSquared() > 1.0 ? Input.GetSafeNormal() : Input;
	}

	double SideOf(double SideSign)
	{
		return SideSign < 0.0 ? -1.0 : 1.0;
	}

	/** 固定の刻みの 1 ステップ（SimDt）。AdvanceFrame と PredictPath が同じものを使う（予測が本番とずれない） */
	void StepOnce(FMovementState& State, FVector2D& Position, TOptional<FAutoMoveTarget>& AutoMove, const FVector2D& Input, double SideSign,
		const FMovementModifiers& Modifiers, const FMovementParams& Params)
	{
		const FVector2D StepInput = AutoMove.IsSet() ? TennisMovement::AutoMoveInput(Position, *AutoMove, SideSign, Params) : Input;
		State = TennisMovement::Step(State, StepInput, SideSign, Modifiers, Params, TennisConst::SimDt);
		Position += State.Velocity * TennisConst::SimDt;
		TennisMovement::ClampToArea(Position, State.Velocity, SideSign);
		if (AutoMove.IsSet())
		{
			AutoMove->TimeLeft -= TennisConst::SimDt;
		}
	}
}

bool TennisMovement::IsBackpedal(const FVector2D& Dir, double SideSign)
{
	return FVector2D::DotProduct(Dir, FVector2D(SideOf(SideSign), 0.0)) >= BackpedalCos;
}

FMovementParams TennisMovement::MakeParams(double SpeedAttribute, double AccelerationAttribute)
{
	const double Speed = FMath::Clamp(FMath::IsFinite(SpeedAttribute) ? SpeedAttribute : 50.0, 0.0, 100.0) / 100.0;
	const double Acceleration = FMath::Clamp(FMath::IsFinite(AccelerationAttribute) ? AccelerationAttribute : 50.0, 0.0, 100.0) / 100.0;
	FMovementParams Params;
	Params.MaxSpeed = FMath::Lerp(MaxSpeedAt0, MaxSpeedAt100, Speed);
	Params.Accel = FMath::Lerp(AccelAt0, AccelAt100, Acceleration);
	Params.Brake = FMath::Lerp(BrakeAt0, BrakeAt100, Acceleration);
	return Params;
}

FMovementState TennisMovement::Step(const FMovementState& State, FVector2D Input, double SideSign, const FMovementModifiers& Modifiers,
	const FMovementParams& Params, double Dt)
{
	if (!(Dt > 0.0))  // 0・負・NaN は進めない
	{
		return State;
	}

	FMovementState Next = State;
	Input = SanitizeInput(Input);
	const double InputSize = Input.Size();
	const FVector2D InputDir = InputSize > UE_DOUBLE_SMALL_NUMBER ? Input / InputSize : FVector2D::ZeroVector;

	// 方向転換：入力の向きと速度の向きが 90° を超えた瞬間に、速さを失う（1 回の方向転換につき 1 回）
	if (InputSize > UE_DOUBLE_SMALL_NUMBER && Next.Velocity.SizeSquared() > UE_DOUBLE_SMALL_NUMBER)
	{
		const bool bReversing = FVector2D::DotProduct(Next.Velocity, InputDir) < 0.0;
		if (bReversing && !Next.bTurnLossTaken)
		{
			Next.Velocity *= Params.TurnKeep;
		}
		Next.bTurnLossTaken = bReversing;
	}

	// 目標の速度と、それへ近づける割合
	const bool bBackpedal = IsBackpedal(InputDir, SideSign);
	double MaxSpeed = Params.MaxSpeed * InputSize;
	MaxSpeed *= bBackpedal ? Params.BackpedalSpeedFactor : 1.0;
	MaxSpeed *= Modifiers.bDash ? Params.DashSpeedFactor : 1.0;
	MaxSpeed *= Modifiers.bFollowThrough ? Params.FollowThroughSpeedFactor : 1.0;
	const FVector2D Desired = InputDir * MaxSpeed;

	const bool bSpeedingUp = Desired.Size() >= Next.Velocity.Size();
	double Rate = bSpeedingUp ? Params.Accel : Params.Brake;
	if (bSpeedingUp)
	{
		Rate *= Next.BoostTimeLeft > 0.0 ? Params.BoostAccelFactor : 1.0;
		Rate *= bBackpedal ? Params.BackpedalAccelFactor : 1.0;
	}

	const FVector2D Change = Desired - Next.Velocity;
	const double MaxChange = Rate * Dt;
	Next.Velocity = Change.Size() <= MaxChange ? Desired : Next.Velocity + Change.GetSafeNormal() * MaxChange;
	Next.BoostTimeLeft = FMath::Max(Next.BoostTimeLeft - Dt, 0.0);
	return Next;
}

FVector2D TennisMovement::AdvanceFrame(FMovementStepper& Stepper, const FVector2D& Position, FVector2D Input, double SideSign,
	const FMovementModifiers& Modifiers, const FMovementParams& Params, double FrameTime, double TimeScale)
{
	if (!(FrameTime > 0.0))
	{
		return Stepper.State.Velocity;
	}
	FVector2D StepPosition = Position - Stepper.Lead;  // 最後のステップのモデルの位置
	const int32 Steps = Stepper.Clock.Advance(FrameTime, TimeScale);
	for (int32 Index = 0; Index < Steps; ++Index)
	{
		StepOnce(Stepper.State, StepPosition, Stepper.AutoMove, Input, SideSign, Modifiers, Params);
	}
	// 外に渡す位置：まだステップにしていない時間の分だけ今の速度で先へ（範囲に収める。モデルの状態は変えない）
	FVector2D Target = StepPosition + Stepper.State.Velocity * Stepper.Clock.Remainder();
	FVector2D Unused = Stepper.State.Velocity;
	ClampToArea(Target, Unused, SideSign);
	Stepper.Lead = Target - StepPosition;
	return (Target - Position) / FrameTime;  // 外はこの速度で FrameTime 秒動く
}

TArray<FVector2D> TennisMovement::PredictPath(FMovementState State, FVector2D Position, TOptional<FAutoMoveTarget> AutoMove, FVector2D Input,
	double SideSign, const FMovementModifiers& Modifiers, const FMovementParams& Params, TConstArrayView<double> Times)
{
	const auto ClampTime = [](double Time) { return FMath::IsFinite(Time) ? FMath::Clamp(Time, 0.0, MaxPredictTime) : 0.0; };
	double Horizon = 0.0;
	for (const double Time : Times)
	{
		Horizon = FMath::Max(Horizon, ClampTime(Time));
	}
	const int32 Steps = FMath::CeilToInt32(Horizon / TennisConst::SimDt - 1e-9);  // 割り切れる時刻で 1 つ余分に進めない
	TArray<FVector2D> Path;  // ステップごとのモデルの位置（0 番は今）
	Path.Reserve(Steps + 1);
	Path.Add(Position);
	for (int32 Index = 0; Index < Steps; ++Index)
	{
		StepOnce(State, Position, AutoMove, Input, SideSign, Modifiers, Params);
		Path.Add(Position);
	}
	TArray<FVector2D> Out;
	Out.Reserve(Times.Num());
	for (const double Time : Times)
	{
		const double At = ClampTime(Time) / TennisConst::SimDt;
		const int32 Below = FMath::Min(FMath::FloorToInt32(At), Path.Num() - 1);
		const int32 Above = FMath::Min(Below + 1, Path.Num() - 1);
		Out.Add(FMath::Lerp(Path[Below], Path[Above], FMath::Clamp(At - Below, 0.0, 1.0)));
	}
	return Out;
}

FVector2D TennisMovement::AutoMoveInput(const FVector2D& Position, const FAutoMoveTarget& AutoMove, double SideSign, const FMovementParams& Params)
{
	const FVector2D ToTarget = AutoMove.Target - Position;
	const double Distance = ToTarget.Size();
	if (!FMath::IsFinite(Distance) || Distance <= ArriveTolerance)
	{
		return FVector2D::ZeroVector;  // 着いた（または数でない目標）：止まる
	}
	const FVector2D Dir = ToTarget / Distance;
	const bool bBackpedal = IsBackpedal(Dir, SideSign);
	const double MaxSpeed = Params.MaxSpeed * (bBackpedal ? Params.BackpedalSpeedFactor : 1.0);
	const double Accel = Params.Accel * (bBackpedal ? Params.BackpedalAccelFactor : 1.0);
	if (!(MaxSpeed > 0.0) || !(Accel > 0.0) || !(Params.Brake > 0.0))
	{
		return FVector2D::ZeroVector;
	}
	// 間に合う最小の速さ u：止まった状態から、加速 → 速さ u → 減速 で、距離を使える時間 T で進む（u² · C − u · T + 距離 = 0 の小さい解。
	// C = 1/(2·加速) + 1/(2·減速)）。止まった状態からと見るので、実際は少し早く着く。解がない（間に合わない）ときは最高速度
	const double Available = AutoMove.TimeLeft - AutoMove.SettleTime;
	const double C = 0.5 / Accel + 0.5 / Params.Brake;
	const double Discriminant = Available * Available - 4.0 * C * Distance;
	const double Needed = Available > TennisConst::SimDt && Discriminant >= 0.0 ? (Available - FMath::Sqrt(Discriminant)) / (2.0 * C) : MaxSpeed;
	const double Stoppable = FMath::Sqrt(2.0 * Params.Brake * Distance);  // ここから減速して、目標で止まれる速さ
	return Dir * (FMath::Min3(Needed, Stoppable, MaxSpeed) / MaxSpeed);
}

void TennisMovement::StartBoost(FMovementState& State, const FMovementParams& Params)
{
	State.BoostTimeLeft = Params.BoostSeconds;
}

void TennisMovement::ClampToArea(FVector2D& Position, FVector2D& Velocity, double SideSign)
{
	const double Side = SideOf(SideSign);
	const double Near = NetGap + BodyRadius;                       // ネットに近い端（|X|）
	const double Far = TennisConst::HalfLength + BehindBaseline;    // ベースラインの後ろの端（|X|）
	const double Wide = TennisConst::HalfWidthDoubles + OutsideDoublesSideline;

	// 自分の側の向きにした X（ネットから離れるほど大きい）で収める
	const double Depth = Position.X * Side;
	if (Depth < Near || Depth > Far)
	{
		Position.X = FMath::Clamp(Depth, Near, Far) * Side;
		const bool bMovingOut = (Depth < Near && Velocity.X * Side < 0.0) || (Depth > Far && Velocity.X * Side > 0.0);
		Velocity.X = bMovingOut ? 0.0 : Velocity.X;
	}
	if (FMath::Abs(Position.Y) > Wide)
	{
		Position.Y = FMath::Clamp(Position.Y, -Wide, Wide);
		Velocity.Y = Velocity.Y * Position.Y > 0.0 ? 0.0 : Velocity.Y;
	}
}
