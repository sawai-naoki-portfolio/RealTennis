// W-3.1：選手の水平の移動（DD-03 §2）。T-MOVE-001〜003。固定の時間刻み（240 Hz）で確かめる。
#include "CQTest.h"
#include "Movement/TennisMovement.h"
#include "TennisConstants.h"

namespace
{
	constexpr double Dt = TennisConst::SimDt;
	constexpr double Near = TennisMovement::NetGap + TennisMovement::BodyRadius;  // カプセルの中心が近づける |X|

	// 静止から入力 Input で進み、Distance m 進むまでの時間（s）。届かなければ負
	double TimeToCover(FMovementState State, FVector2D Input, double Distance, double MaxSeconds = 3.0)
	{
		FVector2D Position(-8.0, 0.0);
		const FVector2D Start = Position;
		for (double Time = 0.0; Time < MaxSeconds; Time += Dt)
		{
			State = TennisMovement::Step(State, Input, -1.0, FMovementModifiers(), FMovementParams(), Dt);
			Position += State.Velocity * Dt;
			if (FVector2D::Distance(Position, Start) >= Distance)
			{
				return Time + Dt;
			}
		}
		return -1.0;
	}

	// 入力を続けたときの最高速度
	double TopSpeed(FVector2D Input, const FMovementModifiers& Modifiers)
	{
		FMovementState State;
		for (int32 Step = 0; Step < 480; ++Step)
		{
			State = TennisMovement::Step(State, Input, -1.0, Modifiers, FMovementParams(), Dt);
		}
		return State.Velocity.Size();
	}
}

