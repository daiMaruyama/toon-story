#include "TBToyAnimationComponent.h"
#include "Data/TBToyDefinition.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/BlendSpace.h"
#include "Components/SkeletalMeshComponent.h"

void UTBToyAnimationComponent::Reset()
{
	if (Mesh)
	{
		Mesh->bPauseAnims = false;
		Mesh->GlobalAnimRateScale = 1.f;
		if (auto* Player = Mesh->GetSingleNodeInstance()) Player->SetPlaying(false);
	}
	Mesh = nullptr;
	Definition = nullptr;
	ActiveAnimation = nullptr;
}

void UTBToyAnimationComponent::Configure(USkeletalMeshComponent* InMesh, UTBToyDefinition* InDefinition,
                                        float InWalkSpeed, float InRunSpeed)
{
	Reset();
	Mesh = InMesh;
	Definition = InDefinition;
	WalkSpeed = InWalkSpeed;
	RunSpeed = InRunSpeed;
	if (!Mesh || !Definition) return;
	Mesh->bPauseAnims = false;
	Mesh->GlobalAnimRateScale = 1.f;
	// Clear the previous toy/child animation instance even when the new toy has no clips.
	Mesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	Mesh->SetAnimation(nullptr);
	if (Definition->AnimationBlueprint)
	{
		Mesh->SetAnimInstanceClass(Definition->AnimationBlueprint);
	}
}

void UTBToyAnimationComponent::Update(float Speed, bool bFalling, bool bLocked)
{
	if (!Mesh || !Definition) return;
	if (Definition->AnimationBlueprint)
	{
		Mesh->bPauseAnims = bLocked;
		return;
	}
	if (Definition->Locomotion)
	{
		UAnimationAsset* Asset = bFalling && Definition->Jump
			? static_cast<UAnimationAsset*>(Definition->Jump.Get()) : Definition->Locomotion.Get();
		auto* Player = Mesh->GetSingleNodeInstance();
		if (!Player || Player->GetAnimationAsset() != Asset)
		{
			Mesh->PlayAnimation(Asset, Asset == Definition->Locomotion);
			Player = Mesh->GetSingleNodeInstance();
		}
		if (Player) Player->SetBlendSpacePosition(FVector(bFalling ? 0.f : Speed, 0, 0));
		Mesh->GlobalAnimRateScale = 1.f;
		Mesh->bPauseAnims = bLocked;
		return;
	}
	// Keep the current pose while carried/frozen; do not switch to idle on zero velocity.
	Mesh->bPauseAnims = bLocked;
	if (bLocked && ActiveAnimation) return;
	const float RunThreshold = (WalkSpeed + RunSpeed) * .5f +
		(ActiveAnimation == Definition->Run ? -10.f : 10.f);
	const bool bRunning = Definition->Run && Speed > RunThreshold;
	UAnimSequence* Animation = Speed > 5.f
		? (bRunning ? Definition->Run.Get() : Definition->Walk.Get()) : Definition->Idle.Get();
	if (bFalling) Animation = Definition->Jump ? Definition->Jump.Get() : Definition->Idle.Get();
	if (Animation && Animation != ActiveAnimation)
	{
		Mesh->PlayAnimation(Animation, Animation != Definition->Jump);
		ActiveAnimation = Animation;
	}
	const bool bPlayingRun = Definition->Run && ActiveAnimation == Definition->Run;
	const bool bLocomotion = bPlayingRun || (Definition->Walk && ActiveAnimation == Definition->Walk);
	const float ReferenceSpeed = bPlayingRun ? Definition->RunReferenceSpeed : Definition->WalkReferenceSpeed;
	Mesh->GlobalAnimRateScale = bLocomotion ? FMath::Clamp(Speed / FMath::Max(ReferenceSpeed, 1.f), .7f, 1.8f) : 1.f;
}
