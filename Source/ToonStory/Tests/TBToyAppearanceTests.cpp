// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/ScopeExit.h"
#include "Animation/AnimInstance.h"

#include "Animation/BlendSpace.h"
#include "Character/TBToyCharacter.h"
#include "Character/TBToyCarVisual.h"
#include "Components/ChildActorComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/TBController.h"
#include "Core/TBGameMode.h"
#include "Core/TBGameState.h"
#include "Core/TBPlayerState.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBToyRoundTest, "ToonStory.Appearance.RandomRound",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTBToyRoundTest::RunTest(const FString& Parameters)
{
	UWorld* World = nullptr;
	for (const auto& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::Game && Context.World()->GetNetMode() == NM_Standalone)
		{
			World = Context.World();
		}
	}
	if (!TestNotNull(TEXT("Standalone world"), World))
	{
		return false;
	}
	auto* Host = Cast<ATBController>(World->GetFirstPlayerController());
	auto* Mode = World->GetAuthGameMode<ATBGameMode>();
	auto* State = World->GetGameState<ATBGameState>();
	auto* AddedPlayer = UGameplayStatics::CreatePlayer(World, -1, true);
	// 型の不一致や途中の失敗でも、追加したプレイヤーを残さない。
	ON_SCOPE_EXIT
	{
		if (IsValid(AddedPlayer)) UGameplayStatics::RemovePlayer(AddedPlayer, true);
	};
	auto* Second = Cast<ATBController>(AddedPlayer);
	if (!Host || !Mode || !State || !Second)
	{
		return false;
	}
	auto* Toy = Cast<ATBToyCharacter>(Second->GetPawn());
	if (!TestNotNull(TEXT("Blueprint toy character"), Toy))
	{
		return false;
	}
	TestEqual(TEXT("No selection in lobby"), Toy->SelectedToyIndex, INDEX_NONE);
	if (!TestTrue(TEXT("Toy catalogue is not empty"), !Toy->ToyCatalogue.IsEmpty())) return false;
	for (auto Class : Toy->ToyCatalogue)
	{
		if (!TestNotNull(TEXT("Toy catalogue entry"), Class.Get())) return false;
	}
	// ローカルの2人で、開始時の抽選とラウンド中の再抽選防止を確認する。
	FTBSettings Rules;
	Rules.Humans = 1;
	Rules.Toys = 1;
	Rules.Duration = 600;
	Rules.RequiredItems = 5;
	Mode->SetRules(Host, Rules);
	auto* HumanInfo = Host->GetPlayerState<ATBPlayerState>();
	auto* ToyInfo = Second->GetPlayerState<ATBPlayerState>();
	HumanInfo->Preference = ETBTeam::Human;
	ToyInfo->Preference = ETBTeam::Toy;
	HumanInfo->bReady = ToyInfo->bReady = true;
	Mode->StartRound(Host);
	TestEqual(TEXT("Round started"), State->Phase, ETBPhase::Playing);
	TestTrue(TEXT("Selection made before playing"), Toy->ToyCatalogue.IsValidIndex(Toy->SelectedToyIndex));
	const int32 Selection = Toy->SelectedToyIndex;
	Toy->ChooseRandomToy();
	TestEqual(TEXT("No reroll during round"), Toy->SelectedToyIndex, Selection);
	TestEqual(TEXT("Human has no toy selection"), CastChecked<ATBToyCharacter>(Host->GetPawn())->SelectedToyIndex,
	          INDEX_NONE);
	Toy->Tick(0.f);
	auto* Visual = Toy->ToyVisual->GetChildActor();
	if (TestNotNull(TEXT("Visual spawned"), Visual))
	{
		TestEqual(TEXT("Chosen class applied"), Visual->GetClass(), Toy->ToyCatalogue[Selection].Get());
		TestEqual(TEXT("Visual owner is character"), Visual->GetOwner(), static_cast<AActor*>(Toy));
		// 親の状態更新を繰り返しても、表示Actorを作り直したり仮の箱を出したりしない。
		const FVector GroundOffset = Toy->ToyVisual->GetRelativeLocation();
		Toy->ApplyState();
		Toy->RefreshToyPresentation();
		TestTrue(TEXT("Repeated state keeps visual actor"), Toy->ToyVisual->GetChildActor() == Visual);
		TestFalse(TEXT("Toy hides fallback body"), Toy->Body->IsVisible());
		TestTrue(TEXT("Ground offset remains stable"), Toy->ToyVisual->GetRelativeLocation().Equals(GroundOffset));
		ToyInfo->bFrozen = true;
		Toy->Tick(0.f);
		TInlineComponentArray<USkeletalMeshComponent*> Parts(Visual);
		for (auto* Part : Parts)
		{
			TestTrue(TEXT("Frozen pose"), Part->bPauseAnims);
		}
		ToyInfo->bFrozen = false;
		Toy->Tick(0.f);
		for (auto* Part : Parts)
		{
			TestFalse(TEXT("Pose resumed"), Part->bPauseAnims);
		}
		Toy->Carrier = Cast<ATBCharacter>(Host->GetPawn());
		ToyInfo->ToyState = ETBToyState::Carried;
		Toy->OnRep_Carrier();
		Toy->Tick(0.f);
		for (auto* Part : Parts)
		{
			TestFalse(TEXT("Carried toy visible to owner"), Part->bOwnerNoSee);
			TestTrue(TEXT("Carried pose paused"), Part->bPauseAnims);
		}
		Toy->Carrier = nullptr;
		ToyInfo->ToyState = ETBToyState::Free;
		Toy->OnRep_Carrier();
		Toy->Tick(0.f);
		for (auto* Part : Parts)
		{
			TestTrue(TEXT("First person hides toy again"), Part->bOwnerNoSee);
		}
	}
	// 全候補の材質・アニメーションと、表示専用（衝突・独自同期なし）の構成を確認する。
	for (auto Class : Toy->ToyCatalogue)
	{
		FActorSpawnParameters Spawn;
		Spawn.Owner = Toy;
		auto* Actor = World->SpawnActor<AActor>(Class, Spawn);
		if (!TestNotNull(TEXT("Catalogue actor"), Actor))
		{
			continue;
		}
		Actor->SetOwner(Toy);
		TestFalse(TEXT("Visual actor does not replicate separately"), Actor->GetIsReplicated());
		TInlineComponentArray<USkeletalMeshComponent*> Parts(Actor);
		if (auto* Car = Cast<ATBToyCarVisual>(Actor))
		{
			TestTrue(TEXT("Car has saved mesh"), Car->Car->GetStaticMesh() != nullptr);
			TestEqual(TEXT("Car visual has no collision"), Car->Car->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
		}
		else TestTrue(TEXT("Visual has a skeletal mesh"), Parts.Num() > 0);
		for (auto* Part : Parts)
		{
			TestNotNull(TEXT("Saved mesh"), Part->GetSkeletalMeshAsset());
			TestTrue(TEXT("Animation Blueprint or pose follower"),
			         Part->GetAnimInstance() != nullptr || Part->LeaderPoseComponent.IsValid());
			TestEqual(TEXT("Visual has no collision"), Part->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
			for (int32 Slot = 0; Slot < Part->GetNumMaterials(); ++Slot)
			{
				TestNotNull(TEXT("Saved material"), Part->GetMaterial(Slot));
			}
		}
		Actor->Destroy();
	}
	// 同じPawnでも、おもちゃ→人間→おもちゃの表示が正しく切り替わること。
	ToyInfo->Team = ETBTeam::Human;
	Toy->ApplyState();
	TestNull(TEXT("Human removes toy visual"), Toy->ToyVisual->GetChildActor());
	TestTrue(TEXT("Human model restored"), Toy->ChildMesh.Get() &&
	         Toy->GetMesh()->GetSkeletalMeshAsset() == Toy->ChildMesh.Get());
	ToyInfo->Team = ETBTeam::Toy;
	Toy->ApplyState();
	TestNotNull(TEXT("Toy visual restored after human"), Toy->ToyVisual->GetChildActor());
	TestFalse(TEXT("Restored toy hides fallback body"), Toy->Body->IsVisible());
	Mode->Finish(ETBWinner::Toys, TEXT("test"));
	Mode->ReturnToLobby(Host);
	auto* NextPawn = Cast<ATBToyCharacter>(Second->GetPawn());
	if (TestNotNull(TEXT("Next round pawn"), NextPawn))
	{
		TestEqual(TEXT("Selection resets for next round"), NextPawn->SelectedToyIndex, INDEX_NONE);
	}
	return true;
}
// 手動のポーズ更新では見逃す入力不良を、通常のフレーム更新で検出する。
class FTBToyLocomotionFrames : public IAutomationLatentCommand
{
public:
	explicit FTBToyLocomotionFrames(FAutomationTestBase* InTest) : Test(InTest)
	{
	}
	virtual bool Update() override
	{
		if (!Owner.IsValid())
		{
			UWorld* World = nullptr;
			for (const auto& Context : GEngine->GetWorldContexts())
			{
				if (Context.WorldType == EWorldType::Game && Context.World()->GetNetMode() == NM_Standalone)
				{
					World = Context.World();
				}
			}
			if (!Test->TestNotNull(TEXT("Standalone world"), World))
			{
				return true;
			}
			auto* Class =
			    LoadClass<ATBToyCharacter>(nullptr, TEXT("/Game/Characters/Toys/BP_ToyCharacter.BP_ToyCharacter_C"));
			if (!Test->TestNotNull(TEXT("Toy catalogue"), Class))
			{
				return true;
			}
			Owner = World->SpawnActor<ACharacter>();
			// テスト用の速度が移動処理で上書きされるのを防ぐ。アニメーションは通常更新する。
			Owner->GetCharacterMovement()->SetComponentTickEnabled(false);
			FActorSpawnParameters Spawn;
			Spawn.Owner = Owner.Get();
			for (auto VisualClass : Class->GetDefaultObject<ATBToyCharacter>()->ToyCatalogue)
			{
				auto* Actor = World->SpawnActor<AActor>(VisualClass, Spawn);
				if (!Test->TestNotNull(TEXT("Visual actor"), Actor))
				{
					continue;
				}
				Actors.Add(Actor);
				TInlineComponentArray<USkeletalMeshComponent*> Parts(Actor);
				for (auto* Part : Parts)
				{
					if (!Part->LeaderPoseComponent.IsValid())
					{
						Meshes.Add(Part);
					}
				}
			}
			Since = FPlatformTime::Seconds();
			return false;
		}
		if (FPlatformTime::Seconds() - Since < .4)
		{
			return false;
		}
		// 待機→歩行→走行→停止で、速度がBlendSpaceへ届くことを確認する。
		const float Speeds[] = {0.f, 30.f, 120.f, 0.f};
		for (auto WeakMesh : Meshes)
		{
			auto* Part = WeakMesh.Get();
			if (!Part || !Test->TestNotNull(TEXT("Animation instance"), Part->GetAnimInstance()))
			{
				continue;
			}
			bool bCorrectInput = false;
			for (const FAnimTickRecord& Record : Part->GetAnimInstance()->GetUngroupedActivePlayersRead())
			{
				if (Cast<UBlendSpace>(Record.SourceAsset))
				{
					bCorrectInput |= FMath::IsNearlyEqual(Record.BlendSpace.BlendSpacePositionX, Speeds[Phase], .01f);
				}
			}
			Test->TestTrue(FString::Printf(TEXT("%s: automatic blend input at speed %.0f"),
			                               *Part->GetOwner()->GetName(), Speeds[Phase]),
			               bCorrectInput);
			if (Phase == 0)
			{
				IdlePoses.Add(WeakMesh, Part->GetComponentSpaceTransforms());
			}
			if (Phase == 2)
			{
				const auto* Idle = IdlePoses.Find(WeakMesh);
				if (!Idle)
				{
					Test->AddError(TEXT("Missing idle pose for running comparison"));
					continue;
				}
				const auto& Run = Part->GetComponentSpaceTransforms();
				bool bChanged = false;
				for (int32 Bone = 0; Bone < FMath::Min(Idle->Num(), Run.Num()); ++Bone)
				{
					bChanged |= !(*Idle)[Bone].Equals(Run[Bone], .001f);
				}
				Test->TestTrue(TEXT("Running pose advances under normal world ticking"), bChanged);
			}
		}
		if (++Phase == 4)
		{
			for (auto Actor : Actors)
			{
				if (Actor.IsValid())
				{
					Actor->Destroy();
				}
			}
			Owner->Destroy();
			return true;
		}
		Owner->GetCharacterMovement()->Velocity = FVector(Speeds[Phase], 0, 0);
		Since = FPlatformTime::Seconds();
		return false;
	}

private:
	FAutomationTestBase* Test;
	TWeakObjectPtr<ACharacter> Owner;
	TArray<TWeakObjectPtr<AActor>> Actors;
	TArray<TWeakObjectPtr<USkeletalMeshComponent>> Meshes;
	TMap<TWeakObjectPtr<USkeletalMeshComponent>, TArray<FTransform>> IdlePoses;
	int32 Phase = 0;
	double Since = 0;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBToyLocomotionTest, "ToonStory.Appearance.Locomotion",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBToyLocomotionTest::RunTest(const FString&)
{
	ADD_LATENT_AUTOMATION_COMMAND(FTBToyLocomotionFrames(this));
	return true;
}
#endif
