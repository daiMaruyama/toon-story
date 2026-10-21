#include "TBEventDirector.h"
#include "TBRandomEvent.h"
#include "Core/TBGameHelpers.h"
#include "Data/TBRuleData.h"
#include "GameFramework/Actor.h"

bool UTBEventDirector::CanRun() const
{
	return GetOwner() && GetOwner()->HasAuthority() && TB::Playing(GetWorld()) &&
	       TB::Rules().Events.bEnabled && !bExecuting;
}

void UTBEventDirector::ResetRound()
{
	Events.Reset();
	Elapsed = CooldownUntil = 0;
	NextRollAt = 1;
	bPrepared = false;
}

void UTBEventDirector::PrepareRound()
{
	if (bPrepared) return;
	bPrepared = true;
	TSet<UClass*> Added;
	for (const auto& Class : TB::Rules().Events.EventClasses)
	{
		if (!Class || Class->HasAnyClassFlags(CLASS_Abstract) || Added.Contains(Class.Get())) continue;
		Added.Add(Class.Get());
		Events.Add(NewObject<UTBRandomEvent>(this, Class));
	}
}

bool UTBEventDirector::ExecuteEvent(UTBRandomEvent* Event)
{
	if (!CanRun() || Elapsed < CooldownUntil || !IsValid(Event)) return false;
	// Eligibility and execution callbacks cannot recursively trigger other events.
	TGuardValue<bool> Guard(bExecuting, true);
	if (!Event->CanExecute(GetWorld()) || !Event->Execute(GetWorld())) return false;
	CooldownUntil = Elapsed + FMath::Max(0.f, TB::Rules().Events.CooldownSeconds);
	UE_LOG(LogTemp, Log, TEXT("[ToonStoryEvents] Executed %s"), *Event->GetClass()->GetName());
	return true;
}

bool UTBEventDirector::TryTrigger(TSubclassOf<UTBRandomEvent> EventClass)
{
	if (!CanRun()) return false;
	PrepareRound();
	for (UTBRandomEvent* Event : Events)
		if (Event->GetClass() == EventClass) return ExecuteEvent(Event);
	return false;
}

void UTBEventDirector::Advance(float DeltaSeconds)
{
	if (bExecuting) return;
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;
	if (!TB::Playing(GetWorld()))
	{
		ResetRound();
		return;
	}
	if (!TB::Rules().Events.bEnabled) return;
	PrepareRound();
	if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0) return;
	Elapsed += DeltaSeconds;
	if (Elapsed < NextRollAt) return;
	// At most one attempt per frame; never burst after a hitch.
	NextRollAt += FMath::FloorToDouble(Elapsed - NextRollAt) + 1;
	if (Elapsed < CooldownUntil ||
	    FMath::FRand() >= FMath::Clamp(TB::Rules().Events.ChancePerSecond, 0.f, 1.f)) return;
	TArray<UTBRandomEvent*> Eligible;
	{
		TGuardValue<bool> Guard(bExecuting, true);
		for (UTBRandomEvent* Event : Events)
			if (Event->CanExecute(GetWorld())) Eligible.Add(Event);
	}
	if (!Eligible.IsEmpty()) ExecuteEvent(Eligible[FMath::RandHelper(Eligible.Num())]);
}

