// W-3.1：選手のキャラクターと移動のコンポーネント（DD-03 §1・§2.2）、ボールとの処理の順（ARCH-01 §5）。
#include "Ball/TennisBall.h"
#include "Ball/TennisBallSimComponent.h"
#include "Components/ActorTestSpawner.h"
#include "CQTest.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Court/TennisCourt.h"
#include "Player/TennisCharacter.h"
#include "Player/TennisMovementComponent.h"
#include "Player/TennisStrokeComponent.h"
#include "TennisConstants.h"

TEST_CLASS(TennisCharacter, "Tennis.Game.Player")
{
	// 移動は UTennisMovementComponent。カプセルは半径 0.30 m・半分の高さ 0.90 m
	TEST_METHOD(Defaults_UseTennisMovementAndBodySize)
	{
		const ATennisCharacter* Character = GetDefault<ATennisCharacter>();
		ASSERT_THAT(IsTrue(Character->GetTennisMovement() != nullptr));
		ASSERT_THAT(IsNear(30.0, static_cast<double>(Character->GetCapsuleComponent()->GetUnscaledCapsuleRadius()), 1e-4));
		ASSERT_THAT(IsNear(90.0, static_cast<double>(Character->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()), 1e-4));
		ASSERT_THAT(IsFalse(Character->GetTennisMovement()->bOrientRotationToMovement));
	}

	// ワールドの速度（cm/s）は、コート局所（m/s）で TennisMovement::AdvanceFrame と同じ規則で進む。Z はそのまま
	TEST_METHOD(AdvanceWorldVelocity_MatchesTheModelInCourtSpace)
	{
		const FMovementParams Params;
		FMovementStepper Model;
		const FVector2D Planar = TennisMovement::AdvanceFrame(Model, FVector2D(-8.0, 0.0), FVector2D(0.0, 1.0), -1.0, FMovementModifiers(), Params, 1.0 / 60.0);
		const FVector Expected = ATennisCourt::LocalMetersToWorldVector(FTransform::Identity, FVector(Planar, 0.0));

		FMovementStepper Stepper;
		const FVector World = UTennisMovementComponent::AdvanceWorldVelocity(FTransform::Identity, FVector(-800.0, 0.0, 90.0), FVector(0.0, 0.0, -25.0),
			FVector(0.0, 1.0, 0.0), -1.0, Stepper, Params, 1.0 / 60.0, 1.0);
		ASSERT_THAT(IsTrue(Planar.Y > 0.0));
		ASSERT_THAT(IsNear(Expected.Y, World.Y, 1e-6));
		ASSERT_THAT(IsNear(0.0, World.X, 1e-9));
		ASSERT_THAT(IsNear(-25.0, World.Z, 1e-12));
	}

	// フォロースルーの制限（DD-03 §2.1・§6.6）：付けている間は、最高速度 × FollowThroughSpeedFactor を超えない。コンポーネントは付け外しを覚える
	TEST_METHOD(AdvanceWorldVelocity_FollowThroughCapsTheSpeed)
	{
		const FMovementParams Params;
		FMovementModifiers FollowThrough;
		FollowThrough.bFollowThrough = true;
		FMovementStepper Free, Limited;
		FVector FreeVelocity = FVector::ZeroVector, LimitedVelocity = FVector::ZeroVector;
		for (int32 Frame = 0; Frame < 120; ++Frame)  // 2 s：どちらも上限に届く
		{
			FreeVelocity = UTennisMovementComponent::AdvanceWorldVelocity(FTransform::Identity, FVector(-800.0, 0.0, 90.0), FreeVelocity,
				FVector(0.0, 1.0, 0.0), -1.0, Free, Params, 1.0 / 60.0, 1.0);
			LimitedVelocity = UTennisMovementComponent::AdvanceWorldVelocity(FTransform::Identity, FVector(-800.0, 0.0, 90.0), LimitedVelocity,
				FVector(0.0, 1.0, 0.0), -1.0, Limited, Params, 1.0 / 60.0, 1.0, FollowThrough);
		}
		const double Cap = Params.MaxSpeed * Params.FollowThroughSpeedFactor;  // m/s
		ASSERT_THAT(IsTrue(Free.State.Velocity.Size() > Cap + 0.5));
		ASSERT_THAT(IsNear(Cap, Limited.State.Velocity.Size(), 1e-6));

		UTennisMovementComponent* Movement = NewObject<UTennisMovementComponent>();
		ASSERT_THAT(IsFalse(Movement->GetModifiers().bFollowThrough));
		Movement->SetFollowThrough(true);
		ASSERT_THAT(IsTrue(Movement->GetModifiers().bFollowThrough));
		Movement->SetFollowThrough(false);
		ASSERT_THAT(IsFalse(Movement->GetModifiers().bFollowThrough));
	}

	// 回したコート：位置・入力の向き・速度は、コートの軸で解いてからワールドに戻す（CMC と同じく、返した速度で動かす）
	TEST_METHOD(AdvanceWorldVelocity_FollowsARotatedCourt)
	{
		const FTransform Court(FRotator(0.0, 90.0, 0.0), FVector(500.0, 0.0, 0.0));  // コートの +X はワールドの +Y
		FMovementStepper Stepper;
		FVector Location(500.0, -1700.0, 90.0);  // コート局所 (−17, 0)：2 s 進んでもネットの手前の端に届かない
		FVector World = FVector::ZeroVector;
		for (int32 Frame = 0; Frame < 120; ++Frame)  // 2 s（60 fps）、コートの +X（ネットへ向かう向き）へ入力
		{
			World = UTennisMovementComponent::AdvanceWorldVelocity(Court, Location, World, FVector(0.0, 1.0, 0.0), -1.0, Stepper, FMovementParams(), 1.0 / 60.0, 1.0);
			Location += World / 60.0;
		}
		ASSERT_THAT(IsTrue(World.Equals(FVector(0.0, 550.0, 0.0), 1e-6), *World.ToString()));
		ASSERT_THAT(IsTrue(FVector2D(Stepper.State.Velocity).Equals(FVector2D(5.5, 0.0), 1e-9)));
		ASSERT_THAT(IsNear(500.0, Location.X, 1e-6));
	}

	// ARCH-01 §5：ボールは TG_PrePhysics で最初に進め、選手のストローク・移動・メッシュはボールの後に処理する。移動とメッシュはストロークの後
	// （ストロークが決めた体の向き・モンタージュの位置・自動移動を、同じフレームで使う：DD-03 §6.6）
	TEST_METHOD(TickOrder_BallFirstThenPlayer)
	{
		ASSERT_THAT(IsTrue(GetDefault<UTennisBallSimComponent>()->PrimaryComponentTick.TickGroup == TG_PrePhysics));

		FActorTestSpawner Spawner;
		ATennisBall& Ball = Spawner.SpawnActor<ATennisBall>();
		ATennisCharacter& Character = Spawner.SpawnActor<ATennisCharacter>();
		Character.FollowBall(*Ball.GetSim());
		const auto Waits = [](const FTickFunction& Tick, const FTickFunction& First)
		{
			return Tick.GetPrerequisites().ContainsByPredicate(
				[&First](const FTickPrerequisite& Prerequisite) { return Prerequisite.PrerequisiteTickFunction == &First; });
		};
		const FTickFunction& BallTick = Ball.GetSim()->PrimaryComponentTick;
		const FTickFunction& StrokeTick = Character.GetStroke()->PrimaryComponentTick;
		ASSERT_THAT(IsTrue(Waits(StrokeTick, BallTick)));
		ASSERT_THAT(IsTrue(Waits(Character.GetCharacterMovement()->PrimaryComponentTick, BallTick)));
		ASSERT_THAT(IsTrue(Waits(Character.GetMesh()->PrimaryComponentTick, BallTick)));
		ASSERT_THAT(IsTrue(Waits(Character.GetCharacterMovement()->PrimaryComponentTick, StrokeTick)));
		ASSERT_THAT(IsTrue(Waits(Character.GetMesh()->PrimaryComponentTick, StrokeTick)));
	}
};
