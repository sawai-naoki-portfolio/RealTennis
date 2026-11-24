#include "Ball/TennisBallPhysics.h"

#include "NetCordContact.h"
#include "NetPlaneContact.h"
#include "NetUprightContact.h"
#include "TennisConstants.h"
#include "TennisSimLog.h"

namespace
{
	// これより遅い相対速度では、空気の力を 0 とする（0 で割らないため。この速さでの力は 1e-12 m/s² 未満）
	constexpr double MinAeroSpeed = 1e-6;  // m/s

	// 法線方向の速さがこれ未満の接地は弾ませず、地面に沿わせる（跳ね上がりは 0.5 mm 未満。DD-01 §4.1）
	constexpr double MinBounceSpeed = 0.1;  // m/s

	// 1 ステップの中で解く衝突の最大の回数（これを超えたら、残りは衝突を解かずに進める。無限のループを防ぐ）
	constexpr int32 MaxContactsPerStep = 4;

	enum class EContactKind : uint8 { Ground, NetCord, NetUpright, NetPlane };

	struct FContact
	{
		EContactKind Kind;
		double Fraction;  // Prev → Next のうち、衝突までの割合（0〜1）
	};

	// 地面：ステップの終わりに中心が R より下で、下へ動いていれば接地。時刻は線形補間（ACT-01）。
	// すでに地面より下から始まったなら、ステップの始めで接地する
	TOptional<double> GroundContactFraction(const FBallState& Prev, const FBallState& Next, double Radius)
	{
		if (Next.Pos.Z >= Radius || Next.Vel.Z >= 0.0)
		{
			return {};
		}
		const double Above = Prev.Pos.Z - Radius;
		return Above > 0.0 ? FMath::Clamp(Above / (Prev.Pos.Z - Next.Pos.Z), 0.0, 1.0) : 0.0;
	}

	// ネット（X = 0）の白帯・ポストに触れうるか：ステップの間に X = 0 をまたぐか、どちらかの端が触れうる距離にある
	bool IsNearNet(const FBallState& Prev, const FBallState& Next, double Radius)
	{
		const double Reach = Radius + TennisConst::PostRadius + 0.01;  // m（いちばん太いポストの半径と余裕）
		const bool bCrosses = (Prev.Pos.X < 0.0) != (Next.Pos.X < 0.0) || Prev.Pos.X == 0.0 || Next.Pos.X == 0.0;
		return bCrosses || FMath::Min(FMath::Abs(Prev.Pos.X), FMath::Abs(Next.Pos.X)) < Reach;
	}

	// このステップで最初に起きる衝突（同時なら、地面・白帯・ポストとスティック・ネットの面の順）
	TOptional<FContact> FindFirstContact(const FBallState& Prev, const FBallState& Next, const FBallSimContext& Ctx)
	{
		TOptional<FContact> First;
		const auto Consider = [&First](EContactKind Kind, TOptional<double> Fraction)
		{
			if (Fraction.IsSet() && (!First.IsSet() || *Fraction < First->Fraction))
			{
				First = FContact{ Kind, *Fraction };
			}
		};
		Consider(EContactKind::Ground, GroundContactFraction(Prev, Next, Ctx.Ball.Radius));
		if (IsNearNet(Prev, Next, Ctx.Ball.Radius))  // ネットから遠いステップでは、ネットまわりを調べない（結果は同じで速い）
		{
			Consider(EContactKind::NetCord, TennisNetCordContact::ContactFraction(Prev, Next, Ctx));
			Consider(EContactKind::NetUpright, TennisNetUprightContact::ContactFraction(Prev, Next, Ctx));
			Consider(EContactKind::NetPlane, TennisNetPlaneContact::CrossingFraction(Prev, Next));
		}
		return First;
	}

	// 残りの時間を、衝突を解かずに進め、地面より下に入らないようにする（転がりの抵抗はまだ扱わない。DD-01 §4.1）
	void FinishAboveGround(FBallState& State, double Remaining, const FBallSimContext& Ctx)
	{
		TennisBallPhysics::IntegrateFlight(State, Remaining, Ctx);
		if (State.Pos.Z < Ctx.Ball.Radius)
		{
			State.Pos.Z = Ctx.Ball.Radius;
			State.Vel.Z = FMath::Max(State.Vel.Z, 0.0);
		}
	}

