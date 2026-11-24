// データアセットのテストに共通の道具。
#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetRegistryModule.h"

namespace TennisTest
{
	/** DA_Stroke_* を置く場所（MCP のツールの stroke_assets.STROKE_FOLDER と同じ） */
	inline const FString StrokesFolder = TEXT("/Game/Tennis/Data/Strokes");
	/** 選手の見た目のデータを置く場所（MCP のツールの stroke_build.PLAYERS_FOLDER と同じ。コミットしない試しのアセットは入れない） */
	inline const FString PlayersFolder = TEXT("/Game/Tennis/Characters/Players");
	/** ART-01 §1 の予算：選手（体・服・顔・髪）の試合中の三角形 */
	inline constexpr int32 MaxPlayerTriangles = 60000;

	// Folder（とその下）に置いた T 型のデータアセットをすべて読み込む（アセットを足せば、テストを変えずに確認の対象に入る：OCP）。
	// ほかのフォルダ（コミットしない試しのアセットなど）のものは入れない
	template <typename T>
	TArray<const T*> LoadAllDataAssets(const FString& Folder = TEXT("/Game/Tennis/Data"))
	{
		IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		Registry.ScanPathsSynchronous({ Folder });
		TArray<FAssetData> Found;
		Registry.GetAssetsByClass(T::StaticClass()->GetClassPathName(), Found);
		Found.Sort([](const FAssetData& A, const FAssetData& B) { return A.PackageName.LexicalLess(B.PackageName); });  // 毎回同じ順
		TArray<const T*> Assets;
		for (const FAssetData& Data : Found)
		{
			const FString Package = Data.PackageName.ToString();
			if (Package.StartsWith(Folder + TEXT("/")))
			{
				if (const T* Asset = Cast<T>(Data.GetAsset()))
				{
					Assets.Add(Asset);
				}
			}
		}
		return Assets;
	}
}
