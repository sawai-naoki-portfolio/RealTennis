// W-2.5：球種のデータアセット（DA_Shot_*）。球種ごとに 1 つあり、どれも典型の打点から解け、球種ごとに軌道の形が違う（T-SHOT-003）。
#include "CQTest.h"
#include "Ball/TennisBallPredictor.h"
#include "Data/ShotTypeData.h"
#include "Shot/ShotRequestMapping.h"
#include "Shot/TennisShotSolver.h"
#include "TennisDataAssetTestHelpers.h"
#include "UObject/Package.h"

namespace
{
	struct FSituation
	{
		FVector Origin;  // m（打点）
		FVector Target;  // m（狙い）
	};

	// 球種ごとの典型の打点と狙い（コート局所。打つ側は X < 0）
	FSituation TypicalSituation(ETennisShotType Type)
	{
		switch (Type)
		{
		case ETennisShotType::Drop:          return { FVector(-9.0, 0.0, 1.0), FVector(3.0, 1.0, 0.0) };
		case ETennisShotType::Volley:        return { FVector(-2.5, 0.5, 1.2), FVector(8.5, 2.0, 0.0) };
		case ETennisShotType::HalfVolley:    return { FVector(-6.0, 0.0, 0.3), FVector(9.0, 1.0, 0.0) };
		case ETennisShotType::Smash:         return { FVector(-4.0, 0.0, 2.6), FVector(8.0, 2.0, 0.0) };
		case ETennisShotType::ServeFlat:
		case ETennisShotType::ServeSlice:
		case ETennisShotType::ServeKick:     return { FVector(-11.985, -0.5, 2.8), FVector(5.5, 1.0, 0.0) };
		case ETennisShotType::ServeUnderarm: return { FVector(-11.985, -0.5, 0.8), FVector(4.5, 1.0, 0.0) };
		default:                             return { FVector(-11.0, 0.0, 1.0), FVector(9.0, 1.0, 0.0) };  // ベースラインから深く
		}
	}

	// 同じ打点と目標・溜め 0.5 で解いた球の形（T-SHOT-003）
	struct FShape
	{
		bool   bSolved = false;
		double NetClearance = 0.0;  // m
		double Apex = 0.0;          // m（最初のバウンドまでの最高点）
		double BounceHeight = 0.0;  // m（最初のバウンドから 2 回目までの最高点）
		double Speed = 0.0;         // m/s（打ち出し）
	};

	FShape RallyShape(const UShotTypeData& Shot)
	{
		const FTennisShotSolver Solver{ FBallSimContext() };
		const FShotSolveResult Result = Solver.Solve(TennisShotRequestMapping::Make(
			Shot.Params, FVector(-11.0, 0.0, 1.0), FVector(9.0, 0.0, 0.0), 0.0, 0.5, ETennisHandedness::Right, 0.0));
		FShape Shape;
		Shape.bSolved = Result.bSolved;
		Shape.NetClearance = Result.PredictedNetClearance;
		Shape.Apex = Solver.SimulateToFirstBounce(Result.Launch).Apex;
		Shape.Speed = Result.Launch.Vel.Size();
		FTrajectory Trajectory;
		FTennisBallPredictor::Predict(Result.Launch, FBallSimContext(), FPredictOptions(), Trajectory);
		const FBallEvent* FirstBounce = Trajectory.FirstBounce();
		for (const FBallState& State : Trajectory.States)
		{
			if (FirstBounce != nullptr && State.Time > FirstBounce->Time)
			{
				Shape.BounceHeight = FMath::Max(Shape.BounceHeight, State.Pos.Z);
			}
		}
		return Shape;
	}

	const UShotTypeData* FindShot(const TArray<const UShotTypeData*>& Shots, ETennisShotType Type)
	{
		const UShotTypeData* const* Found = Shots.FindByPredicate([Type](const UShotTypeData* Shot) { return Shot->Type == Type; });
		return Found != nullptr ? *Found : nullptr;
	}

	int32 NumShotTypes()
	{
		return StaticEnum<ETennisShotType>()->NumEnums() - 1;  // 最後は _MAX
	}
}

