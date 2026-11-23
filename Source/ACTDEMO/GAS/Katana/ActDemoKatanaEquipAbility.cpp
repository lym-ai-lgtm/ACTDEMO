#include "GAS/Katana/ActDemoKatanaEquipAbility.h"

#include "AbilitySystemComponent.h"
#include "GAS/ACTDEMOGASCharacter.h"
#include "GAS/Katana/ActDemoKatanaAttributeSet.h"

UActDemoKatanaEquipAbility::UActDemoKatanaEquipAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

bool UActDemoKatanaEquipAbility::CanActivateAbility(
	FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!ActorInfo)
	{
		return false;
	}

	const AActDemoGASCharacter* Character = Cast<AActDemoGASCharacter>(ActorInfo->AvatarActor.Get());
	const UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	return Character && Character->GetCurrentHealth() > 0.f && ASC &&
		ASC->HasAttributeSetForAttribute(UActDemoKatanaAttributeSet::GetWalkSpeedAttribute()) &&
		Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

void UActDemoKatanaEquipAbility::ActivateAbility(
	FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData*)
{
	// 显式交给派生蓝图播放拔收刀蒙太奇，避免能力已经激活但蓝图入口未执行。
	if (!GetClass()->IsFunctionImplementedInScript(
			GET_FUNCTION_NAME_CHECKED(UActDemoKatanaEquipAbility, K2_ActivateAbility)) ||
		!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	K2_ActivateAbility();
}
