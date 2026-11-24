// W-3.5、T-STRK-012：ストロークの同期の計算（DD-03 §6.1・§6.2）。始め方（待つ時間・開始位置・再生速度）、再生中の補正、
// 体の向き（rad。ネットの方向から ±35°）、体の目標位置。W-3.6、T-STRK-019：取り消した後に向きをネットへ戻す（§6.5）。
#include "CQTest.h"
#include "Reach/TennisReach.h"
#include "Stroke/TennisStrokeTiming.h"
#include "TennisConstants.h"

#include <limits>

namespace
{
	constexpr double ImpactTime = 23.0 / 30.0;  // 仮のストロークのインパクト（0.767 s）

	/** 計画どおりに再生したとき、インパクトまでの時間（s） */
	double TimeToImpact(const FStrokeStartPlan& Plan)
	{
		return Plan.Delay + (ImpactTime - Plan.StartPosition) / Plan.PlayRate;
	}
}

TEST_CLASS(StrokeTiming, "Tennis.Sim.Stroke")
{
	const FStrokeSyncParams Params;

	// 再生速度が範囲（0.80〜1.25）に収まる時間なら、先頭から、その速度で
	TEST_METHOD(PlanStart_UsesTheRateWhenItIsInRange)
	{
		for (const double Available : { ImpactTime / 1.25, 0.70, ImpactTime, 0.90, ImpactTime / 0.80 })
		{
			const FStrokeStartPlan Plan = TennisStrokeTiming::PlanStart(ImpactTime, Available, Params);
			ASSERT_THAT(IsNear(0.0, Plan.Delay, 1e-12));
			ASSERT_THAT(IsNear(0.0, Plan.StartPosition, 1e-12));
			ASSERT_THAT(IsTrue(Plan.PlayRate >= 0.80 - 1e-12 && Plan.PlayRate <= 1.25 + 1e-12));
			ASSERT_THAT(IsNear(Available, TimeToImpact(Plan), 1e-12));
		}
	}

	// 時間が足りなければ、上限の速度で途中から（テイクバックを省く）。余れば、下限の速度で開始を遅らせる。どちらもインパクトは合う
	TEST_METHOD(PlanStart_SkipsTheTakebackOrWaits)
	{
		const FStrokeStartPlan Short = TennisStrokeTiming::PlanStart(ImpactTime, 0.40, Params);
		ASSERT_THAT(IsNear(1.25, Short.PlayRate, 1e-12));
		ASSERT_THAT(IsNear(ImpactTime - 0.40 * 1.25, Short.StartPosition, 1e-12));
		ASSERT_THAT(IsNear(0.0, Short.Delay, 1e-12));
		ASSERT_THAT(IsNear(0.40, TimeToImpact(Short), 1e-12));

		const FStrokeStartPlan Long = TennisStrokeTiming::PlanStart(ImpactTime, 2.0, Params);
		ASSERT_THAT(IsNear(0.80, Long.PlayRate, 1e-12));
		ASSERT_THAT(IsNear(0.0, Long.StartPosition, 1e-12));
		ASSERT_THAT(IsNear(2.0 - ImpactTime / 0.80, Long.Delay, 1e-12));
		ASSERT_THAT(IsNear(2.0, TimeToImpact(Long), 1e-12));
	}

	// 打点の時刻を過ぎていれば、インパクトの位置から上限の速度。数でない入力は、待たずに先頭から等倍
	TEST_METHOD(PlanStart_LateOrInvalidInput)
	{
		for (const double Available : { 0.0, -0.3 })
		{
			const FStrokeStartPlan Late = TennisStrokeTiming::PlanStart(ImpactTime, Available, Params);
			ASSERT_THAT(IsTrue(Late.StartPosition == ImpactTime && Late.PlayRate == 1.25 && Late.Delay == 0.0));
		}
		const double NaN = std::numeric_limits<double>::quiet_NaN();
		for (const FStrokeStartPlan& Plan : { TennisStrokeTiming::PlanStart(0.0, 1.0, Params), TennisStrokeTiming::PlanStart(NaN, 1.0, Params),
			TennisStrokeTiming::PlanStart(ImpactTime, NaN, Params) })
		{
			ASSERT_THAT(IsTrue(Plan.Delay == 0.0 && Plan.StartPosition == 0.0 && Plan.PlayRate == 1.0));
		}
	}

	// 補正は 1 秒あたり ±0.5 まで、範囲の中で。インパクトを過ぎたら変えない
	TEST_METHOD(CorrectPlayRate_MovesTowardTheNeededRateWithinTheLimit)
	{
		// 位置 0.2 s で残り 0.567 s。0.5 s 後に合わせるには 1.133 倍。0.05 s で動かせるのは 0.025
		ASSERT_THAT(IsNear(1.025, TennisStrokeTiming::CorrectPlayRate(1.0, 0.2, ImpactTime, 0.5, 0.05, Params), 1e-12));
		ASSERT_THAT(IsNear(0.975, TennisStrokeTiming::CorrectPlayRate(1.0, 0.2, ImpactTime, 0.7, 0.05, Params), 1e-12));
		ASSERT_THAT(IsNear((ImpactTime - 0.2) / 0.56, TennisStrokeTiming::CorrectPlayRate(1.0, 0.2, ImpactTime, 0.56, 0.05, Params), 1e-12));  // 近ければ届く
		ASSERT_THAT(IsNear(1.25, TennisStrokeTiming::CorrectPlayRate(1.24, 0.2, ImpactTime, 0.1, 0.05, Params), 1e-12));   // 上限
		ASSERT_THAT(IsNear(0.80, TennisStrokeTiming::CorrectPlayRate(0.81, 0.2, ImpactTime, 5.0, 0.05, Params), 1e-12));   // 下限
		ASSERT_THAT(IsNear(1.25, TennisStrokeTiming::CorrectPlayRate(1.24, 0.2, ImpactTime, -0.1, 0.05, Params), 1e-12));  // 時刻を過ぎた
		ASSERT_THAT(IsNear(1.1, TennisStrokeTiming::CorrectPlayRate(1.1, ImpactTime + 0.1, ImpactTime, 0.5, 0.05, Params), 0.0));  // インパクトの後
		ASSERT_THAT(IsNear(1.1, TennisStrokeTiming::CorrectPlayRate(1.1, 0.2, ImpactTime, 0.5, 0.0, Params), 0.0));
		const double NaN = std::numeric_limits<double>::quiet_NaN();
		ASSERT_THAT(IsNear(1.025, TennisStrokeTiming::CorrectPlayRate(NaN, 0.2, ImpactTime, 0.5, 0.05, Params), 1e-12));  // 数でない速度は等倍から
	}

	// 再生中に打点の時刻が 0.03 s ずれても（風・ネットワークの補正）、0.05 s ごとの補正でインパクトが 1 ステップ以内で合う
	TEST_METHOD(Correction_KeepsTheImpactOnTimeWhenTheHitTimeShifts)
	{
		for (const double Shift : { 0.03, -0.03 })
		{
			double HitTime = 0.9;
			const FStrokeStartPlan Plan = TennisStrokeTiming::PlanStart(ImpactTime, HitTime, Params);
			double Rate = Plan.PlayRate;
			double Position = Plan.StartPosition;
			double SinceReplan = 0.0;
			double Now = Plan.Delay;
			while (Position < ImpactTime)
			{
				if (FMath::IsNearlyEqual(Now, 0.3, TennisConst::SimDt * 0.5))
				{
					HitTime += Shift;
				}
				SinceReplan += TennisConst::SimDt;
				if (SinceReplan >= Params.ReplanInterval)
				{
					Rate = TennisStrokeTiming::CorrectPlayRate(Rate, Position, ImpactTime, HitTime - Now, SinceReplan, Params);
					SinceReplan = 0.0;
				}
				Position += Rate * TennisConst::SimDt;
				Now += TennisConst::SimDt;
			}
			ASSERT_THAT(IsTrue(FMath::Abs(Now - HitTime) <= TennisConst::SimDt + 1e-9, *FString::Printf(TEXT("shift %.2f: impact at %.4f, hit %.4f"), Shift, Now, HitTime)));
		}
	}

	// 体の向き（rad）：狙いへ。ネットの方向（手前の選手は +X、奥の選手は −X）から ±35° まで
	TEST_METHOD(FacingYaw_PointsAtTheAimWithinTheLimit)
	{
		const double Limit = FMath::DegreesToRadians(35.0);
		const FVector2D Hit(-10.0, 1.0);
		ASSERT_THAT(IsNear(0.0, TennisStrokeTiming::FacingYaw(Hit, FVector2D(9.0, 1.0), -1.0, Params), 1e-12));
		ASSERT_THAT(IsNear(FMath::Atan2(-1.0, 19.0), TennisStrokeTiming::FacingYaw(Hit, FVector2D(9.0, 0.0), -1.0, Params), 1e-12));
		ASSERT_THAT(IsNear(Limit, TennisStrokeTiming::FacingYaw(Hit, FVector2D(-9.0, 4.0), -1.0, Params), 1e-12));    // 横すぎる狙い
		ASSERT_THAT(IsNear(-Limit, TennisStrokeTiming::FacingYaw(Hit, FVector2D(-20.0, 0.0), -1.0, Params), 1e-12));  // 後ろの狙い
		ASSERT_THAT(IsNear(0.0, TennisStrokeTiming::FacingYaw(Hit, Hit, -1.0, Params), 0.0));                         // 向きが決まらない
		const double NaN = std::numeric_limits<double>::quiet_NaN();
		ASSERT_THAT(IsNear(0.0, TennisStrokeTiming::FacingYaw(Hit, FVector2D(NaN, 0.0), -1.0, Params), 0.0));

		const FVector2D FarHit(10.0, -1.0);  // 奥の選手：ネットの方向は −X（ヨー π）
		ASSERT_THAT(IsNear(UE_DOUBLE_PI, TennisStrokeTiming::FacingYaw(FarHit, FVector2D(-9.0, -1.0), 1.0, Params), 1e-12));
		ASSERT_THAT(IsNear(UE_DOUBLE_PI - Limit, TennisStrokeTiming::FacingYaw(FarHit, FVector2D(9.0, 4.0), 1.0, Params), 1e-12));
		ASSERT_THAT(IsNear(-(UE_DOUBLE_PI - Limit), TennisStrokeTiming::FacingYaw(FarHit, FVector2D(9.0, -4.0), 1.0, Params), 1e-12));
	}

	// 体の目標：ネットを向いていれば、打点の探索の体の位置（DD-03 §4.2）と同じ式。向きを回すと、ずれも一緒に回る
	TEST_METHOD(BodyTarget_PutsTheSweetSpotOnTheBall)
	{
		const FReachParams Reach;
		const FVector2D Ball(-10.0, 1.5);
		const FVector2D ForehandOffset(Reach.Forward, Reach.ForehandSide);  // 前 0.38 m、右 0.82 m
		ASSERT_THAT(IsTrue(TennisStrokeTiming::BodyTarget(Ball, 0.0, ForehandOffset).Equals(FVector2D(-10.38, 0.68), 1e-9)));       // T-STRK-008 と同じ値
		ASSERT_THAT(IsTrue(TennisStrokeTiming::BodyTarget(FVector2D(10.0, -1.5), UE_DOUBLE_PI, ForehandOffset).Equals(FVector2D(10.38, -0.68), 1e-9)));
		ASSERT_THAT(IsTrue(TennisStrokeTiming::BodyTarget(Ball, 0.0, FVector2D(0.38, -0.62)).Equals(FVector2D(-10.38, 2.12), 1e-9)));  // バック：左

		// 向きを 30° 回す：体から見たスイートスポットの位置を、同じだけ回して足すとボールに重なる
		const FVector2D Body = TennisStrokeTiming::BodyTarget(Ball, FMath::DegreesToRadians(30.0), ForehandOffset);
		ASSERT_THAT(IsTrue((Body + ForehandOffset.GetRotated(30.0)).Equals(Ball, 1e-9)));
	}

	// M3 のニュートラルの狙い（DD-03 §6.4）：相手のコートの中央、ネットから 9.0 m。手前の選手は +X、奥の選手は −X
	TEST_METHOD(NeutralAim_IsTheOpponentsCentre)
	{
		const FStrokeSyncParams Sync;
		ASSERT_THAT(IsTrue(TennisStrokeTiming::NeutralAim(-1.0, Sync).Equals(FVector2D(9.0, 0.0), 0.0)));
		ASSERT_THAT(IsTrue(TennisStrokeTiming::NeutralAim(1.0, Sync).Equals(FVector2D(-9.0, 0.0), 0.0)));
		FStrokeSyncParams Bad;  // 負・数でない距離は 0（ネットの中央）にする
		Bad.NeutralAimDepth = -3.0;
		ASSERT_THAT(IsTrue(TennisStrokeTiming::NeutralAim(-1.0, Bad).Equals(FVector2D::ZeroVector, 0.0)));
		Bad.NeutralAimDepth = std::numeric_limits<double>::quiet_NaN();
		ASSERT_THAT(IsTrue(TennisStrokeTiming::NeutralAim(-1.0, Bad).Equals(FVector2D::ZeroVector, 0.0)));
		ASSERT_THAT(IsNear(0.0, TennisStrokeTiming::NetYaw(-1.0), 0.0));  // ネットの方向：手前は 0、奥は π
		ASSERT_THAT(IsNear(UE_DOUBLE_PI, TennisStrokeTiming::NetYaw(1.0), 0.0));
	}

	// 角度の包み：−π より大きく π 以下。±π はどちらも π。double の π で計算する（エンジンの単精度の π の誤差を入れない）
	TEST_METHOD(WrapAngle_KeepsTheAngleInMinusPiToPi)
	{
		const double Pi = UE_DOUBLE_PI;
		ASSERT_THAT(IsTrue(TennisStrokeTiming::WrapAngle(Pi) == Pi));
		ASSERT_THAT(IsTrue(TennisStrokeTiming::WrapAngle(-Pi) == Pi));
		ASSERT_THAT(IsTrue(TennisStrokeTiming::WrapAngle(0.0) == 0.0));
		ASSERT_THAT(IsNear(0.25, TennisStrokeTiming::WrapAngle(0.25), 1e-15));                   // 範囲の中は変えない
		ASSERT_THAT(IsNear(-0.5 * Pi + 0.1, TennisStrokeTiming::WrapAngle(1.5 * Pi + 0.1), 1e-14));
		ASSERT_THAT(IsNear(0.5 * Pi, TennisStrokeTiming::WrapAngle(-1.5 * Pi), 1e-14));
		ASSERT_THAT(IsNear(0.1, TennisStrokeTiming::WrapAngle(100.0 * Pi + 0.1), 1e-12));     // 大きな値
		ASSERT_THAT(IsNear(-0.1, TennisStrokeTiming::WrapAngle(-100.0 * Pi - 0.1), 1e-12));
		for (const double Angle : { 3.0 * Pi, -3.0 * Pi, 7.0 * Pi + 1e-9, -Pi - 1e-9 })
		{
			const double Wrapped = TennisStrokeTiming::WrapAngle(Angle);
			ASSERT_THAT(IsTrue(Wrapped > -Pi && Wrapped <= Pi, *FString::Printf(TEXT("%.17g -> %.17g"), Angle, Wrapped)));  // 境界の近くでも範囲の中
			ASSERT_THAT(IsNear(1.0, FMath::Cos(Wrapped - Angle), 1e-12));                                                     // 2π の倍数だけずらす
		}
	}

	// T-STRK-019：ネットの方向へ、1 秒あたり ReturnYawRateDeg 以下で近づき、越えない
	TEST_METHOD(ReturnYaw_ApproachesTheTargetWithoutOvershooting)
	{
		const double Rate = FMath::DegreesToRadians(Params.ReturnYawRateDeg);  // 720°/s
		const double Dt = 1.0 / 60.0;
		ASSERT_THAT(IsNear(Rate * Dt, TennisStrokeTiming::ReturnYaw(0.0, 1.0, Rate, Dt), 1e-12));
		ASSERT_THAT(IsNear(-Rate * Dt, TennisStrokeTiming::ReturnYaw(0.0, -1.0, Rate, Dt), 1e-12));
		ASSERT_THAT(IsNear(1.0, TennisStrokeTiming::ReturnYaw(0.95, 1.0, Rate, Dt), 1e-12));  // 越えない
		double Yaw = FMath::DegreesToRadians(-35.0);
		for (int32 Frame = 0; Frame < 10; ++Frame)
		{
			Yaw = TennisStrokeTiming::ReturnYaw(Yaw, 0.0, Rate, Dt);
		}
		ASSERT_THAT(IsNear(0.0, Yaw, 1e-12));  // 35° は 3 フレーム（1 フレーム 12°）で戻り、その後は動かない
	}

	// T-STRK-019：±180° をまたいでも短い向きへ回る（奥の選手のネットの方向は π）
	TEST_METHOD(ReturnYaw_TurnsTheShortWayAcrossPi)
	{
		const double Rate = FMath::DegreesToRadians(Params.ReturnYawRateDeg);
		const double Dt = 1.0 / 60.0;
		const double Pi = UE_DOUBLE_PI;
		const double Yaw = TennisStrokeTiming::ReturnYaw(-Pi + 0.05, Pi - 0.5, Rate, Dt);  // 差は −0.55 rad。1 フレームの 0.209 rad で −π をまたぐ
		ASSERT_THAT(IsNear(Pi + 0.05 - Rate * Dt, Yaw, 1e-12));  // −π − 0.159 を π − 0.159 に直す
		ASSERT_THAT(IsTrue(Yaw > 0.0 && Yaw <= Pi));
	}

	// T-STRK-019：数でない向き・目標、正でない速さ・時間
	TEST_METHOD(ReturnYaw_InvalidInput)
	{
		const double NaN = std::numeric_limits<double>::quiet_NaN();
		ASSERT_THAT(IsNear(0.5, TennisStrokeTiming::ReturnYaw(0.5, NaN, 10.0, 0.1), 1e-12));
		ASSERT_THAT(IsNear(0.2, TennisStrokeTiming::ReturnYaw(NaN, 0.2, 10.0, 0.1), 1e-12));
		ASSERT_THAT(IsNear(0.5, TennisStrokeTiming::ReturnYaw(0.5, 0.0, 0.0, 0.1), 1e-12));
		ASSERT_THAT(IsNear(0.5, TennisStrokeTiming::ReturnYaw(0.5, 0.0, 10.0, -0.1), 1e-12));
		ASSERT_THAT(IsNear(0.5, TennisStrokeTiming::ReturnYaw(0.5, 0.0, NaN, 0.1), 1e-12));
	}
};
