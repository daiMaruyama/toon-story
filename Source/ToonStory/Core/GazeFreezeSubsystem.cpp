#include "Core/GazeFreezeSubsystem.h"

#include "Character/HumanCharacter.h"
#include "Character/ToyCharacter.h"
#include "CollisionQueryParams.h"
#include "Core/ToyBoxPlayerState.h"
#include "Core/ToyBoxTypes.h"
#include "Engine/World.h"
#include "TimerManager.h"

namespace
{
	/**
	 * 視野角の半分（度）。
	 *
	 * 人間のカメラ FOV をそのまま使うと画面の端に映った瞬間に止まり、
	 * おもちゃ側からは理不尽に感じる。実 FOV より 5〜10 度内側に絞る
	 * （仕様書「視野角は画角より少し狭くする」）。実測で詰めること。
	 */
	static float GGazeHalfFovDegrees = 40.f;
	static FAutoConsoleVariableRef CVarGazeHalfFov(
		TEXT("ToyBox.Gaze.HalfFovDegrees"),
		GGazeHalfFovDegrees,
		TEXT("視線判定の視野角の半分（度）。カメラ FOV より少し狭くする。"));

	/** 解除の遅延（秒）。凍結は即時、解除だけ遅らせるのが「人間有利」の実装。 */
	static float GGazeUnfreezeDelay = 0.25f;
	static FAutoConsoleVariableRef CVarGazeUnfreezeDelay(
		TEXT("ToyBox.Gaze.UnfreezeDelay"),
		GGazeUnfreezeDelay,
		TEXT("視線が外れてから凍結が解けるまでの秒数。凍結側は常に即時。"));

	/** 判定の間隔（秒）。毎フレームは回さない。 */
	static float GGazeEvaluateInterval = 0.05f;
	static FAutoConsoleVariableRef CVarGazeInterval(
		TEXT("ToyBox.Gaze.EvaluateInterval"),
		GGazeEvaluateInterval,
		TEXT("サーバーが視線判定を回す間隔（秒）。変更は次のマップ読み込みから効く。"));
}

UGazeFreezeSubsystem* UGazeFreezeSubsystem::Get(const UObject* WorldContext)
{
	if (!WorldContext)
	{
		return nullptr;
	}

	const UWorld* World = WorldContext->GetWorld();
	return World ? World->GetSubsystem<UGazeFreezeSubsystem>() : nullptr;
}

void UGazeFreezeSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// 判定を成立させるのはサーバーだけ。クライアントでは登録簿としてだけ働く。
	if (InWorld.GetNetMode() == NM_Client)
	{
		return;
	}

	InWorld.GetTimerManager().SetTimer(
		EvaluateTimerHandle,
		FTimerDelegate::CreateUObject(this, &UGazeFreezeSubsystem::EvaluateAll),
		GGazeEvaluateInterval,
		true);
}

void UGazeFreezeSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EvaluateTimerHandle);
	}

	CachedToys.Reset();
	CachedHumans.Reset();

	Super::Deinitialize();
}

void UGazeFreezeSubsystem::RegisterToy(AToyCharacter* Toy)
{
	if (Toy)
	{
		CachedToys.AddUnique(Toy);
	}
}

void UGazeFreezeSubsystem::UnregisterToy(AToyCharacter* Toy)
{
	CachedToys.RemoveAll([Toy](const TWeakObjectPtr<AToyCharacter>& Entry)
	{
		return !Entry.IsValid() || Entry.Get() == Toy;
	});
}

void UGazeFreezeSubsystem::RegisterHuman(AHumanCharacter* Human)
{
	if (Human)
	{
		CachedHumans.AddUnique(Human);
	}
}

void UGazeFreezeSubsystem::UnregisterHuman(AHumanCharacter* Human)
{
	CachedHumans.RemoveAll([Human](const TWeakObjectPtr<AHumanCharacter>& Entry)
	{
		return !Entry.IsValid() || Entry.Get() == Human;
	});
}

bool UGazeFreezeSubsystem::IsWatchedByHuman(const AToyCharacter* Toy, const AHumanCharacter* Human) const
{
	if (!Toy || !Human)
	{
		return false;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	FVector EyeLocation;
	FVector ViewDirection;
	Human->GetGazeOrigin(EyeLocation, ViewDirection);

	// --- 1. 視野角の粗判定 ---
	const FVector ToToy = Toy->GetActorLocation() - EyeLocation;
	const FVector ToToyDir = ToToy.GetSafeNormal();
	if (ToToyDir.IsNearlyZero())
	{
		// 完全に重なっている。見えている扱いにする（人間有利）。
		return true;
	}

	const float CosHalfFov = FMath::Cos(FMath::DegreesToRadians(GGazeHalfFovDegrees));
	if (FVector::DotProduct(ViewDirection, ToToyDir) < CosHalfFov)
	{
		return false;
	}

	// --- 2. 遮蔽判定（頭・胴・足の3点） ---
	// 1点トレースだと足元だけ物陰に入ったときに凍結と解除がガタつく。
	// 1点でも通れば「見えている」とする（人間有利の方針と一致）。
	FVector Samples[3];
	Toy->GetGazeSamplePoints(Samples);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ToyBoxGazeCheck), false, Human);
	Params.AddIgnoredActor(Toy);

	for (const FVector& Point : Samples)
	{
		if (!World->LineTraceTestByChannel(EyeLocation, Point, ECC_Visibility, Params))
		{
			return true;
		}
	}

	return false;
}

bool UGazeFreezeSubsystem::IsWatchedByAnyHuman(const AToyCharacter* Toy) const
{
	for (const TWeakObjectPtr<AHumanCharacter>& Entry : CachedHumans)
	{
		const AHumanCharacter* Human = Entry.Get();
		if (Human && IsWatchedByHuman(Toy, Human))
		{
			return true;
		}
	}

	return false;
}

void UGazeFreezeSubsystem::EvaluateAll()
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const double Now = World->GetTimeSeconds();

	for (int32 Index = CachedToys.Num() - 1; Index >= 0; --Index)
	{
		AToyCharacter* Toy = CachedToys[Index].Get();
		if (!Toy)
		{
			CachedToys.RemoveAtSwap(Index);
			continue;
		}

		AToyBoxPlayerState* PS = Toy->GetPlayerState<AToyBoxPlayerState>();
		if (!PS)
		{
			continue;
		}

		// 箱の中や運搬中は視線の対象外。凍結を持ち越さない。
		if (PS->ToyState != EToyState::Free)
		{
			PS->SetFrozen(false);
			continue;
		}

		const bool bWatched = IsWatchedByAnyHuman(Toy);

		if (bWatched)
		{
			Toy->LastWatchedTime = Now;
			PS->SetFrozen(true);                                  // 即時
		}
		else if (PS->bFrozen && (Now - Toy->LastWatchedTime) > GGazeUnfreezeDelay)
		{
			PS->SetFrozen(false);                                 // 0.25 秒後
		}

		// 先行凍結は改造クライアントなら無効化できるので、位置は必ずサーバーが見張る。
		Toy->EnforceFreezeAnchor();
	}
}
