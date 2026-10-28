#include "TBMirror.h"
#include "Data/TBMirrorData.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "SceneView.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ATBMirror::ATBMirror()
{
	DisplayName = NSLOCTEXT("TBItems", "Mirror", "Mirror");
	Actions.Reset();
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	MirrorData = TSoftObjectPtr<UTBMirrorData>(FSoftObjectPath(TEXT("/Game/Data/DA_Mirror.DA_Mirror")));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
	Mesh->SetStaticMesh(Cube.Object);
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));
	Surface = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MirrorSurface"));
	Surface->SetupAttachment(Root);
	Surface->SetStaticMesh(Plane.Object);
	Surface->SetRelativeRotation(FRotator(-90, 0, 0));
	Surface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Surface->SetCastShadow(false);
	Surface->SetVisibleInRayTracing(false);
	for (int32 Index=0; Index<4; ++Index)
	{
		auto* Bar = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Frame%d"),Index));
		Bar->SetupAttachment(Root); Bar->SetStaticMesh(Cube.Object);
		Bar->SetCollisionProfileName(TEXT("BlockAll")); Frame.Add(Bar);
	}
	ReflectionCamera = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("ReflectionCamera"));
	ReflectionCamera->SetupAttachment(Root);
	ReflectionCamera->bCaptureEveryFrame = false;
	ReflectionCamera->bCaptureOnMovement = false;
	ReflectionCamera->bAlwaysPersistRenderingState = true;
	ReflectionCamera->bEnableClipPlane = true;
	ReflectionCamera->bUseRayTracingIfEnabled = false;
	ReflectionCamera->CaptureSource = ESceneCaptureSource::SCS_SceneColorHDR;
	ReflectionCamera->ShowFlags.SetScreenSpaceReflections(false);
	ReflectionCamera->ShowFlags.SetMotionBlur(false);
}

const UTBMirrorData& ATBMirror::GetData() const
{
	return LoadedData ? *LoadedData : *GetDefault<UTBMirrorData>();
}

void ATBMirror::ApplyData()
{
	LoadedData = MirrorData.LoadSynchronous();
	const auto& Data = GetData();
	const float W=FMath::Max(10.f,Data.Width), H=FMath::Max(10.f,Data.Height), B=FMath::Max(1.f,Data.FrameWidth);
	Mesh->SetRelativeLocation(FVector(-B*.5f,0,0)); Mesh->SetRelativeScale3D(FVector(B,W,H)/100.f);
	Surface->SetRelativeLocation(FVector(.1f,0,0)); Surface->SetRelativeScale3D(FVector(H/100.f,W/100.f,1));
	for (int32 Index=0; Index<4; ++Index)
	{
		const bool Side=Index<2; const float Sign=Index%2==0 ? -1.f : 1.f;
		Frame[Index]->SetRelativeLocation(Side ? FVector(0,Sign*(W+B)*.5f,0) : FVector(0,0,Sign*(H+B)*.5f));
		Frame[Index]->SetRelativeScale3D((Side ? FVector(B*2,B,H+B*2) : FVector(B*2,W,B))/100.f);
	}
	if (auto* Material=Data.MirrorMaterial.LoadSynchronous()) Surface->SetMaterial(0,Material);
	if (auto* Material=Data.FrameMaterial.LoadSynchronous())
	{
		Mesh->SetMaterial(0,Material);
		for (const auto& Bar:Frame) Bar->SetMaterial(0,Material);
	}
}

void ATBMirror::OnConstruction(const FTransform& Transform) { Super::OnConstruction(Transform); ApplyData(); }
void ATBMirror::PostInitializeComponents() { Super::PostInitializeComponents(); ApplyData(); }

FVector ATBMirror::ReflectPoint(FVector Point, FVector Origin, FVector Normal)
{ return Point-2*FVector::DotProduct(Point-Origin,Normal)*Normal; }
FVector ATBMirror::ReflectDirection(FVector Direction, FVector Normal)
{ return Direction-2*FVector::DotProduct(Direction,Normal)*Normal; }

bool ATBMirror::ReflectionPoint(FVector Eye, FVector Target, FVector& Hit) const
{
	if (IsHidden() || IsActorBeingDestroyed() || GetActorScale3D().GetMin()<=0 || Eye.ContainsNaN() || Target.ContainsNaN()) return false;
	const FVector Origin=GetActorLocation(), Normal=GetActorForwardVector();
	const double A=FVector::DotProduct(Eye-Origin,Normal), B=FVector::DotProduct(Target-Origin,Normal);
	if (A<=.2 || B<=.2 || FVector::DistSquared(Eye,Origin)>FMath::Square(FMath::Max(100.f,GetData().ViewDistance))) return false;
	const FVector VirtualTarget=ReflectPoint(Target,Origin,Normal);
	Hit=Eye+(VirtualTarget-Eye)*(A/(A+B));
	const FVector Local=GetActorTransform().InverseTransformPosition(Hit);
	return FMath::Abs(Local.Y)<FMath::Max(10.f,GetData().Width)*.5f && FMath::Abs(Local.Z)<FMath::Max(10.f,GetData().Height)*.5f;
}

