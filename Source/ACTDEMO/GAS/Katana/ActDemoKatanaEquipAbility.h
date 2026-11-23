#pragma once

#include "Abilities/GameplayAbility.h"
#include "ActDemoKatanaEquipAbility.generated.h"

// 太刀装备能力的 GAS 入口。动画、通知和装备状态提交在派生蓝图中处理。
UCLASS(Abstract, Blueprintable)
class ACTDEMO_API UActDemoKatanaEquipAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UActDemoKatanaEquipAbility();

	virtual bool CanActivateAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

protected:
	virtual void ActivateAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
};
