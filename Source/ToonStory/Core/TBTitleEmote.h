// Copyright (c) 2026 Mamodou. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Character/TBCharacter.h"
#include "TBTitleEmote.generated.h"

class ALevelSequenceActor;

/**
 * タイトルのおもちゃに、ゲームと同じエモートを出させる。持ち主のキャラクターがいないので、ここが代わりに状態を持つ。
 * 踊る・固まる・やめるは、ループのシーケンスのマーカー（Emote / EmoteHold / EmoteOff）で決まる。
 */
UCLASS()
class TOONSTORY_API UTBTitleEmote : public UActorComponent
{
	GENERATED_BODY()
public:
	UTBTitleEmote();
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	// クリックで次のエモートへ。固まっている間は変えない。
	bool Next();
	bool GetSample(FTBEmoteSample& Out) const;
	ETBEmoteKind GetKind() const;
	bool IsHolding() const { return Mode == EMode::Hold; }
	// 骨ではなく体ごと動かすおもちゃ（アヒル・テディ・ロボット）か。
	bool UsesBodyMoves() const;
	// 体ごと動かす間に、腕だけ小さく振る姿勢。アニメの後処理から読む。
	void BuildArmSwing(const USkeletalMeshComponent* Mesh, struct FTBEmotePose& Out) const;

private:
	enum class EMode : uint8 { Off, Play, Hold };
	EMode ReadMode();
	void MoveBody();
	TWeakObjectPtr<USceneComponent> Body;
	FTransform BodyRest;
	TWeakObjectPtr<ALevelSequenceActor> Loop;
	EMode Mode = EMode::Off;
	int32 Index = 0;
	int32 PendingSteps = 0;
	float Phase = 0.f;
	float Weight = 0.f;
	// 切り替えるときに一度ほどいてから次へ移る。
	float Blend = 1.f;
};
