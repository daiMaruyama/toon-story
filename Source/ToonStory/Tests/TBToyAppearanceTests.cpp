#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Character/TBCharacter.h"
#include "Character/TBToyCharacter.h"
#include "Character/TBToyAnimationComponent.h"
#include "Character/TBCharacterAppearanceComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Core/TBController.h"
#include "Core/TBGameMode.h"
#include "Core/TBGameState.h"
#include "Core/TBPlayerState.h"
#include "Data/TBToyDefinition.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/BlendSpace.h"
#include "GameFramework/CharacterMovementComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBToyAppearanceTest, "ToonStory.Appearance.ToyDefinition",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTBToyAppearanceTest::RunTest(const FString& Parameters)
{
	UWorld* World = nullptr;
	for (const auto& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::Game && Context.World()->GetNetMode() == NM_Standalone)
		{
			World = Context.World();
			break;
		}
	}
	if (!TestNotNull(TEXT("Standalone game world"), World)) return false;
	auto* Host = Cast<ATBController>(World->GetFirstPlayerController());
	auto* Mode = World->GetAuthGameMode<ATBGameMode>();
	auto* State = World->GetGameState<ATBGameState>();
	if (!Host || !Mode || !State) return false;
	auto* Second = Cast<ATBController>(UGameplayStatics::CreatePlayer(World, -1, true));
	if (!TestNotNull(TEXT("Second local player"), Second)) return false;
	auto* HumanInfo = Host->GetPlayerState<ATBPlayerState>();
	auto* ToyInfo = Second->GetPlayerState<ATBPlayerState>();
	FTBSettings Rules;
	Rules.Humans = 1;
	Rules.Toys = 1;
	Rules.Duration = 600;
	Rules.RequiredItems = 5;
	Mode->SetRules(Host, Rules);
	HumanInfo->Preference = ETBTeam::Human;
	ToyInfo->Preference = ETBTeam::Toy;
	HumanInfo->bReady = ToyInfo->bReady = true;
	Mode->StartRound(Host);
	TestEqual(TEXT("Round started"), State->Phase, ETBPhase::Playing);
	auto* Toy = Cast<ATBToyCharacter>(Second->GetPawn());
	auto* Human = Cast<ATBCharacter>(Host->GetPawn());
	if (!Toy || !Human) { UGameplayStatics::RemovePlayer(Second, true); return false; }
	Toy->Tick(0.f); // StartRound applies team state; presentation reconciles on the next actor tick.
	auto* Definition = Toy->ToyDefinition.Get();
	if (!TestNotNull(TEXT("Configured definition loaded"), Definition))
	{
		UGameplayStatics::RemovePlayer(Second, true);
		return false;
	}
	FString ValidationMessage;
	TestTrue(TEXT("Definition validates"), Definition->ValidateDefinition(ValidationMessage));
	TestNull(TEXT("Base toy animation is disabled to avoid double playback"), Toy->ToySkeletalMesh.Get());
	const auto CheckMaterials = [this, Toy, Definition]()
	{
		TestEqual(TEXT("Definition mesh selected"), Toy->GetMesh()->GetSkeletalMeshAsset(), Definition->SkeletalMesh.Get());
		for (int32 Index = 0; Index < Toy->GetMesh()->GetNumMaterials(); ++Index)
		{
			auto* Material = Toy->GetMesh()->GetMaterial(Index);
			if (!TestNotNull(TEXT("Material exists"), Material)) continue;
			auto* Expected = Definition->MaterialOverrides.IsValidIndex(Index) && Definition->MaterialOverrides[Index]
				? Definition->MaterialOverrides[Index].Get() : Definition->SkeletalMesh->GetMaterials()[Index].MaterialInterface.Get();
			TestEqual(TEXT("Every material slot matches the selected toy"), Material, Expected);
			TestTrue(TEXT("Saved material supports skeletal rendering"),
			         Material->GetMaterial()->GetUsageByFlag(MATUSAGE_SkeletalMesh));
		}
	};
	CheckMaterials();
	// StartRound spawns above the floor; this synchronous check exercises grounded locomotion.
	Toy->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	{
		auto* Appearance = Toy->FindComponentByClass<UTBCharacterAppearanceComponent>();
		auto* Movement = Toy->GetCharacterMovement();
		const auto PreviousFloor = Movement->CurrentFloor;
		FHitResult Floor;
		Floor.bBlockingHit = true;
		Floor.ImpactNormal = FVector::UpVector;
		Movement->CurrentFloor.SetFromSweep(Floor, 2.2f, true);
		Appearance->UpdateGrounding(*Toy);
		const auto Bounds = Definition->SkeletalMesh->GetBounds();
		const double MeshBottom = Toy->GetMesh()->GetRelativeLocation().Z +
			(Bounds.Origin.Z - Bounds.BoxExtent.Z) * Toy->GetMesh()->GetRelativeScale3D().Z;
		TestTrue(TEXT("Toy bottom compensates capsule floor clearance"),
			FMath::IsNearlyEqual(MeshBottom, -double(Toy->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()) - 2.2, .001));
		const FVector BeforeSmoothing = Toy->GetMesh()->GetRelativeLocation();
		Toy->GetMesh()->AddLocalOffset(FVector(0, 0, .5));
		Appearance->UpdateGrounding(*Toy);
		TestTrue(TEXT("Grounding preserves network smoothing when clearance is unchanged"),
			Toy->GetMesh()->GetRelativeLocation().Equals(BeforeSmoothing + FVector(0, 0, .5), .001));
		Toy->GetMesh()->SetRelativeLocation(BeforeSmoothing);
		Movement->CurrentFloor = PreviousFloor;
		Appearance->UpdateGrounding(*Toy);
	}
	if (Definition->Locomotion)
	{
		auto* Player = Toy->GetMesh()->GetSingleNodeInstance();
		if (TestNotNull(TEXT("Blend space player exists"), Player))
		{
			TestEqual(TEXT("Configured locomotion selected"), Player->GetAnimationAsset(),
			          static_cast<UAnimationAsset*>(Definition->Locomotion.Get()));
			for (const float Speed : {0.f, Toy->ToySpeed, Toy->ToySprintSpeed})
			{
				TArray<FBlendSampleData> Samples;
				int32 Segment = INDEX_NONE;
				TestTrue(TEXT("Saved blend space evaluates samples"),
				         Definition->Locomotion->GetSamplesFromBlendInput(FVector(Speed, 0, 0), Samples, Segment, true));
				TestTrue(TEXT("Locomotion has a weighted animation"), Samples.Num() > 0);
				Toy->GetCharacterMovement()->Velocity = FVector(Speed, 0, 0);
				Toy->Tick(1.f / 30.f);
				FVector Input, Filtered;
				Player->GetBlendSpaceState(Input, Filtered);
				TestEqual(TEXT("Speed reaches blend space"), Input.X, double(Speed));
				TestFalse(TEXT("Free toy animates"), Toy->GetMesh()->bPauseAnims);
			}
			Toy->GetCharacterMovement()->Velocity = FVector::ZeroVector;
		}
	}
	// Simulate a previous character's component override, then change roles twice.
	Toy->GetMesh()->SetMaterial(0, UMaterial::GetDefaultMaterial(MD_Surface));
	ToyInfo->Team = ETBTeam::Human;
	Toy->ApplyState();
	Toy->RefreshToyPresentation();
	TestFalse(TEXT("Child animation is not left paused"), Toy->GetMesh()->bPauseAnims);
	ToyInfo->Team = ETBTeam::Toy;
	Toy->ApplyState();
	Toy->RefreshToyPresentation();
	CheckMaterials();
	TestTrue(TEXT("Old override removed"), Toy->GetMesh()->GetMaterial(0) != UMaterial::GetDefaultMaterial(MD_Surface));
	auto* BeforeCarry = Toy->GetMesh()->GetMaterial(0);
	ToyInfo->ToyState = ETBToyState::Carried;
	Toy->Carrier = Human;
	Toy->OnRep_Carrier();
	Toy->Tick(0.f); // Camera state is reconciled by Tick after replication.
	if (Definition->Locomotion) TestTrue(TEXT("Carried toy stops animating"), Toy->GetMesh()->bPauseAnims);
	TestTrue(TEXT("Carry view enabled"), Toy->CarryCamera->IsActive());
	TestEqual(TEXT("Carry view keeps material"), Toy->GetMesh()->GetMaterial(0), BeforeCarry);
	Toy->Carrier = nullptr;
	ToyInfo->ToyState = ETBToyState::Free;
	Toy->OnRep_Carrier();
	Toy->Tick(0.f);
	if (Definition->Locomotion) TestFalse(TEXT("Released toy resumes animating"), Toy->GetMesh()->bPauseAnims);
	TestTrue(TEXT("First person restored"), Toy->Camera->IsActive());
	TestFalse(TEXT("Carry view disabled"), Toy->CarryCamera->IsActive());
	TestTrue(TEXT("Own body hidden in first person"), Toy->GetMesh()->bOwnerNoSee);
	TestEqual(TEXT("First person keeps material"), Toy->GetMesh()->GetMaterial(0), BeforeCarry);
	TestEqual(TEXT("Seven playable toy definitions"), Toy->ToyCatalogue.Num(), 7);
	for (int32 Index = 0; Index < Toy->ToyCatalogue.Num(); ++Index)
	{
		Toy->ServerSelectToy(Index);
		TestEqual(TEXT("Selection accepted"), Toy->SelectedToyIndex, Index);
		auto* Selected = Toy->ToyCatalogue[Index].Get();
		if (!TestNotNull(TEXT("Catalogue definition loaded"), Selected)) continue;
		TestTrue(TEXT("Catalogue definition validates"), Selected->ValidateDefinition(ValidationMessage));
		TestEqual(TEXT("Catalogue mesh selected"), Toy->GetMesh()->GetSkeletalMeshAsset(), Selected->SkeletalMesh.Get());
		for (int32 Slot = 0; Slot < Toy->GetMesh()->GetNumMaterials(); ++Slot)
		{
			auto* Material = Toy->GetMesh()->GetMaterial(Slot);
			TestTrue(TEXT("Catalogue material supports skinning"), Material && Material->GetMaterial()->GetUsageByFlag(MATUSAGE_SkeletalMesh));
		}
		for (float Speed : {0.f, Toy->ToySpeed, Toy->ToySprintSpeed})
		{
			TArray<FBlendSampleData> Samples; int32 Segment = INDEX_NONE;
			TestTrue(TEXT("Catalogue locomotion evaluates"), Selected->Locomotion && Selected->Locomotion->GetSamplesFromBlendInput(FVector(Speed,0,0), Samples, Segment, true) && Samples.Num() > 0);
		}
	}
	const int32 LastIndex = Toy->SelectedToyIndex;
	Toy->ServerSelectToy(999);
	TestEqual(TEXT("Invalid selection rejected"), Toy->SelectedToyIndex, LastIndex);
	ToyInfo->ToyState = ETBToyState::Carried;
	Toy->ServerSelectToy(0);
	TestEqual(TEXT("Carried selection rejected"), Toy->SelectedToyIndex, LastIndex);
	ToyInfo->ToyState = ETBToyState::Free;
	Toy->NextToy();
	TestEqual(TEXT("Catalogue wraps to first toy"), Toy->SelectedToyIndex, 0);
	// Exercise the reusable player without a TBCharacter tick or gameplay state change.
	// This also covers legacy sequence playback, which the current catalogue's blend spaces bypass.
	auto* Clips = NewObject<UTBToyDefinition>(Toy);
	Clips->Idle = Definition->Idle;
	Clips->Walk = Definition->Walk;
	Clips->Run = Definition->Run;
	Clips->Jump = Definition->Walk;
	Clips->WalkReferenceSpeed = Toy->ToySpeed;
	Clips->RunReferenceSpeed = Toy->ToySprintSpeed;
	Toy->ToyAnimation->Configure(Toy->GetMesh(), Clips, Toy->ToySpeed, Toy->ToySprintSpeed);
	const auto CheckClip = [this, Toy](UAnimSequence* Expected)
	{
		auto* Player = Toy->GetMesh()->GetSingleNodeInstance();
		if (TestNotNull(TEXT("Independent animation player"), Player))
			TestEqual(TEXT("Expected sequence playing"), Player->GetAnimationAsset(), static_cast<UAnimationAsset*>(Expected));
	};
	Toy->ToyAnimation->Update(0.f, false, false);
	CheckClip(Clips->Idle);
	Toy->ToyAnimation->Update(Toy->ToySpeed, false, false);
	CheckClip(Clips->Walk);
	Toy->ToyAnimation->Update(Toy->ToySprintSpeed, false, false);
	CheckClip(Clips->Run);
	Toy->ToyAnimation->Update(0.f, false, true);
	CheckClip(Clips->Run);
	TestTrue(TEXT("Frozen sequence retains its pose"), Toy->GetMesh()->bPauseAnims);
	Toy->ToyAnimation->Update(0.f, true, false);
	CheckClip(Clips->Jump);
	TestFalse(TEXT("Sequence resumes after freeze"), Toy->GetMesh()->bPauseAnims);
	Toy->ToyAnimation->Update(0.f, false, false);
	CheckClip(Clips->Idle);
	Toy->ToyAnimation->Configure(Toy->GetMesh(), NewObject<UTBToyDefinition>(Toy), 30.f, 120.f);
	Toy->ToyAnimation->Update(0.f, false, false);
	CheckClip(nullptr);
	TestEqual(TEXT("New toy resets playback rate"), Toy->GetMesh()->GlobalAnimRateScale, 1.f);
	UGameplayStatics::RemovePlayer(Second, true);
	return true;
}
#endif
