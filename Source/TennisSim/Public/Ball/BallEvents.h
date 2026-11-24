// ボールのイベント（DD-01 §8、クラス図 §1）。Step が時刻の順に出し、ルール → 統計 → 演出の順に配信される。
// 恒久施設・体は、それを解く作業で種類と属性を加える。
#pragma once

#include "CoreMinimal.h"
#include "BallEvents.generated.h"

enum class EBallEventType : uint8
{
	Bounce,    // 地面との衝突
	NetCross,  // ネットの面（ポストの内側）を越えた
	NetTouch,  // ネットの本体か白帯に触れた
	PostHit,   // ネットポストかシングルススティックに当たった
};

/** ログ用の名前。 */
inline const TCHAR* LexToString(EBallEventType Type)
{
	switch (Type)
	{
	case EBallEventType::Bounce:   return TEXT("Bounce");
	case EBallEventType::NetCross: return TEXT("NetCross");
	case EBallEventType::NetTouch: return TEXT("NetTouch");
	case EBallEventType::PostHit:  return TEXT("PostHit");
	}
	return TEXT("Unknown");
}

enum class ENetPart : uint8
{
	None,
	Body,          // 網
	Cord,          // 白帯
	SinglesStick,  // シングルススティック（PostHit）
	Post,          // ネットポスト（PostHit）
};

USTRUCT()
struct TENNISSIM_API FBallEvent
{
	GENERATED_BODY()

	EBallEventType Type = EBallEventType::Bounce;
	double  Time = 0.0;                   // s（衝突の時刻）
	FVector Pos  = FVector::ZeroVector;   // m（衝突の瞬間のボールの中心）
	FVector Vel  = FVector::ZeroVector;   // m/s（衝突の後）
	FVector Spin = FVector::ZeroVector;   // rad/s（衝突の後）
	double  NormalSpeed  = 0.0;           // m/s（Bounce：衝突の前の法線方向の速さ、正の値）
	double  ContactPatch = 0.0;           // m（Bounce：接地の半径。イン／アウトの判定に使う：DD-01 §4.3）
	double  NetClearance = 0.0;           // m（NetCross：ボールの底とネットの上端の差。負ならネットの上端より下）
	ENetPart NetPart     = ENetPart::None; // NetTouch・PostHit：触れた部位
};
