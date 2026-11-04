#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Audio/TBAudioTypes.h"
#include "Audio/TBAudioEvents.h"
#include "Components/StaticMeshComponent.h"
#include "Core/TBController.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "Tests/AutomationCommon.h"
#include "Tests/TBTestMatch.h"
#include "UObject/UnrealType.h"

namespace
{
	struct FPropDropProbe
	{
		TWeakObjectPtr<UStaticMeshComponent> Part;
		TWeakObjectPtr<AStaticMeshActor> Platform;
		FTransform Original;
		FVector Velocity, Spin;
		bool bWasAwake = false;
		bool bHeardLanding = false;
		FDelegateHandle Hook;
		TOptional<TBTest::FPlayingScope> Match;

		~FPropDropProbe()
		{
			if (AActor::ProcessEventDelegate.GetHandle() == Hook) AActor::ProcessEventDelegate.Unbind();
			if (auto* Mesh = Part.Get())
			{
				Mesh->SetWorldTransform(Original, false, nullptr, ETeleportType::TeleportPhysics);
				Mesh->SetPhysicsLinearVelocity(Velocity);
				Mesh->SetPhysicsAngularVelocityInRadians(Spin);
				if (!bWasAwake) Mesh->PutRigidBodyToSleep();
			}
			if (Platform.IsValid()) Platform->Destroy();
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBPropLandingSoundTest, "ToonStory.Playroom.PropLandingSound",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTBPropLandingSoundTest::RunTest(const FString&)
{
	UWorld* World = nullptr;
	for (const auto& Context : GEngine->GetWorldContexts())
		if (Context.WorldType == EWorldType::Game && Context.World() &&
		    Context.World()->GetMapName().EndsWith(TEXT("TB_StylizedHouse"))) World = Context.World();
	if (!TestNotNull(TEXT("Run in standalone TB_StylizedHouse"), World)) return false;
	auto* Controller = Cast<ATBController>(World->GetFirstPlayerController());
	auto* State = World->GetGameState<ATBGameState>();
	if (!TestNotNull(TEXT("Local game controller"), Controller) || !TestNotNull(TEXT("Game state"), State)) return false;
	if (!TestFalse(TEXT("RPC observation hook is available"), AActor::ProcessEventDelegate.IsBound())) return false;
	UStaticMeshComponent* Part = nullptr;
	for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
	{
		auto* Mesh = It->GetStaticMeshComponent();
		if (Mesh->ComponentHasTag(TEXT("TBPushable")) && Mesh->IsSimulatingPhysics() && Mesh->IsGravityEnabled() &&
		    Mesh->Bounds.SphereRadius < 60.) { Part = Mesh; break; }
	}
	if (!TestNotNull(TEXT("Authored dynamic small prop"), Part)) return false;
	auto* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!TestNotNull(TEXT("Solid test platform mesh"), Cube)) return false;
	const FVector PlatformCenter(15000, 0, 4500);
	auto* Platform = World->SpawnActor<AStaticMeshActor>(PlatformCenter, FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("Isolated landing platform"), Platform)) return false;
	auto* Floor = Platform->GetStaticMeshComponent();
	Floor->SetMobility(EComponentMobility::Movable);
	Floor->SetStaticMesh(Cube);
	Floor->SetWorldScale3D(FVector(6, 6, .2));
	Floor->SetCollisionProfileName(TEXT("BlockAll"));
	const FVector Contact(PlatformCenter.X, PlatformCenter.Y, Floor->Bounds.GetBox().Max.Z);

	auto Probe = MakeShared<FPropDropProbe>();
	Probe->Platform = Platform;
	Probe->Part = Part;
	Probe->Original = Part->GetComponentTransform();
	Probe->Velocity = Part->GetPhysicsLinearVelocity();
	Probe->Spin = Part->GetPhysicsAngularVelocityInRadians();
	Probe->bWasAwake = Part->IsAnyRigidBodyAwake();
	Probe->Match.Emplace(*State, State->GetServerWorldTimeSeconds() + 120.);

	UFunction* RPC = Controller->FindFunctionChecked(GET_FUNCTION_NAME_CHECKED(ATBController, ClientPlaySoundEvent));
	const FProperty* EventParam = RPC->FindPropertyByName(TEXT("Event"));
	const FProperty* LocationParam = FindFProperty<FStructProperty>(RPC, TEXT("Location"));
	if (!TestNotNull(TEXT("Sound event parameter"), EventParam) || !TestNotNull(TEXT("Sound location parameter"), LocationParam)) return false;
	// Observe the real Client RPC even under -nosound. Returning false lets normal dispatch continue.
	AActor::ProcessEventDelegate.BindLambda([Controller, RPC, Contact, Expected = *UTBAudioEvents::PropDropSound(160., UTBAudioEvents::IsGlassProp(Part)), EventParam, LocationParam, Raw = &Probe.Get()](AActor* Actor, UFunction* Fn, void* Args)
	{
		if (Actor == Controller && Fn == RPC)
		{
			const auto Event = *EventParam->ContainerPtrToValuePtr<ETBSoundEvent>(Args);
			const auto Location = *LocationParam->ContainerPtrToValuePtr<FVector>(Args);
			if (Event == Expected && FVector::Dist(Location, Contact) < 65.) Raw->bHeardLanding = true;
		}
		return false;
	});
	Probe->Hook = AActor::ProcessEventDelegate.GetHandle();
	const FVector Bottom(Part->Bounds.Origin.X, Part->Bounds.Origin.Y, Part->Bounds.GetBox().Min.Z);
	Part->AddWorldOffset(Contact + FVector(0, 0, 160) - Bottom, false, nullptr, ETeleportType::TeleportPhysics);
	Part->SetPhysicsLinearVelocity(FVector::ZeroVector);
	Part->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
	Part->WakeAllRigidBodies();
	const double Deadline = World->GetTimeSeconds() + 3.;
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, Probe, World, Deadline]()
	{
		Probe->Match->Hold();
		if (!Probe->bHeardLanding && World->GetTimeSeconds() < Deadline) return false;
		TestTrue(TEXT("A 160 cm physical fall emits the heavy landing RPC at the platform"), Probe->bHeardLanding);
		return true;
	}));
	return true;
}
#endif