TEST_CLASS(ShotTypeData, "Tennis.Game.Data")
{
	// 新しく作ったデータの既定値は、TennisSim の型の既定値と同じ（DRY：既定値の置き場所は型の 1 か所）
	TEST_METHOD(Defaults_MatchSimulationTypes)
	{
		const UShotTypeData* Data = NewObject<UShotTypeData>(GetTransientPackage());
		const FShotTypeParams Expected;
		ASSERT_THAT(IsNear(Expected.MinSpeed, Data->Params.MinSpeed, 1e-12));
		ASSERT_THAT(IsNear(Expected.ConstraintAtCharge0, Data->Params.ConstraintAtCharge0, 1e-12));
		ASSERT_THAT(IsTrue(Expected.Constraint == Data->Params.Constraint));
	}

	// 球種ごとにちょうど 1 つのアセットがあり、速さの範囲が正しい向き
	TEST_METHOD(ShotAssets_CoverEveryTypeOnce)
	{
		const TArray<const UShotTypeData*> Shots = TennisTest::LoadAllDataAssets<UShotTypeData>();
		ASSERT_THAT(AreEqual(NumShotTypes(), Shots.Num()));
		TSet<ETennisShotType> Seen;
		for (const UShotTypeData* Shot : Shots)
		{
			bool bAlreadySeen = false;
			Seen.Add(Shot->Type, &bAlreadySeen);
			ASSERT_THAT(IsFalse(bAlreadySeen, *Shot->GetName()));
			ASSERT_THAT(IsTrue(Shot->Params.MinSpeed <= Shot->Params.MaxSpeed, *Shot->GetName()));
		}
	}

	// どの球種も、典型の打点から溜め 0.5 で解ける（球種のデータとソルバの組み合わせの確認）
	TEST_METHOD(ShotAssets_SolveFromTypicalSituations)
	{
		const FTennisShotSolver Solver{ FBallSimContext() };
		for (const UShotTypeData* Shot : TennisTest::LoadAllDataAssets<UShotTypeData>())
		{
			const FSituation Situation = TypicalSituation(Shot->Type);
			const FShotRequest Request = TennisShotRequestMapping::Make(
				Shot->Params, Situation.Origin, Situation.Target, 0.0, 0.5, ETennisHandedness::Right, 0.0);
			const FShotSolveResult Result = Solver.Solve(Request);
			ASSERT_THAT(IsTrue(Result.bSolved, *Shot->GetName()));
		}
	}

	// T-SHOT-003：同じ打点と目標・溜め 0.5 で、球種ごとに軌道の形が違う。ネットの上の余裕・最高点・バウンドの後の高さは
	// HeavyTopspin > Topspin > Flat > Slice（スライスは低く飛び、低く滑る）。打ち出しの速さは Flat が最も速い
	TEST_METHOD(RallyShapes_DifferByType)
	{
		const TArray<const UShotTypeData*> Shots = TennisTest::LoadAllDataAssets<UShotTypeData>();
		TArray<FShape> Shapes;
		for (const ETennisShotType Type : { ETennisShotType::HeavyTopspin, ETennisShotType::Topspin, ETennisShotType::Flat, ETennisShotType::Slice })
		{
			const UShotTypeData* Shot = FindShot(Shots, Type);
			ASSERT_THAT(IsTrue(Shot != nullptr));
			Shapes.Add(RallyShape(*Shot));
			ASSERT_THAT(IsTrue(Shapes.Last().bSolved, *Shot->GetName()));
		}
		for (int32 Index = 1; Index < Shapes.Num(); ++Index)
		{
			ASSERT_THAT(IsTrue(Shapes[Index - 1].NetClearance > Shapes[Index].NetClearance));
			ASSERT_THAT(IsTrue(Shapes[Index - 1].Apex > Shapes[Index].Apex));
			ASSERT_THAT(IsTrue(Shapes[Index - 1].BounceHeight > Shapes[Index].BounceHeight));
		}
		const double FlatSpeed = Shapes[2].Speed;
		ASSERT_THAT(IsTrue(FlatSpeed > Shapes[0].Speed && FlatSpeed > Shapes[1].Speed && FlatSpeed > Shapes[3].Speed));
	}
};
