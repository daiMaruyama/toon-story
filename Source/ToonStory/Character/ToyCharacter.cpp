#include "Character/ToyCharacter.h"

#include "Components/CapsuleComponent.h"
#include "Core/GazeFreezeSubsystem.h"
#include "Core/ToyBoxPlayerState.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace
{
	/** 凍結中にこれ以上ずれていたら巻き戻す（cm）。 */
	static float GToyBoxMaxFreezeDrift = 25.f;
	static FAutoConsoleVariableRef CVarMaxFreezeDrift(
		TEXT("ToyBox.Gaze.MaxFreezeDrift"),
		GToyBoxMaxFreezeDrift,
		TEXT("凍結中のおもちゃがこの距離(cm)以上動いていたら、サーバーが凍結位置へ戻す。"));
}

AToyCharacter::AToyCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// おもちゃなので小さい。カプセルはナビメッシュ・段差・カメラのニアクリップに
	// 全部効くので、変えるなら早い段階で決めること（仕様書「8. 先に決めておきたいこと」）。
	GetCapsuleComponent()->InitCapsuleSize(22.f, 44.f);

	bReplicates = true;
	SetReplicateMovement(true);
}

void AToyCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		DefaultMaxWalkSpeed = Movement->MaxWalkSpeed;
		DefaultMaxAcceleration = Movement->MaxAcceleration;
	}

	if (UGazeFreezeSubsystem* Gaze = UGazeFreezeSubsystem::Get(this))
	{
		Gaze->RegisterToy(this);
	}
}

void AToyCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UGazeFreezeSubsystem* Gaze = UGazeFreezeSubsystem::Get(this))
	{
		Gaze->UnregisterToy(this);
	}

	Super::EndPlay(EndPlayReason);
}

void AToyCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	BindToPlayerState();
}

void AToyCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	BindToPlayerState();
}

void AToyCharacter::BindToPlayerState()
{
	AToyBoxPlayerState* PS = GetPlayerState<AToyBoxPlayerState>();
	if (!PS)
	{
		return;
	}

	if (!PS->OnFrozenChanged.IsAlreadyBound(this, &AToyCharacter::HandleFrozenChanged))
	{
		PS->OnFrozenChanged.AddDynamic(this, &AToyCharacter::HandleFrozenChanged);
	}

	// bind より前に届いていた値を取りこぼさない。
	ApplyServerFreeze(PS->bFrozen);
}

void AToyCharacter::HandleFrozenChanged(bool bNewFrozen)
{
	ApplyServerFreeze(bNewFrozen);
}

void AToyCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 所有クライアントだけが先行凍結を回す。サーバーは EvaluateAll 側で判定する。
	if (IsLocallyControlled() && !HasAuthority())
	{
		if (const UGazeFreezeSubsystem* Gaze = UGazeFreezeSubsystem::Get(this))
		{
			// 「見られている」と思ったら即止まる。
			// 「見られていない」と思っても、動き出すのはサーバーの解除を待つ。
			if (Gaze->IsWatchedByAnyHuman(this))
			{
				ApplyPredictedFreeze(true);
			}
		}
	}
}

void AToyCharacter::GetGazeSamplePoints(FVector (&OutPoints)[3]) const
{
	const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Base = GetActorLocation();

	OutPoints[0] = Base + FVector(0.f, 0.f, HalfHeight * 0.85f);   // 頭
	OutPoints[1] = Base;                                           // 胴
	OutPoints[2] = Base - FVector(0.f, 0.f, HalfHeight * 0.85f);   // 足
}

void AToyCharacter::ApplyServerFreeze(bool bFrozen)
{
	bServerFrozen = bFrozen;

	// サーバーが解除したら、先行凍結も一緒に降ろす。
	// ここを残すとクライアントが自分で止まったまま動けなくなる。
	if (!bFrozen)
	{
		bPredictedFrozen = false;
	}

	RefreshFrozenState();
}

void AToyCharacter::ApplyPredictedFreeze(bool bFrozen)
{
	if (bPredictedFrozen == bFrozen)
	{
		return;
	}

	bPredictedFrozen = bFrozen;
	RefreshFrozenState();
}

void AToyCharacter::RefreshFrozenState()
{
	const bool bFrozenNow = IsEffectivelyFrozen();
	if (bFrozenApplied == bFrozenNow)
	{
		return;
	}

	bFrozenApplied = bFrozenNow;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (!Movement)
	{
		return;
	}

	if (bFrozenNow)
	{
		FreezeAnchorLocation = GetActorLocation();

		if (IsFreezeBlocking(EFreezeBlock::Movement))
		{
			// DisableMovement() ではなく速度を 0 にする方式。
			// 空中や坂で DisableMovement を使うと復帰時に挙動が乱れるため
			// （仕様書「実際に止める方法」）。
			Movement->StopMovementImmediately();
			Movement->MaxWalkSpeed = 0.f;
			Movement->MaxAcceleration = 0.f;
		}
	}
	else
	{
		if (IsFreezeBlocking(EFreezeBlock::Movement))
		{
			Movement->MaxWalkSpeed = DefaultMaxWalkSpeed;
			Movement->MaxAcceleration = DefaultMaxAcceleration;
		}
	}
}

void AToyCharacter::EnforceFreezeAnchor()
{
	if (!HasAuthority() || !bServerFrozen || !IsFreezeBlocking(EFreezeBlock::Movement))
	{
		return;
	}

	const float Drift = FVector::Dist(GetActorLocation(), FreezeAnchorLocation);
	if (Drift > GToyBoxMaxFreezeDrift)
	{
		// 改造クライアントが先行凍結を無効化していても、ここで戻されるので凍結は守られる。
		SetActorLocation(FreezeAnchorLocation, false, nullptr, ETeleportType::TeleportPhysics);

		if (UCharacterMovementComponent* Movement = GetCharacterMovement())
		{
			Movement->StopMovementImmediately();
		}
	}
}
