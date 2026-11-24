// 打点の探索（DD-03 §4、ACT-03）：Engine に依存しない純粋な関数。探索は UTennisReachComponent から、選び直しはストロークの側から呼ぶ。
// 座標はコート局所（m）。選手の側は SideSign（−1：X < 0 の側、+1：X > 0 の側）。値の初期値と出典は DATA-02 §4.1。
#pragma once

#include "CoreMinimal.h"
#include "Ball/TennisBallPredictor.h"
#include "Movement/TennisMovement.h"
#include "Shot/ShotTypes.h"
#include "TennisReach.generated.h"

UENUM(BlueprintType)
enum class EStrokeSide : uint8
{
	Forehand,
	Backhand,
};

/** 候補の種類（DD-03 §4.5）。 */
UENUM(BlueprintType)
enum class EHitKind : uint8
{
	Groundstroke,
	Volley,
	HalfVolley,
	Smash,
};

UENUM(BlueprintType)
enum class EBackhandStyle : uint8
{
	OneHanded,
	TwoHanded,
};

/** 探索する選手（DD-03 §4.4）。 */
struct FReachQuery
{
	FVector2D         Position = FVector2D::ZeroVector;  // m
	FVector2D         Velocity = FVector2D::ZeroVector;  // m/s
	double            BoostTimeLeft = 0.0;               // s（初動ブーストの残り）
	double            SideSign = -1.0;
	ETennisHandedness Handedness = ETennisHandedness::Right;
	EBackhandStyle    Backhand = EBackhandStyle::TwoHanded;
	double            Now = 0.0;           // s（シミュレーションの時刻。これより前のサンプルは使わない）
	double            ReactionTime = 0.0;  // s（プレイヤー 0、AI は難易度：DD-05）
	double            PrepTime = 0.0;      // s（ストロークの準備。ストロークのデータがなければ FReachParams::DefaultPrepTime）
};

struct FStrokeSyncParams;

/** 探索の値（DD-03 §4.2・§4.3・§4.5、DATA-02 §4.1）。 */
USTRUCT(BlueprintType)
struct TENNISSIM_API FReachParams
{
	GENERATED_BODY()

	/** 体の横の距離（m）。フォア・バックと前の距離は、選手が見た目のデータの焼いた値（ReachBody）を入れる（DD-03 §4.2・§7）。
	 *  ここの既定は、見た目のデータに値がないときのもので、仮の男性の焼いた値（DATA-02 §4.1。仮の男性の焼いた値と 5 cm 以内：T-STRK-011） */
	UPROPERTY(EditAnywhere, Category = "Body", meta = (ClampMin = "0"))
	double ForehandSide = 0.82;

	/** 両手のバック。M3 は片手の仮のストロークを使うので、片手と同じ値 */
	UPROPERTY(EditAnywhere, Category = "Body", meta = (ClampMin = "0"))
	double TwoHandedBackhandSide = 0.62;

	/** 片手のバック */
	UPROPERTY(EditAnywhere, Category = "Body", meta = (ClampMin = "0"))
	double OneHandedBackhandSide = 0.62;

	UPROPERTY(EditAnywhere, Category = "Body", meta = (ClampMin = "0"))
	double VolleySide = 0.80;

	UPROPERTY(EditAnywhere, Category = "Body", meta = (ClampMin = "0"))
	double SmashSide = 0.25;

	/** 体の前で打つ距離（m）。どの種類の候補にも同じ値（M3。種類ごとの値は M9） */
	UPROPERTY(EditAnywhere, Category = "Body", meta = (ClampMin = "0"))
	double Forward = 0.38;

	/** 打点と今の位置の横の差がこれより小さければ、フォアを先に試す（回り込み） */
	UPROPERTY(EditAnywhere, Category = "Body", meta = (ClampMin = "0"))
	double CenterBand = 0.30;

	/** 高さの帯（ボールの中心、m） */
	UPROPERTY(EditAnywhere, Category = "Bands")
	double GroundMinZ = 0.25;

	UPROPERTY(EditAnywhere, Category = "Bands")
	double GroundMaxZ = 1.7;

	UPROPERTY(EditAnywhere, Category = "Bands")
	double HalfVolleyMaxZ = 0.35;

	/** ハーフボレーにする、バウンドからの時間（s） */
	UPROPERTY(EditAnywhere, Category = "Bands", meta = (ClampMin = "0"))
	double HalfVolleyWindow = 0.15;

	UPROPERTY(EditAnywhere, Category = "Bands")
	double VolleyMinZ = 0.3;

	UPROPERTY(EditAnywhere, Category = "Bands")
	double VolleyMaxZ = 2.2;

	UPROPERTY(EditAnywhere, Category = "Bands")
	double SmashMinZ = 2.2;

	UPROPERTY(EditAnywhere, Category = "Bands")
	double SmashMaxZ = 3.2;

	/** 打ちやすい高さ（高さの点が 1） */
	UPROPERTY(EditAnywhere, Category = "Bands")
	double IdealMinZ = 0.8;

	UPROPERTY(EditAnywhere, Category = "Bands")
	double IdealMaxZ = 1.2;