	// 地面に着いた状態を解く。弾まない（ゆっくりした接地）なら false
	bool ResolveGround(FBallState& State, const FBallSimContext& Ctx, TArray<FBallEvent>& OutEvents)
	{
		State.Pos.Z = Ctx.Ball.Radius;
		const double NormalSpeed = -State.Vel.Z;
		if (NormalSpeed < MinBounceSpeed)
		{
			return false;
		}
		TennisBallPhysics::ResolveGroundBounce(State, Ctx.Ball, Ctx.Surface, Ctx.TempC);
		FBallEvent& Event = OutEvents.AddDefaulted_GetRef();
		Event.Type = EBallEventType::Bounce;
		Event.Time = State.Time;
		Event.Pos = State.Pos;
		Event.Vel = State.Vel;
		Event.Spin = State.Spin;
		Event.NormalSpeed = NormalSpeed;
		Event.ContactPatch = Ctx.Surface.ContactPatch(NormalSpeed);
		return true;
	}
}

void TennisBallPhysics::Step(FBallState& State, double Dt, const FBallSimContext& Ctx, TArray<FBallEvent>& OutEvents)
{
	if (!(Dt >= 0.0))  // NaN も弾く
	{
		UE_LOG(LogTennisBall, Warning, TEXT("[Ball] Step: invalid Dt=%f (state unchanged)"), Dt);
		return;
	}

	double Remaining = Dt;
	for (int32 Contact = 0; Remaining > 0.0; ++Contact)
	{
		if (Contact >= MaxContactsPerStep)
		{
			FinishAboveGround(State, Remaining, Ctx);
			return;
		}

		const FBallState Prev = State;
		IntegrateFlight(State, Remaining, Ctx);
		const TOptional<FContact> First = FindFirstContact(Prev, State, Ctx);
		if (!First.IsSet())
		{
			return;  // このステップでは何にも当たらない
		}

		// 衝突の時刻まで戻して進め直し、そこで解く（ACT-01）
		const double ToContact = Remaining * First->Fraction;
		State = Prev;
		IntegrateFlight(State, ToContact, Ctx);
		Remaining -= ToContact;

		switch (First->Kind)
		{
		case EContactKind::Ground:
			if (!ResolveGround(State, Ctx, OutEvents))
			{
				FinishAboveGround(State, Remaining, Ctx);
				return;
			}
			break;
		case EContactKind::NetCord:
			TennisNetCordContact::Resolve(State, Ctx, OutEvents);
			break;
		case EContactKind::NetUpright:
			TennisNetUprightContact::Resolve(State, Ctx, OutEvents);
			break;
		case EContactKind::NetPlane:
			TennisNetPlaneContact::Resolve(State, Ctx, OutEvents);
			break;
		}
	}
}

bool TennisBallPhysics::AdvanceTo(FBallState& State, double TargetTime, const FBallSimContext& Ctx, TArray<FBallEvent>& OutEvents)
{
	const double Gap = TargetTime - State.Time;
	if (!(Gap >= 0.0 && Gap <= MaxAdvanceTime))  // NaN も弾く
	{
		UE_LOG(LogTennisBall, Warning, TEXT("[Ball] AdvanceTo: cannot advance from %f to %f (state unchanged)"), State.Time, TargetTime);
		return false;
	}

	const int32 FullSteps = FMath::FloorToInt32(Gap / TennisConst::SimDt);
	for (int32 Index = 0; Index < FullSteps; ++Index)
	{
		Step(State, TennisConst::SimDt, Ctx, OutEvents);
	}
	Step(State, FMath::Max(TargetTime - State.Time, 0.0), Ctx, OutEvents);  // 最後の端数
	State.Time = TargetTime;  // 丸めの誤差を残さない
	return true;
}

