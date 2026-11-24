// W-3.2：球種の入力の状態機械（ST-05、操作仕様 §3.1・§4.1）。
#include "CQTest.h"
#include "Input/TennisShotInputMachine.h"

namespace
{
	constexpr int32 TopspinButton = 0;
	constexpr int32 SliceButton = 2;
	const FShotButtonTypes TopspinTypes{ ETennisShotType::Topspin, ETennisShotType::Topspin };
	const FShotButtonTypes SliceTypes{ ETennisShotType::Slice, ETennisShotType::Drop };
}

TEST_CLASS(TennisShotInputMachine, "Tennis.Game.Input.ShotInputMachine")
{
	// 押す → バッファ（溜めはまだ）。打点の候補が来る前に離すと、予約は消える
	TEST_METHOD(ReleaseBeforeCandidate_ReturnsToIdle)
	{
		FTennisShotInputMachine Machine;
		ASSERT_THAT(IsTrue(Machine.Press(TopspinButton, TopspinTypes, 1.0)));
		ASSERT_THAT(IsTrue(Machine.GetState() == EShotInputState::Buffered));
		ASSERT_THAT(IsTrue(Machine.GetIntent().Type == ETennisShotType::Topspin));
		ASSERT_THAT(IsFalse(Machine.GetIntent().ChargeStartTime.IsSet()));

		ASSERT_THAT(IsTrue(Machine.Release(TopspinButton, 1.1)));
		ASSERT_THAT(IsTrue(Machine.GetState() == EShotInputState::Idle));
		ASSERT_THAT(AreEqual(FString(TEXT("Idle")), FString(LexToString(Machine.GetState()))));
	}

	// 候補が見つかる → 溜め中（溜めの始まりを記録）→ 離す（時刻を記録）→ インパクト → ストロークの終わりで待機
	TEST_METHOD(FullStroke_RecordsChargeStartAndRelease)
	{
		FTennisShotInputMachine Machine;
		Machine.Press(TopspinButton, TopspinTypes, 1.0);
		ASSERT_THAT(IsTrue(Machine.CandidateFound(1.05)));
		ASSERT_THAT(IsTrue(Machine.GetState() == EShotInputState::Charging));
		ASSERT_THAT(IsNear(1.05, Machine.GetIntent().ChargeStartTime.Get(0.0), 1e-12));
		ASSERT_THAT(IsFalse(Machine.GetIntent().ReleaseTime.IsSet()));

		ASSERT_THAT(IsTrue(Machine.Release(TopspinButton, 1.6)));
		ASSERT_THAT(IsTrue(Machine.GetState() == EShotInputState::Released));
		ASSERT_THAT(IsNear(1.6, Machine.GetIntent().ReleaseTime.Get(0.0), 1e-12));

		ASSERT_THAT(IsTrue(Machine.Impact()));
		ASSERT_THAT(IsTrue(Machine.GetState() == EShotInputState::Committed));
		ASSERT_THAT(IsFalse(Machine.GetIntent().bAutoReleased));  // 離してからのインパクトは、自動の離しではない
		ASSERT_THAT(IsFalse(Machine.Press(TopspinButton, TopspinTypes, 1.7)));  // 打った後の押下は次のストロークまで受けない
		ASSERT_THAT(IsTrue(Machine.StrokeEnded()));
		ASSERT_THAT(IsTrue(Machine.GetState() == EShotInputState::Idle));
	}

	// 押したままインパクトが来たら、自動で離した扱いで確定する（操作仕様 §4.1：判定は t* + W_late）。その後に離しても何も起きない。
	// ストロークが終わったら待機に戻り、次の押下が効く。バッファ中（ストロークがない）のインパクト・終わりの知らせは無視する
	TEST_METHOD(HeldAtTheImpact_CommitsAsAutoReleased)
	{
		FTennisShotInputMachine Machine;
		Machine.Press(TopspinButton, TopspinTypes, 1.0);
		ASSERT_THAT(IsFalse(Machine.Impact()));       // バッファ中
		ASSERT_THAT(IsFalse(Machine.StrokeEnded()));
		ASSERT_THAT(IsTrue(Machine.GetState() == EShotInputState::Buffered));
		Machine.CandidateFound(1.05);
		ASSERT_THAT(IsTrue(Machine.Impact()));
		ASSERT_THAT(IsTrue(Machine.GetState() == EShotInputState::Committed));
		ASSERT_THAT(IsTrue(Machine.GetIntent().bAutoReleased && !Machine.GetIntent().ReleaseTime.IsSet()));
		ASSERT_THAT(IsFalse(Machine.Release(TopspinButton, 2.0)));  // インパクトの後に離しても何も起きない
		ASSERT_THAT(IsFalse(Machine.SetTechnique(true)));           // 修飾も球種も変わらない
		ASSERT_THAT(IsFalse(Machine.Press(SliceButton, SliceTypes, 2.1)));
		ASSERT_THAT(IsTrue(Machine.GetIntent().Type == ETennisShotType::Topspin));
		ASSERT_THAT(IsTrue(Machine.GetState() == EShotInputState::Committed));
		ASSERT_THAT(IsTrue(Machine.StrokeEnded()));
		ASSERT_THAT(IsTrue(Machine.GetState() == EShotInputState::Idle));
		ASSERT_THAT(IsTrue(Machine.Press(TopspinButton, TopspinTypes, 2.6)));
		ASSERT_THAT(IsTrue(Machine.GetState() == EShotInputState::Buffered && !Machine.GetIntent().bAutoReleased));  // 新しい意図
	}

	// 押したまま、インパクトの前にストロークが終わった（始められなかった・取り消された）ときも待機に戻り、次の押下が効く（DD-03 §6.6）
	TEST_METHOD(HeldThroughTheStrokeEnd_ReturnsToIdleAndAcceptsTheNextPress)
	{
		FTennisShotInputMachine Machine;
		Machine.Press(TopspinButton, TopspinTypes, 1.0);
		Machine.CandidateFound(1.05);
		ASSERT_THAT(IsTrue(Machine.StrokeEnded()));
		ASSERT_THAT(IsTrue(Machine.GetState() == EShotInputState::Idle));
		ASSERT_THAT(IsFalse(Machine.Release(TopspinButton, 2.5)));  // 終わった後に離しても何も起きない
		ASSERT_THAT(IsTrue(Machine.Press(TopspinButton, TopspinTypes, 2.6)));
		ASSERT_THAT(IsTrue(Machine.GetState() == EShotInputState::Buffered));
	}

	// 技の修飾は押す順番に依存しない。溜めの間は決め直し、離した後は変わらない
	TEST_METHOD(Technique_IsOrderIndependentUntilRelease)
	{
		FTennisShotInputMachine Before;
		Before.SetTechnique(true);
		Before.Press(SliceButton, SliceTypes, 0.0);
		ASSERT_THAT(IsTrue(Before.GetIntent().Type == ETennisShotType::Drop));

		FTennisShotInputMachine After;
		After.Press(SliceButton, SliceTypes, 0.0);
		ASSERT_THAT(IsTrue(After.GetIntent().Type == ETennisShotType::Slice));
		ASSERT_THAT(IsTrue(After.SetTechnique(true)));
		ASSERT_THAT(IsTrue(After.GetIntent().Type == ETennisShotType::Drop));
		After.CandidateFound(0.1);
		ASSERT_THAT(IsTrue(After.SetTechnique(false)));
		ASSERT_THAT(IsTrue(After.GetIntent().Type == ETennisShotType::Slice));

		After.SetTechnique(true);
		After.Release(SliceButton, 0.5);
		ASSERT_THAT(IsFalse(After.SetTechnique(false)));  // 離した瞬間の組み合わせ（Drop）で確定
		ASSERT_THAT(IsTrue(After.GetIntent().Type == ETennisShotType::Drop));
	}

	// 別のボタンを押すと球種が替わり、離して確定するのは最後に押したボタンだけ
	TEST_METHOD(OtherButton_ChangesTypeAndTakesOverTheRelease)
	{
		FTennisShotInputMachine Machine;
		Machine.Press(TopspinButton, TopspinTypes, 0.0);
		Machine.CandidateFound(0.0);
		ASSERT_THAT(IsTrue(Machine.Press(SliceButton, SliceTypes, 0.2)));
		ASSERT_THAT(IsTrue(Machine.GetIntent().Type == ETennisShotType::Slice));
		ASSERT_THAT(IsNear(0.0, Machine.GetIntent().ChargeStartTime.Get(-1.0), 1e-12));  // 溜めは続く

		ASSERT_THAT(IsFalse(Machine.Release(TopspinButton, 0.3)));
		ASSERT_THAT(IsTrue(Machine.GetState() == EShotInputState::Charging));
		ASSERT_THAT(IsTrue(Machine.Release(SliceButton, 0.4)));
		ASSERT_THAT(IsTrue(Machine.GetState() == EShotInputState::Released));
	}

	// バッファは押してから 1.2 s を超えたら消える（ST-05 の時間切れ）。ちょうど 1.2 s では残る
	TEST_METHOD(Buffer_TimesOutAfterBufferSeconds)
	{
		FTennisShotInputMachine Machine;
		Machine.Press(TopspinButton, TopspinTypes, 10.0);
		ASSERT_THAT(IsFalse(Machine.Update(11.2)));
		ASSERT_THAT(IsTrue(Machine.GetState() == EShotInputState::Buffered));
		ASSERT_THAT(IsTrue(Machine.Update(11.2001)));
		ASSERT_THAT(IsTrue(Machine.GetState() == EShotInputState::Idle));
		ASSERT_THAT(IsFalse(Machine.Release(TopspinButton, 11.3)));  // 時間切れの後に離しても何も起きない
	}

	// 取り消し（入力の段階の変更）は、どの状態からも待機に戻す
	TEST_METHOD(Cancel_ReturnsToIdleFromAnyState)
	{
		FTennisShotInputMachine Machine;
		ASSERT_THAT(IsFalse(Machine.Cancel()));
		Machine.Press(TopspinButton, TopspinTypes, 0.0);
		Machine.CandidateFound(0.1);
		ASSERT_THAT(IsTrue(Machine.Cancel()));
		ASSERT_THAT(IsTrue(Machine.GetState() == EShotInputState::Idle));
		ASSERT_THAT(IsFalse(Machine.Release(TopspinButton, 0.2)));
	}

	// 不正な入力は無視する：負のボタン・数でない時刻。バッファの長さが不正なら既定の 1.2 s
	TEST_METHOD(InvalidInput_IsIgnored)
	{
		FTennisShotInputMachine Machine(0.0);
		const double NaN = std::numeric_limits<double>::quiet_NaN();
		ASSERT_THAT(IsFalse(Machine.Press(-1, TopspinTypes, 0.0)));
		ASSERT_THAT(IsFalse(Machine.Press(TopspinButton, TopspinTypes, NaN)));
		ASSERT_THAT(IsTrue(Machine.GetState() == EShotInputState::Idle));

		Machine.Press(TopspinButton, TopspinTypes, 0.0);
		ASSERT_THAT(IsFalse(Machine.CandidateFound(NaN)));
		ASSERT_THAT(IsFalse(Machine.Update(1.1)));
		ASSERT_THAT(IsTrue(Machine.Update(1.3)));
	}
};
