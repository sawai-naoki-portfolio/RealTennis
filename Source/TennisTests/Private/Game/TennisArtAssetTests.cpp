// アセットの約束（ART-01 §2・§3・§4、ART-03 §5、DD-03 §7）：寸法・ソケット・Nanite・コリジョン・選手の骨格と LOD。
// Blender の生成と UE の取り込み、選手の骨格のソケットが変わっても崩れないことを確かめる。
#include "Animation/Skeleton.h"
#include "CQTest.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "Misc/PackageName.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "TennisDataAssetTestHelpers.h"
#include "Rendering/SkeletalMeshModel.h"

namespace
{
	const UStaticMesh* LoadMesh(const TCHAR* Path)
	{
		return LoadObject<UStaticMesh>(nullptr, Path);
	}

	// ラケットと練習コートの小物には単純コリジョンを付けない（ART-03 §5）。Interchange が自動で付ける全体を覆う凸包は、
	// フェンスではコート全体を覆い、中に出現した選手が地面をすり抜けて落ちる
	bool HasNoSimpleCollision(const UStaticMesh& Mesh)
	{
		const UBodySetup* Body = Mesh.GetBodySetup();
		return Body == nullptr || Body->AggGeom.GetElementCount() == 0;
	}
}

TEST_CLASS(TennisArtAssets, "Tennis.Game.Data.ArtAssets")
{
	// A-RKT-001：全長 68.6 cm（原点はグリップエンド、+X が先端）、幅 27 cm。ソケット racket_sweetspot は 48.1 cm、grip は 6 cm。
	// ソケットの軸はメッシュの軸のまま（回転 0・スケール 1。ART-03 §3。取り込みのツールが Interchange の変換を戻す）
	TEST_METHOD(Racket_HasItsSizeAndSockets)
	{
		const UStaticMesh* Racket = LoadMesh(TEXT("/Game/Tennis/Art/Racket/SM_Racket.SM_Racket"));
		ASSERT_THAT(IsTrue(Racket != nullptr));
		const FBox Bounds = Racket->GetBoundingBox();
		ASSERT_THAT(IsNear(0.0, Bounds.Min.X, 0.05));
		ASSERT_THAT(IsNear(68.6, Bounds.Max.X, 0.05));
		ASSERT_THAT(IsNear(27.0, Bounds.GetSize().Y, 0.05));
		ASSERT_THAT(IsFalse(Racket->IsNaniteEnabled()));
		ASSERT_THAT(IsTrue(HasNoSimpleCollision(*Racket)));

		struct FExpected { const TCHAR* Name; double X; };
		for (const FExpected& Expected : { FExpected{ TEXT("racket_sweetspot"), 48.1 }, FExpected{ TEXT("grip"), 6.0 } })
		{
			const UStaticMeshSocket* Socket = Racket->FindSocket(FName(Expected.Name));
			ASSERT_THAT(IsTrue(Socket != nullptr, Expected.Name));
			ASSERT_THAT(IsTrue(Socket->RelativeLocation.Equals(FVector(Expected.X, 0.0, 0.0), 0.05), Expected.Name));
			ASSERT_THAT(IsTrue(Socket->RelativeRotation.IsNearlyZero(0.01), Expected.Name));
			ASSERT_THAT(IsTrue(Socket->RelativeScale.Equals(FVector::OneVector, 0.001), Expected.Name));
		}
	}

	// A-STD-001：練習コートの小物はコート局所で作り、原点に置くとコートとそろう。フェンスの高さ 3.05 m、照明柱 8 m、ベンチの座面 0.45 m
	TEST_METHOD(PracticeProps_HaveTheirHeights)
	{
		struct FExpected { const TCHAR* Path; double Height; };
		const FExpected Cases[] = {
			{ TEXT("/Game/Tennis/Art/Stadium/SM_Practice_Fence.SM_Practice_Fence"), 305.0 },
			{ TEXT("/Game/Tennis/Art/Stadium/SM_Practice_LightPole.SM_Practice_LightPole"), 800.0 },
			{ TEXT("/Game/Tennis/Art/Stadium/SM_Practice_Bench.SM_Practice_Bench"), 45.0 },
		};
		for (const FExpected& Case : Cases)
		{
			const UStaticMesh* Mesh = LoadMesh(Case.Path);
			ASSERT_THAT(IsTrue(Mesh != nullptr, Case.Path));
			const FBox Bounds = Mesh->GetBoundingBox();
			ASSERT_THAT(IsNear(0.0, Bounds.Min.Z, 0.5, Case.Path));
			ASSERT_THAT(IsNear(Case.Height, Bounds.Max.Z, 0.5, Case.Path));
			ASSERT_THAT(IsNear(0.0, Bounds.GetCenter().X, 0.5, Case.Path));  // X・Y について対称
			ASSERT_THAT(IsNear(0.0, Bounds.GetCenter().Y, 0.5, Case.Path));
			ASSERT_THAT(IsFalse(Mesh->IsNaniteEnabled(), Case.Path));
			ASSERT_THAT(IsTrue(HasNoSimpleCollision(*Mesh), Case.Path));
		}
	}

	// A-PRP-001〜006：コートサイドの小物は自分の局所座標（原点＝接地面の中心、+X＝正面、Y について対称）。寸法は ART-01 §4.2。
	// 球出し機のソケット muzzle は発射口の先端（球の出る点）
	TEST_METHOD(CourtProps_HaveTheirSizesAndTheMuzzleSocket)
	{
		struct FExpected { const TCHAR* Path; FVector Min; FVector Max; };
		const FExpected Cases[] = {
			{ TEXT("/Game/Tennis/Art/Props/SM_Prop_UmpireChair.SM_Prop_UmpireChair"), FVector(-45.0, -35.0, 0.0), FVector(45.0, 35.0, 240.0) },
			{ TEXT("/Game/Tennis/Art/Props/SM_Prop_BallMachine.SM_Prop_BallMachine"), FVector(-25.0, -24.0, 0.0), FVector(31.0, 24.0, 76.0) },
			{ TEXT("/Game/Tennis/Art/Props/SM_Prop_BallCan.SM_Prop_BallCan"), FVector(-3.9, -3.9, 0.0), FVector(3.9, 3.9, 21.6) },
			{ TEXT("/Game/Tennis/Art/Props/SM_Prop_Cooler.SM_Prop_Cooler"), FVector(-33.0, -19.0, 0.0), FVector(33.0, 19.0, 40.0) },
			{ TEXT("/Game/Tennis/Art/Props/SM_Prop_ServeTarget.SM_Prop_ServeTarget"), FVector(-50.0, -50.0, 0.0), FVector(50.0, 50.0, 0.5) },
			{ TEXT("/Game/Tennis/Art/Props/SM_Prop_Scoreboard.SM_Prop_Scoreboard"), FVector(-7.5, -120.0, 0.0), FVector(8.0, 120.0, 220.0) },
		};
		for (const FExpected& Case : Cases)
		{
			const UStaticMesh* Mesh = LoadMesh(Case.Path);
			ASSERT_THAT(IsTrue(Mesh != nullptr, Case.Path));
			const FBox Bounds = Mesh->GetBoundingBox();
			ASSERT_THAT(IsTrue(Bounds.Min.Equals(Case.Min, 0.05), Case.Path));
			ASSERT_THAT(IsTrue(Bounds.Max.Equals(Case.Max, 0.05), Case.Path));
			ASSERT_THAT(IsFalse(Mesh->IsNaniteEnabled(), Case.Path));
			ASSERT_THAT(IsTrue(HasNoSimpleCollision(*Mesh), Case.Path));
		}

		const UStaticMesh* Machine = LoadMesh(Cases[1].Path);
		const UStaticMeshSocket* Muzzle = Machine->FindSocket(FName(TEXT("muzzle")));
		ASSERT_THAT(IsTrue(Muzzle != nullptr));
		ASSERT_THAT(IsTrue(Muzzle->RelativeLocation.Equals(FVector(31.0, 0.0, 33.0), 0.05)));
		ASSERT_THAT(IsTrue(Muzzle->RelativeRotation.IsNearlyZero(0.01)));
		ASSERT_THAT(IsTrue(Muzzle->RelativeScale.Equals(FVector::OneVector, 0.001)));
	}

	// FBX の軸の約束（ART-03 §3）：Blender の +X・+Z は UE でもそのまま、Y だけ反転する。矢印 SM_AxisTest（+X 1.0 m、+Y 0.4 m、+Z 0.2 m）の
	// 境界で確かめる。書き出しの軸の設定や取り込みのツールを変えて取り込み直したとき、向きが変われば失敗する
	TEST_METHOD(AxisTest_KeepsTheFbxAxisConvention)
	{
		const UStaticMesh* Mesh = LoadMesh(TEXT("/Game/Tennis/Art/Dev/SM_AxisTest.SM_AxisTest"));
		ASSERT_THAT(IsTrue(Mesh != nullptr));
		const FBox Bounds = Mesh->GetBoundingBox();
		ASSERT_THAT(IsNear(100.0, Bounds.Max.X, 0.05));   // +X の矢じりの先端
		ASSERT_THAT(IsNear(-40.0, Bounds.Min.Y, 0.05));   // Blender の +Y の棒は UE の −Y へ
		ASSERT_THAT(IsNear(8.0, Bounds.Max.Y, 0.05));     // 矢じりの半径だけ
		ASSERT_THAT(IsNear(20.0, Bounds.Max.Z, 0.05));    // +Z の柱
		ASSERT_THAT(IsTrue(Bounds.Min.X > -3.0 && Bounds.Min.Z > -6.0));  // −X・−Z の側は棒の太さだけ
	}

	// A-STD-010：センターコート A の仮の形（ART-01 §4.3、ART-03 §5）。コート局所座標（原点に置く）。外形 102.23 × 82.11 m・高さ 16.5 m、
	// 看板の壁の外の面は ±20.315・±10.255 m・高さ 1.0 m。Nanite なし。単純コリジョンなし（自動の凸包は観客席の輪の全体を覆い、
	// コートの上の選手が落ちる）。観客席は LOD を 2 つ持つ（LOD1 は取り込みのツールが SM_Stadium_A_Stands_LOD1 から入れる）
	TEST_METHOD(StadiumA_HasItsSizeLodsAndNoCollision)
	{
		struct FExpected { const TCHAR* Path; FVector Max; int32 Lods; };
		const FExpected Cases[] = {
			{ TEXT("/Game/Tennis/Art/Stadium/SM_Stadium_A_Stands.SM_Stadium_A_Stands"), FVector(5111.5, 4105.5, 1650.0), 2 },
			{ TEXT("/Game/Tennis/Art/Stadium/SM_Stadium_A_Barrier.SM_Stadium_A_Barrier"), FVector(2031.5, 1025.5, 100.0), 1 },
			{ TEXT("/Game/Tennis/Art/Stadium/SM_Stadium_A_Floor.SM_Stadium_A_Floor"), FVector(5111.5, 4105.5, 0.0), 1 },
		};
		for (const FExpected& Case : Cases)
		{
			const UStaticMesh* Mesh = LoadMesh(Case.Path);
			ASSERT_THAT(IsTrue(Mesh != nullptr, Case.Path));
			const FBox Bounds = Mesh->GetBoundingBox();
			ASSERT_THAT(IsTrue(Bounds.Max.Equals(Case.Max, 0.5), Case.Path));
			ASSERT_THAT(IsTrue(Bounds.Min.Equals(FVector(-Case.Max.X, -Case.Max.Y, 0.0), 0.5), Case.Path));  // XZ・YZ の面について対称
			ASSERT_THAT(AreEqual(Case.Lods, Mesh->GetNumLODs(), Case.Path));
			ASSERT_THAT(IsFalse(Mesh->IsNaniteEnabled(), Case.Path));
			ASSERT_THAT(IsTrue(HasNoSimpleCollision(*Mesh), Case.Path));
		}
		ASSERT_THAT(IsFalse(FPackageName::DoesPackageExist(TEXT("/Game/Tennis/Art/Stadium/SM_Stadium_A_Stands_LOD1"))));  // LOD に入れて消す
	}

	// A-CHR-001・002：選手の骨格 SK_TennisPlayer_Base（Manny 互換）の両手に、ラケットを持つソケット（骨格のソケット）。
	// 左右は鏡（hand_l の軸は hand_r の 3 軸とも逆）。仮の選手（Manny・Quinn）はこの骨格を使い、メッシュの側で上書きしない
	TEST_METHOD(PlayerSkeleton_HasRacketSocketsOnBothHands)
	{
		const USkeleton* Skeleton = LoadObject<USkeleton>(nullptr, TEXT("/Game/Tennis/Characters/Base/SK_TennisPlayer_Base.SK_TennisPlayer_Base"));
		ASSERT_THAT(IsTrue(Skeleton != nullptr));
		const USkeletalMeshSocket* Right = Skeleton->FindSocket(FName(TEXT("racket_attach_r")));
		const USkeletalMeshSocket* Left = Skeleton->FindSocket(FName(TEXT("racket_attach_l")));
		ASSERT_THAT(IsTrue(Right != nullptr && Left != nullptr));
		ASSERT_THAT(IsTrue(Right->BoneName == FName(TEXT("hand_r")) && Left->BoneName == FName(TEXT("hand_l"))));
		ASSERT_THAT(IsTrue(Right->RelativeLocation.Equals(FVector(-6.5, 3.0, 0.0), 0.01)));   // 手首から指の側へ 6.5 cm、手のひらの側へ 3 cm
		ASSERT_THAT(IsTrue(Left->RelativeLocation.Equals(-Right->RelativeLocation, 0.01)));
		const FVector RightTip = Right->RelativeRotation.RotateVector(FVector::XAxisVector);   // ラケットの先端の向き
		ASSERT_THAT(IsTrue(Left->RelativeRotation.RotateVector(FVector::XAxisVector).Equals(-RightTip, 0.001)));
		ASSERT_THAT(IsTrue(Right->RelativeScale.Equals(FVector::OneVector, 0.001) && Left->RelativeScale.Equals(FVector::OneVector, 0.001)));

		const TCHAR* Meshes[] = {
			TEXT("/Game/Tennis/Characters/Players/Mannequin/SKM_Manny_Simple.SKM_Manny_Simple"),
			TEXT("/Game/Tennis/Characters/Players/Mannequin/SKM_Quinn_Simple.SKM_Quinn_Simple"),
		};
		for (const TCHAR* Path : Meshes)
		{
			const USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, Path);
			ASSERT_THAT(IsTrue(Mesh != nullptr, Path));
			ASSERT_THAT(IsTrue(Mesh->GetSkeleton() == Skeleton, Path));
			ASSERT_THAT(IsTrue(Mesh->FindSocket(FName(TEXT("racket_attach_r"))) == Right, Path));
			ASSERT_THAT(IsTrue(Mesh->FindSocket(FName(TEXT("racket_attach_l"))) == Left, Path));
		}
	}

	// A-CHR-004・005：Blender で作った仮の選手（ART-03 §5）。骨格は SK_TennisPlayer_Base のまま（新しく作らない）、骨の名前・順・
	// 参照姿勢は元の体（Manny・Quinn）と同じ（UE → Blender → UE の往復で変わらない）。LOD は 4 つ、LOD0 は 60k 三角形以下（ART-01 §1）
	TEST_METHOD(InterimPlayers_KeepTheSkeletonAndReferencePoseOfTheirBase)
	{
		const USkeleton* Skeleton = LoadObject<USkeleton>(nullptr, TEXT("/Game/Tennis/Characters/Base/SK_TennisPlayer_Base.SK_TennisPlayer_Base"));
		const UPhysicsAsset* Physics = LoadObject<UPhysicsAsset>(nullptr, TEXT("/Game/Tennis/Characters/Players/Mannequin/PA_Mannequin.PA_Mannequin"));
		ASSERT_THAT(IsTrue(Skeleton != nullptr && Physics != nullptr));
		const TCHAR* Cases[][2] = {
			{TEXT("/Game/Tennis/Characters/Players/Interim/SKM_Player_Male01.SKM_Player_Male01"),
			 TEXT("/Game/Tennis/Characters/Players/Mannequin/SKM_Manny_Simple.SKM_Manny_Simple")},
			{TEXT("/Game/Tennis/Characters/Players/Interim/SKM_Player_Female01.SKM_Player_Female01"),
			 TEXT("/Game/Tennis/Characters/Players/Mannequin/SKM_Quinn_Simple.SKM_Quinn_Simple")},
		};
		for (const auto& [PlayerPath, BasePath] : Cases)
		{
			const USkeletalMesh* Player = LoadObject<USkeletalMesh>(nullptr, PlayerPath);
			const USkeletalMesh* Base = LoadObject<USkeletalMesh>(nullptr, BasePath);
			ASSERT_THAT(IsTrue(Player != nullptr && Base != nullptr, PlayerPath));
			ASSERT_THAT(IsTrue(Player->GetSkeleton() == Skeleton, PlayerPath));
			ASSERT_THAT(IsTrue(Player->GetPhysicsAsset() == Physics, PlayerPath));

			const FReferenceSkeleton& Mine = Player->GetRefSkeleton();
			const FReferenceSkeleton& Theirs = Base->GetRefSkeleton();
			ASSERT_THAT(AreEqual(Theirs.GetNum(), Mine.GetNum(), PlayerPath));
			for (int32 Bone = 0; Bone < Mine.GetNum(); ++Bone)
			{
				const FTransform& A = Mine.GetRefBonePose()[Bone];
				const FTransform& B = Theirs.GetRefBonePose()[Bone];
				ASSERT_THAT(IsTrue(Mine.GetBoneName(Bone) == Theirs.GetBoneName(Bone), PlayerPath));
				ASSERT_THAT(IsTrue(A.GetLocation().Equals(B.GetLocation(), 0.01), PlayerPath));  // cm
				ASSERT_THAT(IsTrue(A.GetRotation().Equals(B.GetRotation(), 1e-4), PlayerPath));
			}

			ASSERT_THAT(AreEqual(4, Player->GetLODNum(), PlayerPath));
			int32 Triangles = 0;
			for (const FSkelMeshSection& Section : Player->GetImportedModel()->LODModels[0].Sections)
			{
				Triangles += Section.NumTriangles;
			}
			ASSERT_THAT(IsTrue(Triangles > 30000 && Triangles <= TennisTest::MaxPlayerTriangles, PlayerPath));
		}
	}
};
