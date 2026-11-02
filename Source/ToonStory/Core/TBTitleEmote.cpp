// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "Core/TBTitleEmote.h"
#include "Character/TBToyPresentation.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "LevelSequence.h"
#include "MovieScene.h"
#include "EngineUtils.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Misc/ScopeExit.h"

namespace
{
	// 手ふり → よろこび → ダンス → 隠しダンス、の順に回す。
	constexpr ETBEmoteKind Kinds[] = {ETBEmoteKind::Wave, ETBEmoteKind::Cheer, ETBEmoteKind::Dance, ETBEmoteKind::WildDance};
	constexpr int32 KindCount = UE_ARRAY_COUNT(Kinds);
	constexpr float FadeSpeed = 3.f;
	constexpr float UnwindSpeed = 8.f;
	constexpr float RewindSpeed = 5.f;
	const FName LoopTag(TEXT("TitleLoop"));
	// 体ごと動かすエモートの1回分の秒数（回る・ハネる・ジャンプ・よちよち）。
	constexpr float BodyDurations[] = {1.6f, 1.f, 1.4f, 1.2f};
}

UTBTitleEmote::UTBTitleEmote()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UTBTitleEmote::BeginPlay()
{
	Super::BeginPlay();
	// 最初はおもちゃごとにばらばら。拍もずらして、みんなで揃わないようにする。
	Index = FMath::RandRange(0, KindCount - 1);
	Phase = FMath::FRand();
	// アヒルは骨を回しても見えにくく体が傾いて浮く。テディとロボットは腕と頭が硬く付いていて、大きく回すと割れる。
	// この3体は体ごと動かす4種類にする。
	auto BodyMover = [](const USkeletalMesh* Asset)
	{
		const FString Name = Asset ? Asset->GetName() : FString();
		return Name.Contains(TEXT("Duck")) || Name.Contains(TEXT("TeddyBear")) || Name.Contains(TEXT("TinRobot"));
	};
	for (UActorComponent* Part : GetOwner()->GetComponents())
	{
		auto* Mesh = Cast<USkeletalMeshComponent>(Part);
		if (Mesh && Mesh->IsVisible() && !Mesh->bHiddenInGame && BodyMover(Mesh->GetSkeletalMeshAsset()) && !Mesh->LeaderPoseComponent.IsValid())
		{
			Body = Mesh;
			BodyRest = Mesh->GetRelativeTransform();
			break;
		}
	}
}

UTBTitleEmote::EMode UTBTitleEmote::ReadMode()
{
	if (!Loop.IsValid())
	{
		for (TActorIterator<ALevelSequenceActor> It(GetWorld()); It; ++It)
		{
			if (It->ActorHasTag(LoopTag)) { Loop = *It; break; }
		}
	}
	const ALevelSequenceActor* Actor = Loop.Get();
	const ULevelSequencePlayer* Player = Actor ? Actor->GetSequencePlayer() : nullptr;
	const ULevelSequence* Sequence = Actor ? Actor->GetSequence() : nullptr;
	const UMovieScene* Scene = Sequence ? Sequence->GetMovieScene() : nullptr;
	if (!Player || !Scene || !Player->IsPlaying()) return EMode::Off;
	// 今の時刻より前で、いちばん新しいマーカーに従う。
	const double Now = Player->GetCurrentTime().AsSeconds();
	double Latest = -1.;
	EMode Found = EMode::Off;
	for (const FMovieSceneMarkedFrame& Mark : Scene->GetMarkedFrames())
	{
		const double At = Scene->GetTickResolution().AsSeconds(Mark.FrameNumber);
		if (At > Now || At < Latest) continue;
		if (Mark.Label == TEXT("Emote")) Found = EMode::Play;
		else if (Mark.Label == TEXT("EmoteHold")) Found = EMode::Hold;
		else if (Mark.Label == TEXT("EmoteOff")) Found = EMode::Off;
		else continue;
		Latest = At;
	}
	return Found;
}