bool ATBMirror::FindView(const AActor* Observer, const AActor* Target, FVector Eye, FRotator View,
    FVector Sample, float TanH, float TanV, FVector& Hit, float MaxPathLength)
{
	if (!IsValid(Observer) || !IsValid(Target) || Observer->GetWorld()!=Target->GetWorld()) return false;
	const FRotationMatrix Basis(View);
	FCollisionQueryParams Query(SCENE_QUERY_STAT(MirrorGaze),false,Observer); Query.AddIgnoredActor(Target);
	for (TActorIterator<ATBMirror> It(Observer->GetWorld()); It; ++It)
	{
		FVector Point;
		if (!It->ReflectionPoint(Eye,Sample,Point)) continue;
		const FVector Delta=Point-Eye;
		const double Depth=FVector::DotProduct(Basis.GetUnitAxis(EAxis::X),Delta);
		if (Depth<=0 || FMath::Abs(FVector::DotProduct(Basis.GetUnitAxis(EAxis::Y),Delta))>Depth*TanH ||
		    FMath::Abs(FVector::DotProduct(Basis.GetUnitAxis(EAxis::Z),Delta))>Depth*TanV ||
		    (MaxPathLength>0 && Delta.Size()+FVector::Dist(Point,Sample)>MaxPathLength)) continue;
		// Stop just in front of the glass; retain the frame, walls, and other mirrors as occluders.
		const FVector Front=Point+It->GetActorForwardVector()*.2f;
		if (!Observer->GetWorld()->LineTraceTestByChannel(Eye,Front,ECC_Visibility,Query) &&
		    !Observer->GetWorld()->LineTraceTestByChannel(Front,Sample,ECC_Visibility,Query))
		{ Hit=Point; return true; }
	}
	return false;
}

void ATBMirror::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (GetNetMode()==NM_DedicatedServer || IsHidden() || GetActorScale3D().GetMin()<=0) return;
	auto* PC=GetWorld()->GetFirstPlayerController();
	if (!PC || !PC->IsLocalController() || !PC->PlayerCameraManager) return;
	const auto& View=PC->PlayerCameraManager->GetCameraCacheView();
	const FVector Eye=View.Location, Normal=GetActorForwardVector(), Origin=GetActorLocation();
	const auto& Data=GetData();
	if (FVector::DotProduct(Eye-Origin,Normal)<=.2 || FVector::DistSquared(Eye,Origin)>FMath::Square(FMath::Max(100.f,Data.ViewDistance)))
	{
		if (SurfaceMaterial) SurfaceMaterial->SetScalarParameterValue(TEXT("ReflectionEnabled"),0);
		return;
	}
	const FRotationMatrix Basis(View.Rotation);
	const auto* Local=PC->GetLocalPlayer();
	FSceneViewProjectionData Projection;
	if (!Local || !Local->ViewportClient || !Local->ViewportClient->Viewport ||
	    !Local->GetProjectionData(Local->ViewportClient->Viewport,Projection)) return;
	const FIntRect Rect=Projection.GetConstrainedViewRect();
	if (Rect.Width()<=0 || Rect.Height()<=0) return;
	// Conservative sphere/frustum culling: a partially visible large mirror still updates.
	const float Radius=Surface->Bounds.SphereRadius;
	const FVector Delta=Origin-Eye;
	const double Depth=FVector::DotProduct(Basis.GetUnitAxis(EAxis::X),Delta);
	const float Aspect=float(Rect.Width())/Rect.Height();
	const float Tan=1.f/Projection.ProjectionMatrix.M[0][0], TanV=1.f/Projection.ProjectionMatrix.M[1][1];
	if (Depth+Radius<=0 || FMath::Abs(FVector::DotProduct(Basis.GetUnitAxis(EAxis::Y),Delta))>FMath::Max(0.,Depth)*Tan+Radius*2 ||
	    FMath::Abs(FVector::DotProduct(Basis.GetUnitAxis(EAxis::Z),Delta))>FMath::Max(0.,Depth)*TanV+Radius*2) return;
	const double Now=GetWorld()->GetTimeSeconds();
	if (Now<NextCapture) return;
	NextCapture=Now+1.f/FMath::Clamp(Data.FramesPerSecond,1.f,60.f);
	const int32 Width=FMath::Clamp(Data.CaptureWidth,128,2048), Height=FMath::Clamp(FMath::RoundToInt(Width/Aspect),64,2048);
	if (!TargetTexture)
	{
		TargetTexture=NewObject<UTextureRenderTarget2D>(this);
		TargetTexture->InitAutoFormat(Width,Height); TargetTexture->UpdateResourceImmediate(true);
		ReflectionCamera->TextureTarget=TargetTexture;
		SurfaceMaterial=Surface->CreateAndSetMaterialInstanceDynamic(0);
		if (SurfaceMaterial) SurfaceMaterial->SetTextureParameterValue(TEXT("ReflectionTexture"),TargetTexture);
	}
	else if (TargetTexture->SizeX!=Width || TargetTexture->SizeY!=Height) TargetTexture->ResizeTarget(Width,Height);
	const FVector Forward=ReflectDirection(Basis.GetUnitAxis(EAxis::X),Normal), Up=ReflectDirection(Basis.GetUnitAxis(EAxis::Z),Normal);
	ReflectionCamera->SetWorldLocationAndRotation(ReflectPoint(Eye,Origin,Normal),FRotationMatrix::MakeFromXZ(Forward,Up).Rotator());
	ReflectionCamera->FOVAngle=View.FOV;
	ReflectionCamera->bUseCustomProjectionMatrix=true;
	ReflectionCamera->CustomProjectionMatrix=Projection.ProjectionMatrix;
	ReflectionCamera->CustomProjectionMatrix.M[2][0]*=-1;
	ReflectionCamera->ClipPlaneBase=Origin+Normal*.2f; ReflectionCamera->ClipPlaneNormal=Normal;
	ReflectionCamera->HiddenComponents.Reset();
	// Single bounce: never draw another live mirror texture into this one.
	for (TActorIterator<ATBMirror> It(GetWorld()); It; ++It) ReflectionCamera->HiddenComponents.Add(It->Surface);
	ReflectionCamera->CaptureScene();
	if (SurfaceMaterial) SurfaceMaterial->SetScalarParameterValue(TEXT("ReflectionEnabled"),1);
}
