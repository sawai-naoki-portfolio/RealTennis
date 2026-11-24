#include "Input/TennisShotInputMachine.h"

#include "TennisGameLog.h"

const TCHAR* LexToString(EShotInputState State)
{
	switch (State)
	{
	case EShotInputState::Idle:      return TEXT("Idle");
	case EShotInputState::Buffered:  return TEXT("Buffered");
	case EShotInputState::Charging:  return TEXT("Charging");
	case EShotInputState::Released:  return TEXT("Released");
	case EShotInputState::Committed: return TEXT("Committed");
	}
	return TEXT("Unknown");
}

FTennisShotInputMachine::FTennisShotInputMachine(double InBufferSeconds)
	: BufferSeconds(InBufferSeconds)
{
	if (!FMath::IsFinite(BufferSeconds) || BufferSeconds <= 0.0)
	{
		UE_LOG(LogTennisInput, Warning, TEXT("[ShotInput] invalid buffer %f s (using %.2f s)"), InBufferSeconds, DefaultBufferSeconds);
		BufferSeconds = DefaultBufferSeconds;
	}
}

bool FTennisShotInputMachine::Press(int32 Button, const FShotButtonTypes& Types, double Now)
{
	if (Button < 0 || !FMath::IsFinite(Now))
	{
		UE_LOG(LogTennisInput, Warning, TEXT("[ShotInput] ignored a press (button=%d, time=%f)"), Button, Now);
		return false;
	}
	switch (State)
	{
	case EShotInputState::Idle:
		State = EShotInputState::Buffered;
		Intent = FShotIntent();
		HeldButton = Button;
		HeldTypes = Types;
		PressTime = Now;
		ResolveType();
		return true;
	case EShotInputState::Buffered:
	case EShotInputState::Charging:
		if (Button == HeldButton)
		{
			return false;
		}
		HeldButton = Button;
		HeldTypes = Types;
		return ResolveType();
	default:
		return false;  // 離した後の球種はストロークに任せる
	}
}

bool FTennisShotInputMachine::Release(int32 Button, double Now)
{
	if (Button != HeldButton || !FMath::IsFinite(Now))
	{
		return false;
	}
	if (State == EShotInputState::Buffered)
	{
		ToIdle();
		return true;
	}
	if (State == EShotInputState::Charging)
	{
		State = EShotInputState::Released;
		Intent.ReleaseTime = Now;
		HeldButton = INDEX_NONE;
		return true;
	}
	return false;
}

bool FTennisShotInputMachine::SetTechnique(bool bHeld)
{
	bTechniqueHeld = bHeld;
	return ResolveType();
}

bool FTennisShotInputMachine::CandidateFound(double Now)
{
	if (State != EShotInputState::Buffered || !FMath::IsFinite(Now))
	{
		return false;
	}
	State = EShotInputState::Charging;
	Intent.ChargeStartTime = Now;
	return true;
}

bool FTennisShotInputMachine::Impact()
{
	if (State == EShotInputState::Charging)
	{
		Intent.bAutoReleased = true;  // 押したまま：ここで離した扱い（判定は t* + W_late＝Late の端、溜めは最大）
		State = EShotInputState::Committed;
		return true;
	}
	if (State != EShotInputState::Released)
	{
		return false;
	}
	State = EShotInputState::Committed;
	return true;
}

bool FTennisShotInputMachine::StrokeEnded()
{
	if (State != EShotInputState::Charging && State != EShotInputState::Released && State != EShotInputState::Committed)
	{
		return false;
	}
	ToIdle();
	return true;
}

bool FTennisShotInputMachine::Update(double Now)
{
	if (State != EShotInputState::Buffered || !(Now - PressTime > BufferSeconds))
	{
		return false;
	}
	UE_LOG(LogTennisInput, Log, TEXT("[ShotInput] no hit candidate within %.2f s; dropped %s"),
		BufferSeconds, *UEnum::GetValueAsString(Intent.Type));
	ToIdle();
	return true;
}

bool FTennisShotInputMachine::Cancel()
{
	if (State == EShotInputState::Idle)
	{
		return false;
	}
	ToIdle();
	return true;
}

bool FTennisShotInputMachine::ResolveType()
{
	if (State != EShotInputState::Buffered && State != EShotInputState::Charging)
	{
		return false;
	}
	const ETennisShotType Type = bTechniqueHeld ? HeldTypes.WithTechnique : HeldTypes.Plain;
	if (Type == Intent.Type)
	{
		return false;
	}
	Intent.Type = Type;
	return true;
}

void FTennisShotInputMachine::ToIdle()
{
	State = EShotInputState::Idle;
	HeldButton = INDEX_NONE;
}
