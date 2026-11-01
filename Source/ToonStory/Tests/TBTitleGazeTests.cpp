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

	// 戻るときは、固まった姿から少しかけて溶け込む。途中でまた照らせば、その姿で固まり直す。
	Gaze->SetGazeRay(FVector(0, 0, 50), FVector(1, 0, 0));
	Gaze->UpdateGaze(.016f);
	Gaze->ClearGaze();
	Gaze->UpdateGaze(.39f);
	Gaze->UpdateGaze(.05f);
	TestFalse(TEXT("Blending back is no longer frozen"), Gaze->IsFrozen(Lit));
	TestTrue(TEXT("The copy stays while blending back"), Lit->IsHidden());
	Gaze->SetGazeRay(FVector(0, 0, 50), FVector(1, 0, 0));
	Gaze->UpdateGaze(.016f);
	TestTrue(TEXT("Lighting it again mid-blend freezes it again"), Gaze->IsFrozen(Lit));
	TestEqual(TEXT("No second copy"), Gaze->NumFrozen(), 1);
	Gaze->ClearGaze();
	Gaze->UpdateGaze(.45f);
	Gaze->UpdateGaze(.3f);
	TestFalse(TEXT("The real toy is back after the blend"), Lit->IsHidden());
	TestEqual(TEXT("The copy is gone"), Gaze->NumFrozen(), 0);

	// 寸劇に切り替わったら（メニューが出ていなければ）、待たずにすぐ戻す。
	Gaze->SetGazeRay(FVector(0, 0, 50), FVector(1, 0, 0));
	Gaze->UpdateGaze(.016f);
	TestTrue(TEXT("Lit again"), Gaze->IsFrozen(Lit));
	Gaze->Tick(.016f);
	TestEqual(TEXT("Leaving the menu releases at once"), Gaze->NumFrozen(), 0);
	TestFalse(TEXT("The real toy is back for the story"), Lit->IsHidden());

	// 壁の端: おもちゃの中心は見えていても、カーソルが壁を指していれば照らさない。
	auto* Edge = World->SpawnActor<AStaticMeshActor>(FVector(300, 15, 50), FRotator::ZeroRotator);
	Edge->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
	Edge->GetStaticMeshComponent()->SetStaticMesh(Cube);
	Edge->SetActorScale3D(FVector(.2f));
	Gaze->SetGazeRay(FVector(0, 8, 50), FVector(1, 0, 0));
	Gaze->UpdateGaze(.016f);
	TestEqual(TEXT("Pointing at the wall edge lights nothing"), Gaze->NumFrozen(), 0);
	Edge->Destroy();
	Gaze->ClearGaze();

	// 手前に壁があれば、奥のおもちゃは照らさない。見えない板は素通しにする。
	auto* Board = World->SpawnActor<AStaticMeshActor>(FVector(200, 0, 50), FRotator::ZeroRotator);
	Board->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
	Board->GetStaticMeshComponent()->SetStaticMesh(Cube);
	Board->SetActorHiddenInGame(true);
	Gaze->SetGazeRay(FVector(0, 0, 50), FVector(1, 0, 0));
	Gaze->UpdateGaze(.016f);
	TestTrue(TEXT("An invisible board lets the light through"), Gaze->IsFrozen(Lit));
	Gaze->ClearGaze();
	Gaze->UpdateGaze(1.f);
	auto* Wall = World->SpawnActor<AStaticMeshActor>(FVector(300, 0, 50), FRotator::ZeroRotator);
	Wall->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
	Wall->GetStaticMeshComponent()->SetStaticMesh(Cube);
	ON_SCOPE_EXIT
	{
		Board->Destroy();
		Wall->Destroy();
	};
	Gaze->SetGazeRay(FVector(0, 0, 50), FVector(1, 0, 0));
	Gaze->UpdateGaze(.016f);
	TestFalse(TEXT("A toy behind a wall is not lit"), Gaze->IsFrozen(Lit));
	return true;
}
#endif
