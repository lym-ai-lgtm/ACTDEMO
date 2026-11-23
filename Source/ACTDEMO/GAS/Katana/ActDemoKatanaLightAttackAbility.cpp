#include "GAS/Katana/ActDemoKatanaLightAttackAbility.h"

UActDemoKatanaLightAttackAbility::UActDemoKatanaLightAttackAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UActDemoKatanaLightAttackAbility::ActivateAbility(
	FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData*)
{
	// 复用太刀装备能力的显式交接方式，播放和战斗流程由派生蓝图执行。
	if (!GetClass()->IsFunctionImplementedInScript(
			GET_FUNCTION_NAME_CHECKED(UActDemoKatanaLightAttackAbility, K2_ActivateAbility)) ||
		!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	K2_ActivateAbility();
}