FVector TennisBallPhysics::ComputeAccel(const FVector& Vel, const FVector& Spin, const FBallParams& Ball, const FAirParams& Air, double Time)
{
	FVector Accel(0.0, 0.0, -TennisConst::Gravity);
	const FVector VRel = Vel - Air.WindAt(Time);
	const double Speed = VRel.Size();
	if (Speed < MinAeroSpeed)
	{
		return Accel;
	}

	const double S = Ball.Radius * Spin.Size() / Speed;                     // スピンパラメータ
	const double K = 0.5 * Air.Rho * Ball.CrossSectionArea() / Ball.Mass;  // 1/m（係数を掛ける前）

	const double Cd = Ball.Cd0 + Ball.CdSpin * S;
	Accel -= (K * Cd * Speed) * VRel;  // 抗力

	const double Cl = S / (1.0 + 2.0 * S);  // = 1 / (2 + 1/S)。S = 0 でも連続（RES-02 §2.2）
	const FVector Lift = FVector::CrossProduct(Spin, VRel);
	const double LiftSize = Lift.Size();
	if (LiftSize > UE_DOUBLE_SMALL_NUMBER)
	{
		Accel += (K * Cl * Speed * Speed / LiftSize) * Lift;  // マグヌス（ω × v_rel の向き）
	}
	return Accel;
}

void TennisBallPhysics::IntegrateFlight(FBallState& State, double Dt, const FBallSimContext& Ctx)
{
	if (!(Dt >= 0.0))  // NaN も弾く
	{
		UE_LOG(LogTennisBall, Warning, TEXT("[Ball] IntegrateFlight: invalid Dt=%f (state unchanged)"), Dt);
		return;
	}

	const FVector Spin = State.Spin;  // ステップの中では一定とする（参照のモデルと同じ）
	const double T0 = State.Time;
	const auto Accel = [&Spin, &Ctx](const FVector& V, double T) { return ComputeAccel(V, Spin, Ctx.Ball, Ctx.Air, T); };

	const FVector V0 = State.Vel;
	const FVector A1 = Accel(V0, T0);
	const FVector V1 = V0 + A1 * (0.5 * Dt);
	const FVector A2 = Accel(V1, T0 + 0.5 * Dt);
	const FVector V2 = V0 + A2 * (0.5 * Dt);
	const FVector A3 = Accel(V2, T0 + 0.5 * Dt);
	const FVector V3 = V0 + A3 * Dt;
	const FVector A4 = Accel(V3, T0 + Dt);

	State.Pos += (V0 + 2.0 * V1 + 2.0 * V2 + V3) * (Dt / 6.0);
	State.Vel += (A1 + 2.0 * A2 + 2.0 * A3 + A4) * (Dt / 6.0);
	State.Spin *= FMath::Exp(-Ctx.Ball.SpinDecayPerMeter * State.Vel.Size() * Dt);  // DD-01 §3.2
	State.Time += Dt;
}

void TennisBallPhysics::ResolveGroundBounce(FBallState& State, const FBallParams& Ball, const FSurfaceParams& Surface, double TempC)
{
	const FVector N(0.0, 0.0, 1.0);
	const double Vn = FVector::DotProduct(State.Vel, N);  // m/s（負なら接近中）
	if (Vn >= 0.0)
	{
		return;
	}

	const double E = Surface.RestitutionAt(-Vn, TempC) * (Ball.RestitutionHard / FBallParams::Type2RestitutionHard);
	const double Jn = -(1.0 + E) * Ball.Mass * Vn;  // N·s

	const FVector Rc = -Ball.Radius * N;                                  // 中心から接触点
	const FVector Vc = State.Vel + FVector::CrossProduct(State.Spin, Rc);  // 接触点の速度
	const FVector Vct = Vc - FVector::DotProduct(Vc, N) * N;              // 接触点の滑りの速度

	// 転がりに必要な撃力の (1 + e_t) 倍。上限は μ·J_n（越えたら滑る。境界は常に同じ式で比べる：ARCH-03 D-5）
	const double A = Ball.InertiaAlpha;
	FVector Jt = -Vct * (Ball.Mass * A / (1.0 + A) * (1.0 + Surface.TangentialRestitution));
	const double JtMax = Surface.Friction * Jn;
	if (Jt.Size() > JtMax)
	{
		Jt = Jt.GetSafeNormal() * JtMax;
	}

	State.Vel += (Jn * N + Jt) / Ball.Mass;
	State.Spin += FVector::CrossProduct(Rc, Jt) / Ball.MomentOfInertia();
	State.NumBounces++;
}
