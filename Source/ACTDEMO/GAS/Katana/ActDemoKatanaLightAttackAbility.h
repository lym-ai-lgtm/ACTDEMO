#pragma once

#include "Abilities/GameplayAbility.h"
#include "ActDemoKatanaLightAttackAbility.generated.h"

class UAnimMontage;
class UGameplayEffect;

UCLASS(Abstract, Blueprintable)
class ACTDEMO_API UActDemoKatanaLightAttackAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UActDemoKatanaLightAttackAbility();

protected:
	// 按攻击顺序配置六段。蓝图负责索引、播放和衔接。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Katana|Light Attack")
	TArray<TObjectPtr<UAnimMontage>> AttackMontages;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Katana|Light Attack", meta = (ClampMin = "0.01"))
	float AttackPlayRate = 1.f;

	// 由蓝图决定何时接受缓存的输入以及何时重置连段。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Katana|Light Attack|Combo")
	FGameplayTag ComboInputEventTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Katana|Light Attack|Combo")
	FGameplayTag ComboChainEventTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Katana|Light Attack|Hit")
	FGameplayTag HitEventTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Katana|Light Attack|Hit")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Katana|Light Attack|Hit", meta = (ClampMin = "0.0", Units = "cm"))
	float AttackDistance = 160.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Katana|Light Attack|Hit", meta = (ClampMin = "1.0", Units = "cm"))
	float AttackRadius = 45.f;

	virtual void ActivateAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
};
