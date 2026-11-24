// ストロークのモンタージュから、UStrokeData と選手の見た目のデータの焼いた値を書く（DD-03 §5.2・§5.4・§7、ART-02 §5、FR-STRK-007）。
// インパクトの時刻・フォロースルーの間の移動の制限の時間（ストローク）と、その体でのインパクトのスイートスポット（見た目のデータ）を、
// 手で入れずにアセットから読む。
#pragma once

#include "CoreMinimal.h"
#include "Chooser.h"  // UFUNCTION の引数（生成されるコードが完全な型を要る）
#include "UObject/Object.h"
#include "TennisStrokeMetaBaker.generated.h"

class USkeletalMesh;
class UStaticMesh;
class UStrokeData;
class UTennisPlayerAppearance;

UCLASS()
class TENNISEDITOR_API UTennisStrokeMetaBaker : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Stroke->Montage の Notify（Tennis Impact・Tennis Recover Start）を読み、ImpactTime・FollowThroughLock を書く（体によらない値）。
	 * 次のときは何も書かずに false を返し、OutError に理由を入れる（警告も出す）：入力がない、Notify が足りない・多い・順序がおかしい、
	 * モンタージュの RateScale が 1 でない、ルートの骨が動いている（その場でない。モンタージュの骨格で評価する）。
	 */
	UFUNCTION(BlueprintCallable, Category = "Tennis|Editor")
	static bool Bake(UStrokeData* Stroke, FString& OutError);

	/**
	 * Body の体での Stroke のインパクトの姿勢で、手のソケット（racket_attach_r）に RacketMesh の grip を重ねたときの racket_sweetspot を、
	 * 選手の足元から見た Transform（位置は m。+X が前）にして OutSweetSpot に入れる（DD-03 §5.4）。検査は Bake と同じで、ソケットも要る。
	 * 合わなければ false を返し、OutError に理由を入れる（警告も出す）
	 */
	static bool BakeStrokeFit(const UStrokeData* Stroke, USkeletalMesh* Body, const UStaticMesh* RacketMesh, FTransform& OutSweetSpot, FString& OutError);

	/**
	 * Appearance の体（Body）で、Strokes のそれぞれのインパクトの姿勢のスイートスポットを BakeStrokeFit と同じ計算（検査と式）で求め、StrokeFits に書く
	 * （ストロークの名前の順）。探索の体の位置（ReachBody）は、StrokeChooser が中くらいの高さのドライブ（フォア・片手と両手のバック）で選ぶ
	 * ストロークの値から作る（横はその横の位置、前はその平均。DD-03 §4.2・§7）。
	 * 次のときは何も書かずに false を返し、OutError に理由を入れる（警告も出す）：入力がない、どれかのストロークが Bake の検査に合わない、
	 * 表が中くらいの高さのドライブで焼いたストロークを選ばない、探索の体の位置が前と横に出ない。
	 */
	UFUNCTION(BlueprintCallable, Category = "Tennis|Editor")
	static bool BakeAppearance(UTennisPlayerAppearance* Appearance, const TArray<UStrokeData*>& Strokes, const UChooserTable* StrokeChooser,
		UStaticMesh* RacketMesh, FString& OutError);
};