TEST_CLASS(Movement, "Tennis.Sim.Movement")
{
	// T-MOVE-001：静止から 4 m の横移動の時間（能力値 50）は 1.0〜1.15 s
	TEST_METHOD(SideStep4m_FromRest_Takes1To1_15s)
	{
		const double Time = TimeToCover(FMovementState(), FVector2D(0.0, 1.0), 4.0);
		ASSERT_THAT(IsTrue(Time >= 1.0 && Time <= 1.15));
	}

	// T-MOVE-002：良いスプリットステップの後（初動ブースト）の 4 m は 0.92〜1.05 s
	TEST_METHOD(SideStep4m_AfterGoodSplitStep_Takes0_92To1_05s)
	{
		FMovementState State;
		TennisMovement::StartBoost(State, FMovementParams());
		const double Time = TimeToCover(State, FVector2D(0.0, 1.0), 4.0);
		ASSERT_THAT(IsTrue(Time >= 0.92 && Time <= 1.05));
	}

	// T-MOVE-003：ネットへ走っても、体はネットの面の 0.15 m 手前で止まり、越えない（どちらの側でも）
	TEST_METHOD(RunningAtTheNet_StopsJustShortOfIt)
	{
		for (const double Side : { -1.0, 1.0 })
		{
			FMovementState State;
			FVector2D Position(3.0 * Side, 1.0);  // 自分の側（X の符号が Side）の、ネットから 3 m
			const FVector2D TowardNet(-Side, 0.0);
			for (int32 Step = 0; Step < 480; ++Step)
			{
				State = TennisMovement::Step(State, TowardNet, Side, FMovementModifiers(), FMovementParams(), Dt);
				Position += State.Velocity * Dt;
				TennisMovement::ClampToArea(Position, State.Velocity, Side);
				ASSERT_THAT(IsTrue(Position.X * Side >= Near - 1e-12));
			}
			ASSERT_THAT(IsNear(Near * Side, Position.X, 1e-12));
			ASSERT_THAT(IsNear(0.0, State.Velocity.X, 1e-12));
		}
	}

	// 範囲の外（ベースラインの後ろ 6 m・サイドラインの外 3.5 m）にも出ない。内側へ向かう速度は残す
	TEST_METHOD(ClampToArea_KeepsThePlayerInsideTheirArea)
	{
		FVector2D Position(-30.0, 9.5);
		FVector2D Velocity(-2.0, 1.0);
		TennisMovement::ClampToArea(Position, Velocity, -1.0);
		ASSERT_THAT(IsNear(-(TennisConst::HalfLength + TennisMovement::BehindBaseline), Position.X, 1e-12));
		ASSERT_THAT(IsNear(TennisConst::HalfWidthDoubles + TennisMovement::OutsideDoublesSideline, Position.Y, 1e-12));
		ASSERT_THAT(IsTrue(Velocity.IsNearlyZero()));

		FVector2D Inside(0.1, 0.0);
		FVector2D Leaving(-3.0, 0.0);  // ネットから離れる向きは残す
		TennisMovement::ClampToArea(Inside, Leaving, -1.0);
		ASSERT_THAT(IsNear(-Near, Inside.X, 1e-12));
		ASSERT_THAT(IsNear(-3.0, Leaving.X, 1e-12));
	}

	// 90° を超える方向転換は、その瞬間に速さの 35% を失う。1 回の方向転換につき 1 回だけ
	TEST_METHOD(Reversal_LosesSpeedOncePerTurn)
	{
		FMovementState State;
		State.Velocity = FVector2D(0.0, 5.5);
		const FMovementState Turned = TennisMovement::Step(State, FVector2D(0.0, -1.0), -1.0, FMovementModifiers(), FMovementParams(), Dt);
		ASSERT_THAT(IsTrue(Turned.bTurnLossTaken));
		ASSERT_THAT(IsNear(5.5 * 0.65 - 8.0 * Dt, Turned.Velocity.Y, 1e-9));

		const FMovementState Next = TennisMovement::Step(Turned, FVector2D(0.0, -1.0), -1.0, FMovementModifiers(), FMovementParams(), Dt);
		ASSERT_THAT(IsNear(Turned.Velocity.Y - 8.0 * Dt, Next.Velocity.Y, 1e-9));  // 2 回目の損失はない

		const FMovementState Along = TennisMovement::Step(State, FVector2D(1.0, 1.0), -1.0, FMovementModifiers(), FMovementParams(), Dt);
		ASSERT_THAT(IsFalse(Along.bTurnLossTaken));  // 90° 以下の変更は損失なし
	}

	// 最高速度：後退 ×0.75、ダッシュ ×1.08、フォロースルー ×0.40。スティックの倒し具合に比例する
	TEST_METHOD(TopSpeed_FollowsModifiersAndStickTilt)
	{
		ASSERT_THAT(IsNear(5.5, TopSpeed(FVector2D(0.0, 1.0), FMovementModifiers()), 1e-9));
		ASSERT_THAT(IsNear(5.5 * 0.75, TopSpeed(FVector2D(-1.0, 0.0), FMovementModifiers()), 1e-9));  // X < 0 の側でネットから離れる
		ASSERT_THAT(IsNear(5.5, TopSpeed(FVector2D(1.0, 0.0), FMovementModifiers()), 1e-9));           // ネットへ向かう
		FMovementModifiers Dash;
		Dash.bDash = true;
		ASSERT_THAT(IsNear(5.5 * 1.08, TopSpeed(FVector2D(0.0, 1.0), Dash), 1e-9));
		FMovementModifiers FollowThrough;
		FollowThrough.bFollowThrough = true;
		ASSERT_THAT(IsNear(5.5 * 0.40, TopSpeed(FVector2D(0.0, 1.0), FollowThrough), 1e-9));
		ASSERT_THAT(IsNear(5.5 * 0.5, TopSpeed(FVector2D(0.0, 0.5), FMovementModifiers()), 1e-9));
	}

	// 入力を離すと減速（10 m/s²）で止まる
	TEST_METHOD(NoInput_BrakesToAStop)
	{
		FMovementState State;
		State.Velocity = FVector2D(0.0, 5.0);
		const FMovementState Next = TennisMovement::Step(State, FVector2D::ZeroVector, -1.0, FMovementModifiers(), FMovementParams(), Dt);
		ASSERT_THAT(IsNear(5.0 - 10.0 * Dt, Next.Velocity.Y, 1e-9));
		ASSERT_THAT(IsTrue(TopSpeed(FVector2D::ZeroVector, FMovementModifiers()) == 0.0));
	}

	// 堅牢性：長すぎる入力は長さ 1、数でない入力は 0、正でない時間刻みは何もしない
	TEST_METHOD(InvalidInputs_AreSanitized)
	{
		ASSERT_THAT(IsNear(5.5, TopSpeed(FVector2D(0.0, 3.0), FMovementModifiers()), 1e-9));
		ASSERT_THAT(IsTrue(TopSpeed(FVector2D(std::numeric_limits<double>::quiet_NaN(), 1.0), FMovementModifiers()) == 0.0));
		FMovementState State;
		State.Velocity = FVector2D(1.0, 2.0);
		for (const double BadDt : { 0.0, -Dt, std::numeric_limits<double>::quiet_NaN() })
		{
			const FMovementState Same = TennisMovement::Step(State, FVector2D(0.0, 1.0), -1.0, FMovementModifiers(), FMovementParams(), BadDt);
			ASSERT_THAT(IsTrue(Same.Velocity == State.Velocity));
		}
	}

	// FR-MOVE-001：能力値 50 で 5.5 m/s・8 m/s²・10 m/s²。0 と 100 は DD-03 §2.1 の端の値、範囲の外は収める
	TEST_METHOD(MakeParams_MapsAttributes)
	{
		const FMovementParams Mid = TennisMovement::MakeParams(50.0, 50.0);
		ASSERT_THAT(IsNear(5.5, Mid.MaxSpeed, 1e-12));
		ASSERT_THAT(IsNear(8.0, Mid.Accel, 1e-12));
		ASSERT_THAT(IsNear(10.0, Mid.Brake, 1e-12));
		const FMovementParams Low = TennisMovement::MakeParams(-10.0, 0.0);
		ASSERT_THAT(IsNear(4.8, Low.MaxSpeed, 1e-12));
		ASSERT_THAT(IsNear(6.5, Low.Accel, 1e-12));
		const FMovementParams High = TennisMovement::MakeParams(100.0, 250.0);
		ASSERT_THAT(IsNear(6.2, High.MaxSpeed, 1e-12));
		ASSERT_THAT(IsNear(11.5, High.Brake, 1e-12));
		ASSERT_THAT(IsNear(FMovementParams().MaxSpeed, Mid.MaxSpeed, 1e-12));  // 既定は能力値 50（DRY）
	}
};
