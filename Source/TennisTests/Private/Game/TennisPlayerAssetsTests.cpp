// 置いた選手の見た目のデータのアセット（ART-03 §6 の手順 8、DD-03 §7。T-CHR-002）：どの見た目のデータでも、体はラケットのソケットを持ち、
// 骨格は SK_TennisPlayer_Base か、それを互換の骨格に持つ（今のアニメーションが動く）。追従する部品のメッシュがある。髪の Binding は Groom を持ち、
// 対象のメッシュは体か追従する部品。MetaHuman の選手（体の骨格が SK_TennisPlayer_Base でない）は服（体と同じ骨格の追従する部品）を着る。
// 試合中の三角形は予算の中（T-CHR-003）。見た目のデータを足せば、テストを変えずに確かめる対象に入る（OCP）。
#include "Animation/Skeleton.h"
#include "CQTest.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GroomAsset.h"
#include "GroomBindingAsset.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Player/TennisCharacter.h"
#include "Player/TennisPlayerAppearance.h"
#include "TennisDataAssetTestHelpers.h"

namespace
{
// 試合中の頭の Groom の画面の大きさのいちばん大きい値。中継カメラ（水平 55°）で、ベースラインの上の計測は 0.042〜0.048（2026-11-18）。
// 選手がベースラインの後ろ 6 m まで下がると（DD-03 §3）カメラに約 10 m まで近づき、見積もりで約 0.075 になるので、余裕を足した
constexpr float MaxMatchGroomScreenSize = 0.08f;

int32 SkeletalTriangles(const USkeletalMesh& Mesh)
{
	const FSkeletalMeshRenderData* Data = Mesh.GetResourceForRendering();
	return Data != nullptr && Data->LODRenderData.Num() > 0 ? Data->LODRenderData[0].GetTotalFaces() : -1;
}

// 試合中に使う LOD の最初。エンジンは画面の大きさを挟む 2 つの LOD の間を選び、どの LOD の ScreenSize より小さければ最後の LOD を使う
// （HairStrandsCore の GetHairInstanceLODIndex）。MaxMatchGroomScreenSize より小さいところでは、これを挟む手前の LOD から最後までを使う
int32 FirstMatchLOD(const TArray<FHairLODSettings>& LODs)
{
	const int32 Below = LODs.IndexOfByPredicate([](const FHairLODSettings& LOD) { return LOD.ScreenSize <= MaxMatchGroomScreenSize; });
	return Below == INDEX_NONE ? LODs.Num() - 1 : FMath::Max(0, Below - 1);
}

// Groom の試合中の三角形の上限：グループごとに、取り込んだカードとメッシュのメッシュのうちいちばん多いもの（試合中の LOD は、そのどれか
// を減らしたものか、そのもので描く。カードもメッシュもないグループは試合中に描かないので 0：F01 のポニーテールのグループ 1）。試合中に使う
// LOD にストランドがある、またはメッシュの三角形が 0（レンダーデータがない）なら、理由を OutError に書いて -1
int32 GroomMatchTriangles(const UGroomAsset& Groom, FString& OutError)
{
	int32 Total = 0;
	const TArray<FHairGroupsLOD>& Groups = Groom.GetHairGroupsLOD();
	for (int32 Group = 0; Group < Groups.Num(); ++Group)
	{
		const TArray<FHairLODSettings>& LODs = Groups[Group].LODs;
		for (int32 Index = FirstMatchLOD(LODs); Index >= 0 && Index < LODs.Num(); ++Index)
		{
			if (LODs[Index].GeometryType == EGroomGeometryType::Strands)
			{
				OutError = FString::Printf(TEXT("group %d draws strands at LOD %d in a match"), Group, Index);
				return -1;
			}
		}
		int32 Most = 0;
		int32 Empty = 0;
		const auto Visit = [&Most, &Empty, Group](const UStaticMesh* Mesh, int32 GroupIndex)
		{
			if (Mesh != nullptr && GroupIndex == Group)
			{
				const int32 Triangles = Mesh->GetNumTriangles(0);  // レンダーデータがなければ 0
				Most = FMath::Max(Most, Triangles);
				Empty += Triangles <= 0 ? 1 : 0;
			}
		};
		for (const FHairGroupsCardsSourceDescription& Cards : Groom.GetHairGroupsCards())
		{
			Visit(Cards.ImportedMesh, Cards.GroupIndex);
		}
		for (const FHairGroupsMeshesSourceDescription& Meshes : Groom.GetHairGroupsMeshes())
		{
			Visit(Meshes.ImportedMesh, Meshes.GroupIndex);
		}
		if (Empty > 0)
		{
			OutError = FString::Printf(TEXT("group %d has %d card or mesh source(s) without triangles (no render data?)"), Group, Empty);
			return -1;
		}
		Total += Most;
	}
	return Total;
}
}

