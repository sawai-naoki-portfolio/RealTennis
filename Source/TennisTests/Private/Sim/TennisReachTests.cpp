// W-3.3：到達時間（DD-03 §4.1、T-STRK-007）。見積もりは、移動のモデル（TennisMovement::Step）を 240 Hz で進めた結果と合う。
#include "CQTest.h"
#include "Reach/TennisReach.h"
#include "TennisConstants.h"

namespace
{
	/** Step を固定ステップで進め、目標の向きに Distance 進むまでの時間（位置は新しい速度で進める：CMC と同じ） */
	double SimulatedTime(const FReachQuery& Q, const FVector2D& Target, const FMovementParams& M)
	{
		const FVector2D Dir = (Target - Q.Position).GetSafeNormal();
		const double Distance = (Target - Q.Position).Size();
		FMovementState State;
		State.Velocity = Q.Velocity;
		State.BoostTimeLeft = Q.BoostTimeLeft;
		FVector2D Position = Q.Position;
		double Time = 0.0;
		while (FVector2D::DotProduct(Position - Q.Position, Dir) < Distance && Time < 10.0)
		{
			State = TennisMovement::Step(State, Dir, Q.SideSign, FMovementModifiers(), M, TennisConst::SimDt);
			Position += State.Velocity * TennisConst::SimDt;
			Time += TennisConst::SimDt;
		}
		return Time;
	}

	FReachQuery Player(const FVector2D& Position, const FVector2D& Velocity = FVector2D::ZeroVector, double Boost = 0.0)
	{
		FReachQuery Q;
		Q.Position = Position;
		Q.Velocity = Velocity;
		Q.BoostTimeLeft = Boost;
		Q.SideSign = -1.0;  // X < 0 の側。ネットは +X、後退は −X
		return Q;
	}
}

TEST_CLASS(TennisReachTime, "Tennis.Sim.Reach.TimeToReach")
{
	const FMovementParams Params;
	const FVector2D Start{ -11.0, 0.0 };

	void ExpectMatchesSimulation(const FReachQuery& Q, const FVector2D& Target, const TCHAR* What)
	{
		const double Model = TennisReach::TimeToReach(Q, Target, Params);
		const double Simulated = SimulatedTime(Q, Target, Params);
		ASSERT_THAT(IsNear(Simulated, Model, TennisConst::SimDt + 0.01 * Simulated, What));
	}

	// T-STRK-007：静止から横へ 2〜8 m
	TEST_METHOD(FromRest_MatchesTheMovementModel)
	{
		for (const double Distance : { 2.0, 4.0, 6.0, 8.0 })
		{
			ExpectMatchesSimulation(Player(Start), Start + FVector2D(0.0, Distance), *FString::Printf(TEXT("%.0f m"), Distance));
		}
	}

	// T-STRK-007：目標の向きの初速・逆向きの初速（方向転換の損失）・初動ブースト・後退・後退で最高速度を超えた初速
	TEST_METHOD(InitialSpeedBoostAndBackpedal_MatchTheMovementModel)
	{
		const FVector2D Side(0.0, 4.0);
		ExpectMatchesSimulation(Player(Start, FVector2D(0.0, 3.0)), Start + Side, TEXT("toward"));
		ExpectMatchesSimulation(Player(Start, FVector2D(0.0, -3.0)), Start + Side, TEXT("reversing"));
		ExpectMatchesSimulation(Player(Start, FVector2D::ZeroVector, Params.BoostSeconds), Start + Side, TEXT("boost"));
		ExpectMatchesSimulation(Player(Start), Start + FVector2D(-4.0, 0.0), TEXT("backpedal"));
		ExpectMatchesSimulation(Player(Start, FVector2D(-Params.MaxSpeed, 0.0)), Start + FVector2D(-6.0, 0.0), TEXT("backpedal over speed"));
	}

	// T-MOVE-001 と同じ条件（静止から横へ 4 m、能力値 50）は 1.0〜1.15 s。初動ブーストがあれば短くなる
	TEST_METHOD(SideStep4m_IsWithinTheMovementTarget)
	{
		const double Time = TennisReach::TimeToReach(Player(Start), Start + FVector2D(0.0, 4.0), Params);
		ASSERT_THAT(IsTrue(Time >= 1.0 && Time <= 1.15));
		ASSERT_THAT(IsTrue(TennisReach::TimeToReach(Player(Start, FVector2D::ZeroVector, Params.BoostSeconds), Start + FVector2D(0.0, 4.0), Params) < Time));
	}

	// 同じ位置は 0。数でない入力は到達できない（最大値）
	TEST_METHOD(SamePointAndInvalidInput)
	{
		ASSERT_THAT(IsNear(0.0, TennisReach::TimeToReach(Player(Start), Start, Params), 1e-12));
		const double NaN = std::numeric_limits<double>::quiet_NaN();
		ASSERT_THAT(IsTrue(TennisReach::TimeToReach(Player(FVector2D(NaN, 0.0)), Start, Params) == TNumericLimits<double>::Max()));
		ASSERT_THAT(IsTrue(TennisReach::TimeToReach(Player(Start), FVector2D(0.0, NaN), Params) == TNumericLimits<double>::Max()));
	}
};
