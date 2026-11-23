#include "GAS/Katana/ActDemoKatanaAttributeSet.h"

#include "Net/UnrealNetwork.h"

UActDemoKatanaAttributeSet::UActDemoKatanaAttributeSet()
{
	InitWalkSpeed(180.f);
	InitRunSpeed(440.f);
}

void UActDemoKatanaAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(UActDemoKatanaAttributeSet, WalkSpeed, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UActDemoKatanaAttributeSet, RunSpeed, COND_None, REPNOTIFY_Always);
}

void UActDemoKatanaAttributeSet::OnRep_WalkSpeed(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UActDemoKatanaAttributeSet, WalkSpeed, OldValue);
}

void UActDemoKatanaAttributeSet::OnRep_RunSpeed(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UActDemoKatanaAttributeSet, RunSpeed, OldValue);
}