TEST_CLASS(TennisPlayerAssets, "Tennis.Game.Data")
{
	TEST_METHOD(Appearances_HaveSocketsACompatibleSkeletonAndBoundGrooms)
	{
		const USkeleton* Base = LoadObject<USkeleton>(nullptr, TEXT("/Game/Tennis/Characters/Base/SK_TennisPlayer_Base.SK_TennisPlayer_Base"));
		ASSERT_THAT(IsTrue(Base != nullptr));
		const TArray<const UTennisPlayerAppearance*> Appearances = TennisTest::LoadAllDataAssets<UTennisPlayerAppearance>(TennisTest::PlayersFolder);
		ASSERT_THAT(IsTrue(Appearances.Num() > 0));
		for (const UTennisPlayerAppearance* Appearance : Appearances)
		{
			const FString Name = Appearance->GetName();
			const USkeletalMesh* Body = Appearance->Body.LoadSynchronous();
			ASSERT_THAT(IsTrue(Body != nullptr, *Name));
			ASSERT_THAT(IsTrue(Body->FindSocket(ATennisCharacter::RacketHandSocket) != nullptr && Body->FindSocket(TEXT("racket_attach_l")) != nullptr, *Name));
			const USkeleton* Skeleton = Body->GetSkeleton();
			ASSERT_THAT(IsTrue(Skeleton == Base || (Skeleton != nullptr && Skeleton->GetCompatibleSkeletons().Contains(TSoftObjectPtr<USkeleton>(Base))), *Name));

			TArray<const USkeletalMesh*> Parts = {Body};
			for (const TSoftObjectPtr<USkeletalMesh>& Follower : Appearance->Followers)
			{
				const USkeletalMesh* Part = Follower.LoadSynchronous();
				ASSERT_THAT(IsTrue(Part != nullptr, *Name));
				Parts.Add(Part);
			}
			for (const TSoftObjectPtr<UGroomBindingAsset>& Soft : Appearance->GroomBindings)
			{
				const UGroomBindingAsset* Binding = Soft.LoadSynchronous();
				ASSERT_THAT(IsTrue(Binding != nullptr && Binding->GetGroom() != nullptr && Binding->GetGroom()->IsValid(), *Name));
				ASSERT_THAT(IsTrue(Parts.Contains(Binding->GetTargetSkeletalMesh()), *FString::Printf(TEXT("%s: %s"), *Name, *Binding->GetName())));
			}
			// MetaHuman の選手は服（A-CHR-040〜043：体と同じ骨格の追従する部品）を着る（ART-03 §6 の手順 5・6）
			if (Skeleton != Base)
			{
				ASSERT_THAT(IsTrue(Parts.ContainsByPredicate([Body, Skeleton](const USkeletalMesh* Part) { return Part != Body && Part->GetSkeleton() == Skeleton; }),
					*FString::Printf(TEXT("%s: no wear on %s (run apply_metahuman_wear)"), *Name, *GetNameSafe(Skeleton))));
			}
		}
	}

	// T-CHR-003：試合中の三角形（体と追従する部品は LOD0：中継カメラでは体が LOD0 で、追従する部品は体の LOD に従う。Groom は GroomMatchTriangles）
	TEST_METHOD(Appearances_FitTheMatchTriangleBudget)
	{
		const TArray<const UTennisPlayerAppearance*> Appearances = TennisTest::LoadAllDataAssets<UTennisPlayerAppearance>(TennisTest::PlayersFolder);
		ASSERT_THAT(IsTrue(Appearances.Num() > 0));
		for (const UTennisPlayerAppearance* Appearance : Appearances)
		{
			const FString Name = Appearance->GetName();
			TArray<const USkeletalMesh*> Parts = {Appearance->Body.LoadSynchronous()};
			for (const TSoftObjectPtr<USkeletalMesh>& Follower : Appearance->Followers)
			{
				Parts.Add(Follower.LoadSynchronous());
			}
			int32 Total = 0;
			for (const USkeletalMesh* Part : Parts)
			{
				ASSERT_THAT(IsTrue(Part != nullptr, *FString::Printf(TEXT("%s: a body or follower mesh is missing"), *Name)));
				const int32 Triangles = SkeletalTriangles(*Part);
				ASSERT_THAT(IsTrue(Triangles > 0, *FString::Printf(TEXT("%s: %s has no render data"), *Name, *Part->GetName())));
				Total += Triangles;
			}
			for (const TSoftObjectPtr<UGroomBindingAsset>& Soft : Appearance->GroomBindings)
			{
				const UGroomBindingAsset* Binding = Soft.LoadSynchronous();
				const UGroomAsset* Groom = Binding != nullptr ? Binding->GetGroom() : nullptr;
				ASSERT_THAT(IsTrue(Groom != nullptr, *FString::Printf(TEXT("%s: a groom binding has no groom"), *Name)));
				FString Error;
				const int32 Triangles = GroomMatchTriangles(*Groom, Error);
				ASSERT_THAT(IsTrue(Triangles >= 0, *FString::Printf(TEXT("%s: %s: %s"), *Name, *Groom->GetName(), *Error)));
				Total += Triangles;
			}
			TestRunner->AddInfo(FString::Printf(TEXT("%s: match triangles %d"), *Name, Total));
			ASSERT_THAT(IsTrue(Total <= TennisTest::MaxPlayerTriangles, *FString::Printf(TEXT("%s: %d > %d"), *Name, Total, TennisTest::MaxPlayerTriangles)));
		}
	}
};
