// W-1.7：環境から空気の性質を求める・風と突風。T-BALL-040・009・041（DD-01 §10、RES-02 §2.4・§6）。
#include "CQTest.h"
#include "TennisSimTypes.h"
#include "TennisTestHelpers.h"

#include <limits>

namespace
{
	// 最初のバウンドの位置（バウンドしなければ非常に大きい値）
	FVector FirstBouncePos(FBallState State, const FBallSimContext& Ctx)
	{
		const TArray<FBallEvent> Events = TennisTest::FlyToFirstBounce(State, Ctx);
		const FBallEvent* Bounce = TennisTest::FirstOf(Events, EBallEventType::Bounce);
		return Bounce != nullptr ? Bounce->Pos : FVector(TNumericLimits<double>::Max());
	}

	// 参照シミュレーションで、海面（ρ = 1.21）で 9.4 m に落ちるように解いたラリーの球（Tools/Reference の rho の実験）
	double LandingX(double Rho)
	{
		FBallSimContext Ctx;
		Ctx.Air.Rho = Rho;
		FBallState State;
		State.Pos = FVector(-13.0, 0.0, 0.95);
		State.Vel = FVector(34.254232534892985, 0.0, 5.544145871590946);
		State.Spin = FVector(0.0, 240.85543677521747, 0.0);
		return FirstBouncePos(State, Ctx).X;
	}

	// ベースラインからのトップスピンのラリーの球の、最初のバウンドの位置
	FVector RallyFirstBounce(const FAirParams& Air)
	{
		FBallSimContext Ctx;
		Ctx.Air = Air;
		return FirstBouncePos(TennisTest::RallyShot(), Ctx);
	}

	FMatchConditions Isa(double AltitudeM, double TempC)
	{
		FMatchConditions Conditions;
		Conditions.AltitudeM = AltitudeM;
		Conditions.TemperatureC = TempC;
		return Conditions;
	}
}