void UTBTitleEmote::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const bool bWasOff = Mode == EMode::Off;
	Mode = ReadMode();
	// 寸劇に切り替わったら、残さずすぐ消す。
	if (Mode == EMode::Off && !(Loop.IsValid() && Loop->GetSequencePlayer() && Loop->GetSequencePlayer()->IsPlaying()))
	{
		Weight = 0.f;
		MoveBody();
		return;
	}
	Weight = FMath::FInterpConstantTo(Weight, Mode == EMode::Off ? 0.f : 1.f, DeltaTime, FadeSpeed);
	ON_SCOPE_EXIT { MoveBody(); };
	if (Mode == EMode::Hold) return; // 見られている間は、その姿勢のまま。
	if (PendingSteps > 0)
	{
		Blend = FMath::FInterpConstantTo(Blend, 0.f, DeltaTime, UnwindSpeed);
		if (Blend <= 0.f || bWasOff)
		{
			Index = (Index + PendingSteps) % KindCount;
			PendingSteps = 0;
			Phase = 0.f;
		}
	}
	else
	{
		Blend = FMath::FInterpConstantTo(Blend, 1.f, DeltaTime, RewindSpeed);
	}
	const double Duration = Body.IsValid() ? BodyDurations[Index] : ATBCharacter::EmoteDuration(GetKind());
	if (Duration > 0) Phase = FMath::Frac(Phase + DeltaTime / Duration);
}

void UTBTitleEmote::MoveBody()
{
	USceneComponent* Mesh = Body.Get();
	if (!Mesh) return;
	const float W = Weight * Blend;
	const float Beat = Phase * 2.f * PI;
	FVector Lift = FVector::ZeroVector;
	FRotator Turn = FRotator::ZeroRotator;
	switch (Index)
	{
	case 0: Turn.Yaw = 360.f * FMath::SmoothStep(0.f, 1.f, Phase); break;       // くるっと回る
	case 1: Lift.Z = 5.f * FMath::Abs(FMath::Sin(Beat * 2.f)); break;         // 小さく縦にハネる
	case 2: Lift.Z = Phase < .6f ? 18.f * FMath::Sin(PI * Phase / .6f) : 0.f; // 大きくジャンプ
		break;
	default: Turn.Roll = 14.f * FMath::Sin(Beat * 2.f); Turn.Yaw = 10.f * FMath::Sin(Beat); break; // よちよち揺れる
	}
	Mesh->SetRelativeLocationAndRotation(BodyRest.GetLocation() + Lift * W, (BodyRest.Rotator() + Turn * W).Quaternion());
}

bool UTBTitleEmote::UsesBodyMoves() const
{
	return Body.IsValid();
}

void UTBTitleEmote::BuildArmSwing(const USkeletalMeshComponent* Mesh, FTBEmotePose& Out) const
{
	Out.Bones.Reset();
	const auto* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	const float W = Weight * Blend;
	if (!Asset || W <= 0.f) return;
	// 肩が割れない角度（12度まで）で、左右の腕を交互に小さく振る。腕の無いアヒルは何もしない。
	const FVector Forward = Mesh->GetComponentTransform().InverseTransformVectorNoScale(GetOwner()->GetActorForwardVector()).GetSafeNormal();
	const float Swing = FMath::DegreesToRadians(12.f * W * FMath::Sin(Phase * 4.f * PI));
	for (const TCHAR* Arm : {TEXT("arm_L"), TEXT("arm_R")})
	{
		if (Asset->GetRefSkeleton().FindBoneIndex(Arm) > 0) Out.Bones.Add({Arm, FQuat(Forward, Arm[4] == TEXT('L') ? Swing : -Swing)});
	}
}

bool UTBTitleEmote::Next()
{
	if (Mode == EMode::Hold) return false;
	++PendingSteps;
	return true;
}

ETBEmoteKind UTBTitleEmote::GetKind() const
{
	return Kinds[Index];
}

bool UTBTitleEmote::GetSample(FTBEmoteSample& Out) const
{
	Out = FTBEmoteSample();
	const float W = Weight * Blend;
	if (W <= 0.f || Body.IsValid()) return false;
	Out.Kind = GetKind();
	Out.Progress = Phase;
	Out.Weight = W;
	return true;
}
