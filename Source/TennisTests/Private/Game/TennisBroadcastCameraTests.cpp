// W-3.8：中継カメラ（DD-07 §1.1、FR-CAM-001、T-CAM-001）。目標の位置と注視点、ばね、投影（エンジンの投影と一致）、
// 相手のベースラインが画面から切れず、操作する選手（足と頭）が画面に入ること。
#include "Camera/TennisBroadcastCamera.h"
#include "Camera/TennisCameraMode_Broadcast.h"
#include "Camera/TennisPlayerCameraManager.h"
#include "Court/TennisCourt.h"
#include "CQTest.h"
#include "Input/TennisPlayerController.h"
#include "Math/RandomStream.h"
#include "SceneView.h"
#include "TennisConstants.h"

namespace
{
	/** エンジンの投影（Maintain Y-Axis FOV。プロジェクトの既定）で、点の画面の位置（端が ±1、上が +） */
	FVector2D EngineScreen(const FMinimalViewInfo& View, int32 Width, int32 Height, const FVector& Point)
	{
		FMinimalViewInfo ViewInfo = View;
		FSceneViewProjectionData Data;
		Data.SetViewRectangle(FIntRect(0, 0, Width, Height));
		Data.ViewOrigin = ViewInfo.Location;
		Data.ViewRotationMatrix = FInverseRotationMatrix(ViewInfo.Rotation)
			* FMatrix(FPlane(0, 0, 1, 0), FPlane(1, 0, 0, 0), FPlane(0, 1, 0, 0), FPlane(0, 0, 0, 1));
		FMinimalViewInfo::CalculateProjectionMatrixGivenViewRectangle(ViewInfo, AspectRatio_MaintainYFOV, Data.GetViewRect(), Data);
		const FVector4 Clip = Data.ComputeViewProjectionMatrix().TransformFVector4(FVector4(Point, 1.0));
		return FVector2D(Clip.X / Clip.W, Clip.Y / Clip.W);
	}

	/** 相手のベースラインの両端と選手の足・頭が、画面の端から EdgeMargin 以上内側にあるか（数を返す：0 なら全部入る） */
	struct FOutside
	{
		int32 Ends = 0;
		int32 Player = 0;
	};
	FOutside CountOutside(const FMinimalViewInfo& View, const FTennisCameraContext& C, const FTennisBroadcastCameraParams& Params)
	{
		const double Limit = 1.0 - 2.0 * Params.EdgeMargin + 1e-6;
		const auto Outside = [&](const FVector& Local)
		{
			const TOptional<FVector2D> S = TennisBroadcastCamera::ProjectToScreen(View.Location, View.Rotation, View.FOV, View.AspectRatio,
				C.ViewAspect, ATennisCourt::LocalMetersToWorld(C.CourtTransform, Local));
			return !S.IsSet() || FMath::Abs(S->X) > Limit || FMath::Abs(S->Y) > Limit;
		};
		FVector Ends[2];
		TennisBroadcastCamera::OpponentBaselineEnds(C.Side, Ends[0], Ends[1]);
		FOutside Result;
		Result.Ends = int32(Outside(Ends[0])) + int32(Outside(Ends[1]));
		Result.Player = int32(Outside(FVector(C.Player.X, C.Player.Y, 0.0))) + int32(Outside(FVector(C.Player.X, C.Player.Y, Params.PlayerHeight)));
		return Result;
	}

	FTennisCameraContext Context(double Side, const FVector& Player, const FVector& Ball, double Aspect)
	{
		FTennisCameraContext Result;
		Result.Side = Side;
		Result.Player = Player;
		Result.Ball = Ball;
		Result.ViewAspect = Aspect;
		return Result;
	}
}