TEST_CLASS(Environment, "Tennis.Sim.Env")
{
	// T-BALL-040：標高・気温から求めた ρ が DD-01 §10 の式（ISA）と一致する
	TEST_METHOD(AirDensity_MatchesIsaFormula)
	{
		ASSERT_THAT(IsNear(1.2250, FMatchConditions::AirDensity(0.0, 15.0), 0.0005));      // ISA の海面
		ASSERT_THAT(IsNear(1.0581, FMatchConditions::AirDensity(1500.0, 5.25), 0.0005));   // ISA の 1,500 m
		ASSERT_THAT(IsNear(0.9569, FMatchConditions::AirDensity(2500.0, -1.25), 0.0005));  // ISA の 2,500 m
		ASSERT_THAT(IsNear(FMatchConditions::AirDensity(1500.0, 5.25), Isa(1500.0, 5.25).ComputeAir().Rho, 1e-12));
	}

	// 既定の環境（海面・20 ℃）の ρ は、FAirParams の既定値（DATA-02 §1 の 1.204）と同じ
	TEST_METHOD(DefaultConditions_MatchDefaultAir)
	{
		ASSERT_THAT(IsNear(FAirParams().Rho, FMatchConditions().ComputeAir().Rho, 0.0005));
	}

	// T-BALL-009：標高 1,500 m（ISA）では、同じ打ち出しの着地点が +1.0〜+1.5 m 深くなる（参照 +1.24 m）
	TEST_METHOD(HighAltitude_LandsDeeper)
	{
		const double SeaLevel = LandingX(Isa(0.0, 15.0).ComputeAir().Rho);
		const double Altitude = LandingX(Isa(1500.0, 5.25).ComputeAir().Rho);
		ASSERT_THAT(IsTrue(Altitude - SeaLevel >= 1.0 && Altitude - SeaLevel <= 1.5));
	}

	// 屋根を閉じれば、気温は 21 ℃、風は 0
	TEST_METHOD(RoofClosed_IsIndoorAndCalm)
	{
		FMatchConditions Conditions = Isa(0.0, 35.0);
		Conditions.WindMean = FVector(3.0, 1.0, 0.0);
		Conditions.bRoofClosed = true;
		ASSERT_THAT(IsNear(FMatchConditions::IndoorTempC, Conditions.EffectiveTempC(), 1e-12));
		const FAirParams Air = Conditions.ComputeAir();
		ASSERT_THAT(IsTrue(Air.WindMean.IsZero()));
		ASSERT_THAT(IsNear(FMatchConditions::AirDensity(0.0, FMatchConditions::IndoorTempC), Air.Rho, 1e-12));
	}

	// T-BALL-041：横風 3 m/s で、ラリーの球が 0.2〜0.8 m 流される
	TEST_METHOD(Crosswind_DriftsRallyBall)
	{
		FAirParams Air;
		Air.WindMean = FVector(0.0, 3.0, 0.0);
		const FVector Bounce = RallyFirstBounce(Air);
		ASSERT_THAT(IsTrue(Bounce.Y >= 0.2 && Bounce.Y <= 0.8));
		ASSERT_THAT(IsNear(0.0, RallyFirstBounce(FAirParams()).Y, 1e-12));
	}

	// DD-01 §10：突風は平均の風の ±G 倍の中で、時刻とともに変わる。突風がなければ平均の風のまま
	TEST_METHOD(Gusts_VaryWithinTheirStrength)
	{
		FMatchConditions Conditions;
		Conditions.WindMean = FVector(4.0, 0.0, 0.0);
		const FAirParams Calm = Conditions.ComputeAir(7);
		Conditions.GustFactor = 0.5;
		const FAirParams Gusty = Conditions.ComputeAir(7);

		double MaxDeviation = 0.0;
		for (double Time = 0.0; Time < 20.0; Time += 0.1)
		{
			ASSERT_THAT(IsTrue(Calm.WindAt(Time).Equals(Conditions.WindMean, 1e-12)));
			const double Deviation = (Gusty.WindAt(Time) - Conditions.WindMean).Size();
			ASSERT_THAT(IsTrue(Deviation <= 0.5 * 4.0 + 1e-9));
			MaxDeviation = FMath::Max(MaxDeviation, Deviation);
		}
		ASSERT_THAT(IsTrue(MaxDeviation > 0.2 * 4.0));
	}

	// ARCH-03 D-2：突風の位相はポイントのシードで決まる（同じシードなら同じ、違えば違う）
	TEST_METHOD(GustPhases_AreSeeded)
	{
		FMatchConditions Conditions;
		const FAirParams A = Conditions.ComputeAir(7);
		const FAirParams B = Conditions.ComputeAir(7);
		const FAirParams C = Conditions.ComputeAir(8);
		ASSERT_THAT(IsTrue(A.GustPhase1 == B.GustPhase1 && A.GustPhase2 == B.GustPhase2));
		ASSERT_THAT(IsTrue(A.GustPhase1 != C.GustPhase1 || A.GustPhase2 != C.GustPhase2));
	}

	// 堅牢性：NaN は既定値、範囲の外は丸める
	TEST_METHOD(InvalidConditions_AreSanitized)
	{
		const double NaN = std::numeric_limits<double>::quiet_NaN();
		FMatchConditions Bad = Isa(NaN, NaN);
		Bad.WindMean = FVector(NaN, 0.0, 0.0);
		const FAirParams Air = Bad.ComputeAir();
		ASSERT_THAT(IsNear(FMatchConditions().ComputeAir().Rho, Air.Rho, 1e-12));
		ASSERT_THAT(IsTrue(Air.WindMean.IsZero()));

		ASSERT_THAT(IsNear(FMatchConditions::MaxTempC, Isa(0.0, 80.0).EffectiveTempC(), 1e-12));
		FMatchConditions Strong;
		Strong.GustFactor = 0.9;
		ASSERT_THAT(IsNear(FMatchConditions::MaxGustFactor, Strong.ComputeAir().GustFactor, 1e-12));
		Strong.GustFactor = NaN;
		ASSERT_THAT(IsNear(0.0, Strong.ComputeAir().GustFactor, 1e-12));
		ASSERT_THAT(IsNear(FMatchConditions::AirDensity(FMatchConditions::MaxAltitudeM, 20.0), Isa(9000.0, 20.0).ComputeAir().Rho, 1e-12));
	}
};