	/** 点数の重み（DD-03 §4.3） */
	UPROPERTY(EditAnywhere, Category = "Score", meta = (ClampMin = "0"))
	double HeightWeight = 0.35;

	UPROPERTY(EditAnywhere, Category = "Score", meta = (ClampMin = "0"))
	double MarginWeight = 0.25;

	UPROPERTY(EditAnywhere, Category = "Score", meta = (ClampMin = "0"))
	double EarlyWeight = 0.15;

	UPROPERTY(EditAnywhere, Category = "Score", meta = (ClampMin = "0"))
	double TravelWeight = 0.15;

	UPROPERTY(EditAnywhere, Category = "Score", meta = (ClampMin = "0"))
	double PostureWeight = 0.10;

	/** 余裕の点が満点になる余裕（s） */
	UPROPERTY(EditAnywhere, Category = "Score", meta = (ClampMin = "0.001"))
	double FullMargin = 0.15;

	/** ストレッチ：使える時間で目標位置まで残りこの距離（m）以内なら候補にし、点数を StretchScoreFactor 倍にする */
	UPROPERTY(EditAnywhere, Category = "Score", meta = (ClampMin = "0"))
	double StretchReach = 0.6;

	UPROPERTY(EditAnywhere, Category = "Score", meta = (ClampMin = "0", ClampMax = "1"))
	double StretchScoreFactor = 0.5;

	/** ストロークのデータがないときの準備の時間（s。W-3.4 で UStrokeData::WindupTime に置き換える） */
	UPROPERTY(EditAnywhere, Category = "Timing", meta = (ClampMin = "0"))
	double DefaultPrepTime = 0.30;
};

/** 打点の候補（DD-03 §4.4）。 */
struct FHitCandidate
{
	double      Time = 0.0;                          // s（インパクトの時刻）
	FVector     BallPos = FVector::ZeroVector;       // m
	FVector     BallVel = FVector::ZeroVector;       // m/s
	FVector2D   BodyTarget = FVector2D::ZeroVector;  // m（体の目標位置）
	EStrokeSide Side = EStrokeSide::Forehand;
	EHitKind    Kind = EHitKind::Groundstroke;
	bool        bStretch = false;
	double      TimeToReach = 0.0;  // s
	double      Score = 0.0;
};

namespace TennisReach
{
	/**
	 * Q の選手が Target まで走る時間（s。DD-03 §4.1）。移動のモデル（TennisMovement::Step）と同じ規則で、目標の向きの成分だけを見る：
	 * 目標と逆向きの速度は、方向転換の損失（TurnKeep）の後に加速で向きを変える。最高速度を超えていれば Brake で落とす。
	 * 後退・初動ブーストは移動と同じ倍率。到着の速さは問わない（止まる時間を含まない）。
	 * 数でない入力は到達できないとして TNumericLimits<double>::Max() を返す（警告を出す）。
	 */
	TENNISSIM_API double TimeToReach(const FReachQuery& Q, const FVector2D& Target, const FMovementParams& M);

	/**
	 * 予測軌道 Trajectory の、Q の選手が打てる打点の候補を Out に書く（ACT-03、DD-03 §4.5）。点数の降順、同点は早い順。Out の前の中身は消す。
	 * 自分の側の、Now より後・2 回目のバウンドより前のサンプルを調べる。体の目標位置が移動の範囲（TennisMovement::ClampToArea）の外なら捨てる。
	 */
	TENNISSIM_API void FindCandidates(const FTrajectory& Trajectory, const FReachQuery& Q, const FMovementParams& M, const FReachParams& P,
		TArray<FHitCandidate>& Out);

	/**
	 * ストロークが決まった後の打点の選び直し（DD-03 §6.2）。候補 Candidate の時刻の前後 Sync.HitRefineWindow 秒の中から、ボールの高さが
	 * ストロークのスイートスポットの高さ（SweetSpotOffset.Z）にいちばん近いサンプルを打点にする。同じ近さなら、候補の時刻に近いほう。
	 * 対象は、Now より後・自分の側・候補と同じバウンドの間・同じ種類（高さの帯）で、体の目標に間に合う（ストレッチにしない）・移動の範囲の中のサンプル。
	 * 候補より高さの遠いサンプルは選ばない。点数（Score）は探索のときのまま。
	 * 体の目標は、狙い AimPoint への体の向き（TennisStrokeTiming::FacingYaw）で、スイートスポットの水平のずれ（X が前、Y が右。右利きの値で、
	 * 左利きは左右を反転する）がボールに重なる位置。対象がなければ、候補の時刻のまま、体の目標だけをこの式で置き直す（bStretch は候補のまま）。
	 * 入力が数でなければ（Q・SweetSpotOffset・候補の時刻と高さ・体の目標）、警告を出して Candidate をそのまま返す（体の目標も置き直さない）。
	 */
	TENNISSIM_API FHitCandidate RefineForStroke(const FTrajectory& Trajectory, const FHitCandidate& Candidate, const FVector& SweetSpotOffset,
		const FVector2D& AimPoint, const FReachQuery& Q, const FMovementParams& M, const FReachParams& P, const FStrokeSyncParams& Sync);
}
