// 選手のキャラクター（DD-03 §1、クラス図）。移動は UTennisMovementComponent（CMC 派生）。見た目は、見た目のデータ
// （UTennisPlayerAppearance：体と、体に追従する顔・服と、髪の Groom）とラケット（UTennisRacketComponent）。既定は Project Settings の Tennis Player が指す。
// ストローク（UTennisStrokeComponent）は打点の候補で振り、インパクトで選手が判定して打つ（DD-03 §6.4・§6.6）。スタミナなどのコンポーネントは、
// 使うマイルストーンで加える。
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Reach/TennisReach.h"
#include "Shot/ShotTypes.h"
#include "TennisCharacter.generated.h"

class UGroomComponent;
class UTennisBallSimComponent;
class UTennisPlayerAppearance;
class UTennisMovementComponent;
class UTennisRacketComponent;
class UTennisReachComponent;
class UTennisStrokeComponent;
struct FStrokeImpact;

UCLASS()
class TENNIS_API ATennisCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	/** カプセルの半径と半分の高さ（cm。DD-03 §1：0.30 m・0.90 m。移動用で、ボールの判定には使わない） */
	static constexpr float CapsuleRadius = 30.0f;
	static constexpr float CapsuleHalfHeight = 90.0f;

	/** ラケットを付ける手のソケット（骨格 SK_TennisPlayer_Base の hand_r。左利きの racket_attach_l は FR-STRK-009 の M6 で使う） */
	static const FName RacketHandSocket;

	explicit ATennisCharacter(const FObjectInitializer& ObjectInitializer);

	UTennisMovementComponent* GetTennisMovement() const;

	UTennisReachComponent* GetReach() const { return Reach; }
	UTennisRacketComponent* GetRacket() const { return Racket; }
	UTennisStrokeComponent* GetStroke() const { return Stroke; }
	/** 体の骨に追従する部品（顔・服）のコンポーネント */
	const TArray<TObjectPtr<USkeletalMeshComponent>>& GetFollowers() const { return Followers; }
	/** 髪・眉・ひげのコンポーネント（Binding の対象のメッシュを持つ部品に付いている） */
	const TArray<TObjectPtr<UGroomComponent>>& GetGrooms() const { return Grooms; }
	/** 付けている見た目のデータ（体ごとの焼いた値を引く：DD-03 §7）。付けていなければ nullptr */
	const UTennisPlayerAppearance* GetAppearance() const { return Appearance; }

	/** 見た目を付ける（体・追従する部品・髪・アニメーションの Blueprint・ラケット）。前の部品と髪は消す。
	 *  体がなければ警告を出し、カプセルを見せてラケットを隠して false。部品のメッシュがなければ、その部品だけを飛ばす。
	 *  髪の Binding がない・Groom がなければその髪だけを飛ばし、Binding の対象のメッシュを持つ部品がなければ体に付ける（警告）。
	 *  探索の体の位置（フォア・バックの横と前）は、見た目のデータの焼いた値（ReachBody）にする。焼いていない・体がなければパラメータ表の既定 */
	bool ApplyAppearance(const UTennisPlayerAppearance* NewAppearance);

	/** このボールを見る：ストローク・移動・メッシュ・打点の探索をボールのシミュレーションの後に処理し（ARCH-01 §5）、移動とメッシュは
	 *  ストロークの後にする（ストロークが決めた体の向き・モンタージュの位置・自動移動を、同じフレームで使う）。探索の対象にする */
	void FollowBall(UTennisBallSimComponent& Ball);

	/** 打つ意図を受け取る（予約・球種の変更・離した。ST-05）。プレイヤーの入力も AI もここを通る（ARCH-01 A6）。
	 *  溜めの始まりがない（バッファ中の）意図なら、打点の候補を探し始める。
	 *  離した意図は記録するだけ（ストロークは RecoverStart で終わる：DD-03 §6.6）。振っていないときに離したら、すぐ終える（OnStrokeEnded） */
	void SubmitShotIntent(const FShotIntent& Intent);
	/** 予約の取り消し（候補の前に離した・時間切れ・入力の段階の変更）。探すのもやめ、インパクトの前ならストロークも止める。
	 *  後なら終わりまで進むが、その RecoverStart では OnStrokeEnded を知らせず、押し直した次の予約も消さない */
	void CancelShotIntent();
	/** 今の意図。なければ nullptr */
	const FShotIntent* GetShotIntent() const;
	/** 予約で決まった打点（候補が見つかった後）。なければ nullptr */
	const FHitCandidate* GetPlannedHit() const;

	/** 打点の候補が見つかった（押した瞬間を含む。ST-05：入力がバッファから溜め中へ移る） */
	FSimpleMulticastDelegate OnHitCandidateFound;
	/** ストロークが終わった・取り消された（ST-05：入力が待機に戻る） */
	FSimpleMulticastDelegate OnStrokeEnded;
	/** インパクト（ST-05：離した → 確定。DD-03 §6.4・§6.6）。打球はこの知らせの前に済んでいる。空振りでも知らせる */
	FSimpleMulticastDelegate OnStrokeImpact;

	virtual void PostInitializeComponents() override;

protected:
	virtual void BeginPlay() override;

private:
	void HandleCandidateFound(const FHitCandidate& Candidate);
	/** インパクト（DD-03 §6.4 の 8）：意図（球種・溜め）を読み、判定し、打ち、最後に入力へ知らせる */
	void HandleStrokeImpact(const FStrokeImpact& Impact);
	/** 判定と M3 の打球（§6.4 の 4・6・7）：T のボールとスイートスポットの距離が判定の距離以下なら、球種のデータでニュートラルの狙いへ打ち出す */
	void HitBall(const FStrokeImpact& Impact, ETennisShotType Type, double Charge);
	void HandleStrokeFinished();
	void EndStroke(const TCHAR* Reason);
	/** 見た目のデータの髪を、Binding の対象のメッシュを持つ部品（追従する部品、なければ体）に付ける（DD-03 §7） */
	void AttachGrooms(const UTennisPlayerAppearance& Source);
	/** 探索の体の位置を、見た目のデータの焼いた値にする（DD-03 §4.2）。焼いていなければ警告を出してパラメータ表の既定。null も既定 */
	void ApplyReachBody(const UTennisPlayerAppearance* Baked);

	UPROPERTY(VisibleAnywhere, Category = "Tennis")
	TObjectPtr<UTennisReachComponent> Reach;

	UPROPERTY(VisibleAnywhere, Category = "Tennis")
	TObjectPtr<UTennisRacketComponent> Racket;

	UPROPERTY(VisibleAnywhere, Category = "Tennis")
	TObjectPtr<UTennisStrokeComponent> Stroke;

	UPROPERTY(VisibleAnywhere, Transient, Category = "Tennis")
	TArray<TObjectPtr<USkeletalMeshComponent>> Followers;

	UPROPERTY(VisibleAnywhere, Transient, Category = "Tennis")
	TArray<TObjectPtr<UGroomComponent>> Grooms;

	UPROPERTY(VisibleAnywhere, Transient, Category = "Tennis")
	TObjectPtr<const UTennisPlayerAppearance> Appearance;

	/** 見ているボール（FollowBall。打球で打ち出す） */
	UPROPERTY(Transient)
	TObjectPtr<UTennisBallSimComponent> FollowedBall;

	TOptional<FShotIntent> ShotIntent;
	TOptional<FHitCandidate> PlannedHit;
};
