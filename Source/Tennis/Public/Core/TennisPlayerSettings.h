// 選手のアセットの参照：見た目とストロークの表（Project Settings → Game → Tennis Player。Config/DefaultGame.ini にコミットする）。
// C++ にアセットのパスを書かないため、ここで指す。選手ごとの見た目の選択（DA_Player_*、M9）ができるまでは、全員がこの既定を使う。
#pragma once

#include "Engine/DeveloperSettings.h"
#include "Shot/ShotTypes.h"
#include "TennisPlayerSettings.generated.h"

class UAnimInstance;
class UChooserTable;
class UShotTypeData;
class UStaticMesh;
class UTennisPlayerAppearance;

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Tennis Player"))
class TENNIS_API UTennisPlayerSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** 選手の見た目（体と、体に追従する部品：DD-03 §7）。体の骨格はラケットのソケット racket_attach_r・_l を持つ（ART-01 §3.1） */
	UPROPERTY(Config, EditAnywhere, Category = "Appearance")
	TSoftObjectPtr<UTennisPlayerAppearance> Appearance;

	/** 選手のアニメーションの Blueprint（ABP_TennisPlayer。中身は DD-03 §6.5） */
	UPROPERTY(Config, EditAnywhere, Category = "Appearance")
	TSoftClassPtr<UAnimInstance> AnimClass;

	/** ラケット（ソケット grip・racket_sweetspot を持つ：ART-01 §2 A-RKT-001） */
	UPROPERTY(Config, EditAnywhere, Category = "Appearance")
	TSoftObjectPtr<UStaticMesh> RacketMesh;

	/** ストロークを選ぶ表 CHT_StrokeSelect（DD-03 §5.3）。中身は DA_Stroke_* から道具が作る（手で編集しない） */
	UPROPERTY(Config, EditAnywhere, Category = "Stroke")
	TSoftObjectPtr<UChooserTable> StrokeChooser;

	/** 打球に使う球種のデータ DA_Shot_*（DD-03 §6.4 の 6。押した球種と同じ Type のものを使う） */
	UPROPERTY(Config, EditAnywhere, Category = "Shot")
	TArray<TSoftObjectPtr<UShotTypeData>> ShotTypes;

	/** ShotTypes のうち、Type の球種のデータ（最初に見つかったもの）。なければ nullptr */
	const UShotTypeData* FindShotType(ETennisShotType Type) const;
};
