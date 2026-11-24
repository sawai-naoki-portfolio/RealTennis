// W-1.6：コートの座標の変換（DD-01 §1、ARCH-03 U-2）。
#include "CQTest.h"
#include "Court/TennisCourt.h"

TEST_CLASS(TennisCourtSpace, "Tennis.Game.Court")
{
	// 原点に置いたコート：m → cm、軸はそのまま
	TEST_METHOD(AtOrigin_ConvertsMetersToCentimeters)
	{
		const FTransform Identity;
		ASSERT_THAT(IsTrue(ATennisCourt::LocalMetersToWorld(Identity, FVector(11.885, -4.115, 0.914))
			.Equals(FVector(1188.5, -411.5, 91.4), 1e-9)));
		ASSERT_THAT(IsTrue(ATennisCourt::WorldToLocalMeters(Identity, FVector(1188.5, -411.5, 91.4))
			.Equals(FVector(11.885, -4.115, 0.914), 1e-12)));
	}

	// 動かして回したコート：行って戻ると元に戻る。コートの +X はアクタの前方
	TEST_METHOD(MovedAndRotatedCourt_RoundTrips)
	{
		const FTransform Court(FRotator(0.0, 90.0, 0.0), FVector(1000.0, -500.0, 20.0));
		const FVector Local(3.0, 1.5, 0.5);
		const FVector World = ATennisCourt::LocalMetersToWorld(Court, Local);
		ASSERT_THAT(IsTrue(World.Equals(FVector(1000.0 - 150.0, -500.0 + 300.0, 70.0), 1e-9)));
		ASSERT_THAT(IsTrue(ATennisCourt::WorldToLocalMeters(Court, World).Equals(Local, 1e-12)));
	}

	// 向きと大きさだけの量（速度）は、回転と単位だけを変え、位置の移動を含まない
	TEST_METHOD(VectorConversion_RotatesAndScalesUnitsOnly)
	{
		const FTransform Court(FRotator(0.0, 90.0, 0.0), FVector(1000.0, -500.0, 20.0));
		const FVector LocalVelocity(5.5, 0.0, 1.0);  // m/s
		const FVector World = ATennisCourt::LocalMetersToWorldVector(Court, LocalVelocity);
		ASSERT_THAT(IsTrue(World.Equals(FVector(0.0, 550.0, 100.0), 1e-9)));
		ASSERT_THAT(IsTrue(ATennisCourt::WorldToLocalMetersVector(Court, World).Equals(LocalVelocity, 1e-12)));
	}

	// アクタの拡大・縮小は無視する（コートは常に実寸）
	TEST_METHOD(ActorScale_IsIgnored)
	{
		const FTransform Scaled(FRotator::ZeroRotator, FVector::ZeroVector, FVector(2.0, 2.0, 2.0));
		ASSERT_THAT(IsTrue(ATennisCourt::LocalMetersToWorld(Scaled, FVector(1.0, 0.0, 0.0)).Equals(FVector(100.0, 0.0, 0.0), 1e-9)));
	}
};
