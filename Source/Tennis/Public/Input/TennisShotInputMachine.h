// 球種の入力の状態機械（ST-05、操作仕様 §3.1・§4.1）。ワールドにも Enhanced Input にも依存しない（時刻は呼ぶ側が渡す）。
// UTennisShotInputComponent が、押す・離す・修飾をここに渡し、意図が変わったら選手（SubmitShotIntent）に知らせる。
// 押したまま（溜め中）インパクトが来たら、そこで自動で離した扱いにする（操作仕様 §4.1）。球種の変更の期限（t* − 0.25 s）は W-4.2 で加える。
#pragma once

#include "CoreMinimal.h"
#include "Shot/ShotTypes.h"

/** ボタン 1 つの球種：修飾なしと、技の修飾（IA_Mod_Technique）つき（操作仕様 §3.1） */
struct FShotButtonTypes
{
	ETennisShotType Plain = ETennisShotType::Topspin;
	ETennisShotType WithTechnique = ETennisShotType::Topspin;
};

/** ST-05 の状態 */
enum class EShotInputState : uint8
{
	Idle,       // 待機
	Buffered,   // 押した。打点の候補を待つ
	Charging,   // 予約済み・溜め中
	Released,   // 離した。インパクトを待つ
	Committed,  // インパクト済み。ストロークの終わりを待つ
};

/** ログ用の名前 */
TENNIS_API const TCHAR* LexToString(EShotInputState State);

class TENNIS_API FTennisShotInputMachine
{
public:
	static constexpr double DefaultBufferSeconds = 1.2;  // 入力バッファ（DATA-02 §5）

	/** BufferSeconds が正の数でなければ、既定の値を使う（警告を出す） */
	explicit FTennisShotInputMachine(double InBufferSeconds = DefaultBufferSeconds);

	// どの関数も、状態か意図が変わったときに true を返す（呼ぶ側は、そのときだけ選手に知らせる）

	/** 押した。待機 → バッファ。バッファ・溜め中に別のボタン → 球種を変える（押しているボタンも替わる） */
	bool Press(int32 Button, const FShotButtonTypes& Types, double Now);
	/** 離した。押しているボタンのときだけ：バッファ → 待機、溜め中 → 離した（離した時刻を記録する） */
	bool Release(int32 Button, double Now);
	/** 技の修飾を押した・離した。どの状態でも覚え、バッファ・溜め中なら球種を決め直す（押す順番に依存しない） */
	bool SetTechnique(bool bHeld);
	/** 打点の候補が見つかった（予約できた）：バッファ → 溜め中。溜めの始まりは Now */
	bool CandidateFound(double Now);
	/** インパクト：離した → 確定。押したまま（溜め中）なら、自動で離した扱い（意図の bAutoReleased）で確定へ（操作仕様 §4.1） */
	bool Impact();
	/** ストロークが終わった・取り消された：溜め中（押したまま）・離した・確定 → 待機。押したまま終わっても、次の押下が効く（DD-03 §6.6） */
	bool StrokeEnded();
	/** 時間切れ：バッファのまま BufferSeconds を超えた → 待機 */
	bool Update(double Now);
	/** 取り消し（入力の段階が変わったなど）：どの状態からも → 待機 */
	bool Cancel();

	EShotInputState GetState() const { return State; }
	/** 今の意図。待機中は前の意図が残っているが、使わない */
	const FShotIntent& GetIntent() const { return Intent; }

private:
	/** バッファ・溜め中なら、押しているボタンと修飾から球種を決め直す。変わったら true */
	bool ResolveType();
	void ToIdle();

	double          BufferSeconds;
	EShotInputState State = EShotInputState::Idle;
	FShotIntent     Intent;
	int32           HeldButton = INDEX_NONE;
	FShotButtonTypes HeldTypes;
	double          PressTime = 0.0;  // s（バッファの時間切れの起点）
	bool            bTechniqueHeld = false;
};
