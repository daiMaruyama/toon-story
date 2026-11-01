// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Components/StaticMeshComponent.h"
#include "Core/TBTitleGaze.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Misc/ScopeExit.h"

namespace
{
	UWorld* GazeTestWorld()
	{
		for (const auto& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::Game && Context.World()) return Context.World();
		}
		return nullptr;
	}
}

// 光の当たった手前の1体だけ固まり、外れてしばらくすると戻る。すぐ横や奥のおもちゃは固まらない。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBTitleGazeTest, "ToonStory.Title.Gaze",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBTitleGazeTest::RunTest(const FString& Parameters)
{
	UWorld* World = GazeTestWorld();
	if (!TestNotNull(TEXT("Game world"), World)) return false;
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	auto Spawn = [&](const FVector& At)
	{
		auto* Toy = World->SpawnActor<AStaticMeshActor>(At, FRotator::ZeroRotator);
		Toy->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
		Toy->GetStaticMeshComponent()->SetStaticMesh(Cube);
		Toy->SetActorScale3D(FVector(.2f));
		Toy->Tags.Add(TEXT("TitleToy"));
		return Toy;
	};
	// 20cm の箱。光を当てる箱のすぐ横と真後ろにも置く。
	AStaticMeshActor* Lit = Spawn(FVector(500, 0, 50));
	AStaticMeshActor* Beside = Spawn(FVector(500, 15, 50));
	AStaticMeshActor* Behind = Spawn(FVector(800, 0, 50));
	AStaticMeshActor* Other = Spawn(FVector(500, 300, 50));
	auto* Gaze = World->SpawnActor<ATBTitleGaze>();
	ON_SCOPE_EXIT
	{
		Gaze->Destroy();
		Lit->Destroy();
		Beside->Destroy();
		Behind->Destroy();
		Other->Destroy();
	};

	Gaze->SetGazeRay(FVector(0, 0, 50), FVector(1, 0, 0));
	Gaze->UpdateGaze(.016f);
	TestTrue(TEXT("The lit toy freezes"), Gaze->IsFrozen(Lit));
	TestTrue(TEXT("The real toy hides behind its frozen copy"), Lit->IsHidden());
	TestFalse(TEXT("A toy out of the light keeps dancing"), Gaze->IsFrozen(Other));
	TestFalse(TEXT("A toy just beside the cursor keeps dancing"), Gaze->IsFrozen(Beside));
	TestFalse(TEXT("A toy hidden behind the lit one keeps dancing"), Gaze->IsFrozen(Behind));
	Gaze->UpdateGaze(1.f);
	TestTrue(TEXT("It stays frozen while lit"), Gaze->IsFrozen(Lit));

	Gaze->ClearGaze();
	Gaze->UpdateGaze(.1f);
	TestTrue(TEXT("It stays frozen for a moment after the light leaves"), Gaze->IsFrozen(Lit));
	Gaze->UpdateGaze(.5f);
	TestFalse(TEXT("Then it dances again"), Gaze->IsFrozen(Lit));
	TestFalse(TEXT("The real toy is shown again"), Lit->IsHidden());
	TestEqual(TEXT("Nothing is left frozen"), Gaze->NumFrozen(), 0);
	return true;
}
#endif