TEST_CLASS(BroadcastCamera, "Tennis.Game.Camera")
{
	// DD-07 §1.1：ベースラインの 15 m 後ろ・高さ 6 m。注視点は選手とボールの 0.4 の内分（Y）、ネットから 2 m 手前・高さ 0.8 m
	TEST_METHOD(Desired_FollowsTheDesign)
	{
		const FTennisBroadcastCameraParams Params;
		FVector Location;
		FVector LookAt;
		TennisBroadcastCamera::Desired(Params, -1.0, FVector(-12.0, 5.0, 0.0), FVector(0.0, 10.0, 1.0), Location, LookAt);
		ASSERT_THAT(IsTrue(Location.Equals(FVector(-26.885, 0.35 * 7.0, 6.0), 1e-9)));  // 焦点 Y = 5 + (10 − 5) × 0.4 = 7
		ASSERT_THAT(IsTrue(LookAt.Equals(FVector(-2.0, 7.0, 0.8), 1e-9)));
		TennisBroadcastCamera::Desired(Params, 1.0, FVector(12.0, 0.0, 0.0), FVector(0.0, 0.0, 1.0), Location, LookAt);
		ASSERT_THAT(IsTrue(Location.Equals(FVector(26.885, 0.0, 6.0), 1e-9)));
		ASSERT_THAT(IsTrue(LookAt.Equals(FVector(2.0, 0.0, 0.8), 1e-9)));
	}

	// 臨界減衰：行き過ぎずに近づく。長いフレームでも行き過ぎない。時間が正の有限の数でなければ動かない
	TEST_METHOD(Spring_SettlesWithoutOvershoot)
	{
		FTennisCameraSpring Spring;
		Spring.Reset(FVector::ZeroVector);
		const FVector Target(1.0, -2.0, 0.5);
		double LastDistance = Target.Size();
		for (int32 Frame = 0; Frame < 240; ++Frame)  // 60 fps で 4 秒
		{
			Spring.Step(Target, 2.5, 1.0 / 60.0);
			const double Distance = (Target - Spring.Value).Size();
			ASSERT_THAT(IsTrue(Distance <= LastDistance + 1e-12));
			ASSERT_THAT(IsTrue(FVector::DotProduct(Target - Spring.Value, Target) >= -1e-12));  // 目標を越えない
			LastDistance = Distance;
		}
		ASSERT_THAT(IsTrue(LastDistance < 0.001 * Target.Size()));

		FTennisCameraSpring Long;
		Long.Reset(FVector::ZeroVector);
		Long.Step(Target, 2.5, 10.0);
		ASSERT_THAT(IsTrue(Long.Value.Equals(Target, 1e-6)));

		const FVector Before = Spring.Value;
		Spring.Step(FVector::ZeroVector, 2.5, 0.0);
		Spring.Step(FVector::ZeroVector, 2.5, -1.0);
		Spring.Step(FVector::ZeroVector, 2.5, std::numeric_limits<double>::quiet_NaN());
		ASSERT_THAT(IsTrue(Spring.Value == Before));
	}

	// 自前の投影は、エンジンの投影（Maintain Y-Axis FOV）と一致する：16:9 と 21:9、横を向いたカメラ
	TEST_METHOD(ProjectToScreen_MatchesTheEngine)
	{
		FMinimalViewInfo View;
		View.Location = FVector(-2088.5, 120.0, 680.0);
		View.Rotation = FRotator(-17.6, 8.0, 0.0);
		View.FOV = 55.0f;
		View.AspectRatio = 16.0f / 9.0f;
		const FVector Points[] = { FVector(1188.5, -548.5, 0.0), FVector(1188.5, 548.5, 0.0), FVector(-200.0, 300.0, 80.0), FVector(600.0, -900.0, 150.0) };
		const FIntPoint Sizes[] = { FIntPoint(1920, 1080), FIntPoint(2560, 1080) };
		for (const FIntPoint& Size : Sizes)
		{
			for (const FVector& Point : Points)
			{
				const TOptional<FVector2D> Ours = TennisBroadcastCamera::ProjectToScreen(
					View.Location, View.Rotation, View.FOV, View.AspectRatio, double(Size.X) / Size.Y, Point);
				ASSERT_THAT(IsTrue(Ours.IsSet()));
				ASSERT_THAT(IsTrue(Ours->Equals(EngineScreen(View, Size.X, Size.Y, Point), 1e-4)));
			}
		}
		ASSERT_THAT(IsFalse(TennisBroadcastCamera::ProjectToScreen(View.Location, View.Rotation, 55.0, 16.0 / 9.0, 16.0 / 9.0,
			View.Location - View.Rotation.Vector() * 100.0).IsSet()));  // カメラの後ろ
	}

	// 最初の評価と Reset の後は目標にそろえる（原点から飛んでこない）。コートを動かしても、コート局所の目標の位置に置く
	TEST_METHOD(FirstEvaluate_SnapsToTheTargetOnAMovedCourt)
	{
		UTennisCameraMode_Broadcast* Mode = NewObject<UTennisCameraMode_Broadcast>();
		FTennisCameraContext C = Context(-1.0, FVector(-12.0, 2.0, 0.0), FVector(3.0, -1.0, 1.0), 16.0 / 9.0);
		C.CourtTransform = FTransform(FRotator(0.0, 90.0, 0.0), FVector(1000.0, -500.0, 20.0));
		FVector Location;
		FVector LookAt;
		TennisBroadcastCamera::Desired(Mode->Params, C.Side, C.Player, C.Ball, Location, LookAt);

		FMinimalViewInfo View = Mode->Evaluate(C, 1.0f / 60.0f);
		ASSERT_THAT(IsTrue(View.Location.Equals(ATennisCourt::LocalMetersToWorld(C.CourtTransform, Location), 1e-3)));
		ASSERT_THAT(IsNear(16.0 / 9.0, double(View.AspectRatio), 1e-6));

		C.Player.Y = 8.0;  // 目標が動く：ばねで遅れる
		View = Mode->Evaluate(C, 1.0f / 60.0f);
		TennisBroadcastCamera::Desired(Mode->Params, C.Side, C.Player, C.Ball, Location, LookAt);
		ASSERT_THAT(IsFalse(View.Location.Equals(ATennisCourt::LocalMetersToWorld(C.CourtTransform, Location), 1.0)));
		Mode->Reset();
		View = Mode->Evaluate(C, 1.0f / 60.0f);
		ASSERT_THAT(IsTrue(View.Location.Equals(ATennisCourt::LocalMetersToWorld(C.CourtTransform, Location), 1e-3)));
	}

	// 出現位置（ベースラインの 0.6 m 後ろ）・2 m 後ろの横の範囲の端・6 m 後ろでも、最初の評価から選手と相手のベースラインが入る
	TEST_METHOD(PlayerAndOpponentBaseline_AreInsideAtFixedPositions)
	{
		const double AreaY = TennisConst::HalfWidthDoubles + 3.5;
		const FVector Players[] = { FVector(-12.5, 0.0, 0.0), FVector(-13.885, AreaY, 0.0), FVector(-13.885, -AreaY, 0.0),
			FVector(-17.885, 0.0, 0.0), FVector(-0.45, 4.0, 0.0) };
		for (const double Aspect : { 16.0 / 9.0, 21.0 / 9.0 })
		{
			for (const FVector& Player : Players)
			{
				UTennisCameraMode_Broadcast* Mode = NewObject<UTennisCameraMode_Broadcast>();
				const FTennisCameraContext C = Context(-1.0, Player, FVector(11.0, 0.0, 1.0), Aspect);
				const FOutside Out = CountOutside(Mode->Evaluate(C, 1.0f / 60.0f), C, Mode->Params);
				ASSERT_THAT(AreEqual(0, Out.Ends));
				ASSERT_THAT(AreEqual(0, Out.Player));
			}
		}
	}

	// T-CAM-001：相手のベースラインの両端と、操作する選手（足と頭）が、上下左右の端から 8% 以上内側にある。記録（W-7.2）ができるまでは、
	// 乱数の 1,000 打の状況の列（選手と球の位置。移動の範囲の端を含む）を 60 fps で流し、ばねの遅れを含めて毎フレーム、16:9 と 21:9 で確かめる
	TEST_METHOD(OpponentBaselineAndPlayer_StayInsideThrough1000Shots)
	{
		const double AreaY = TennisConst::HalfWidthDoubles + 3.5;  // 選手の横の範囲（DD-03 §2.4）
		for (const double Aspect : { 16.0 / 9.0, 21.0 / 9.0 })
		{
			UTennisCameraMode_Broadcast* Mode = NewObject<UTennisCameraMode_Broadcast>();
			FRandomStream Random(20261111);
			int32 Frames = 0;
			FOutside Total;
			int32 PitchedDown = 0;
			int32 Turned = 0;
			FVector Player(-12.0, 0.0, 0.0);
			for (int32 Shot = 0; Shot < 1000; ++Shot)
			{
				const double Side = Shot < 500 ? -1.0 : 1.0;
				if (Shot == 500)
				{
					Player = FVector(12.0, 0.0, 0.0);
				}
				const bool bWide = Shot % 4 == 0;  // 4 打に 1 打は横の範囲の端へ
				const FVector BallFrom(-Side * Random.FRandRange(6.0, 13.0), Random.FRandRange(-6.0, 6.0), 1.0);
				const FVector BallTo(Side * Random.FRandRange(1.0, 12.0),
					bWide ? (Random.FRand() < 0.5 ? -1.0 : 1.0) * Random.FRandRange(AreaY, AreaY + 2.0) : Random.FRandRange(-6.0, 6.0), 1.0);
				const FVector PlayerTo(Side * Random.FRandRange(0.45, TennisConst::HalfLength + 6.0), FMath::Clamp(BallTo.Y, -AreaY, AreaY), 0.0);
				const FVector PlayerFrom = Player;
				const int32 ShotFrames = FMath::RoundToInt(Random.FRandRange(0.6, 1.4) * 60.0);
				for (int32 Frame = 1; Frame <= ShotFrames; ++Frame)
				{
					const double Alpha = double(Frame) / ShotFrames;
					Player = FMath::Lerp(PlayerFrom, PlayerTo, Alpha);
					const FTennisCameraContext C = Context(Side, Player, FMath::Lerp(BallFrom, BallTo, Alpha), Aspect);
					const FMinimalViewInfo View = Mode->Evaluate(C, 1.0f / 60.0f);
					const FOutside Out = CountOutside(View, C, Mode->Params);
					Total.Ends += Out.Ends;
					Total.Player += Out.Player;
					FVector Location;
					FVector LookAt;
					TennisBroadcastCamera::Desired(Mode->Params, Side, C.Player, C.Ball, Location, LookAt);
					const FRotator Aim = (LookAt - Location).Rotation();
					PitchedDown += View.Rotation.Pitch < Aim.Pitch - 1.0 ? 1 : 0;
					Turned += FMath::Abs(FRotator::NormalizeAxis(View.Rotation.Yaw - Aim.Yaw)) > 1.0 ? 1 : 0;
					++Frames;
				}
			}
			ASSERT_THAT(AreEqual(0, Total.Ends));
			ASSERT_THAT(AreEqual(0, Total.Player));
			ASSERT_THAT(IsTrue(Frames > 50000));
			ASSERT_THAT(IsTrue(PitchedDown > 0));  // 選手を入れるために下を向く道が実際に働く
			ASSERT_THAT(IsTrue(Turned > 0));       // 横の制約が実際に働く
		}
	}

	// 選手のコントローラーは、このカメラを使う
	TEST_METHOD(PlayerController_UsesTheTennisCamera)
	{
		ASSERT_THAT(IsTrue(GetDefault<ATennisPlayerController>()->PlayerCameraManagerClass == ATennisPlayerCameraManager::StaticClass()));
		const ATennisPlayerCameraManager* Manager = GetDefault<ATennisPlayerCameraManager>();
		ASSERT_THAT(IsTrue(Manager->GetMode() != nullptr && Manager->GetMode()->IsA<UTennisCameraMode_Broadcast>()));
	}
};
