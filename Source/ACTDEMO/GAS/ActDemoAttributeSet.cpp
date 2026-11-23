// Fill out your copyright notice in the Description page of Project Settings.


#include "GAS/ActDemoAttributeSet.h"
#include "GameplayEffectExtension.h"
#include "GameplayTagContainer.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "GAS/ACTDEMOGASCharacter.h"
#include "GAS/ACTDEMOEnemyCharacter.h"

UActDemoAttributeSet::UActDemoAttributeSet()
{
	InitMaxHealth(500.0f);
	InitHealth(500.0f);

	InitMaxStamina(100.0f);
	InitStamina(100.0f);

	// 玩家默认不使用敌人招架系统；敌人蓝图在 BeginPlay 初始化为 100。
	InitMaxPosture(0.0f);
	InitPosture(0.0f);
}

void UActDemoAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	//上下限保护，避免Health成为负数也避免其超过上限
	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
	}
	else if (Attribute == GetStaminaAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxStamina());
	}
	else if (Attribute == GetMaxHealthAttribute())
	{
		NewValue = FMath::Max(0.0f, NewValue);			
	}
	else if (Attribute == GetMaxStaminaAttribute())
	{
		NewValue = FMath::Max(0.0f, NewValue);
	}
	else if (Attribute == GetPostureAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxPosture());
	}
	else if (Attribute == GetMaxPostureAttribute())
	{
		NewValue = FMath::Max(0.0f, NewValue);
	}
}

bool UActDemoAttributeSet::PreGameplayEffectExecute(
	FGameplayEffectModCallbackData& Data)
{
	if (!Super::PreGameplayEffectExecute(Data))
	{
		return false;
	}

	// 当前项目通过 Health 的负数 Add 修改来造成伤害。
	const bool bIsHealthDamage =
		Data.EvaluatedData.Attribute == GetHealthAttribute()
		&& Data.EvaluatedData.ModifierOp == EGameplayModOp::Additive
		&& Data.EvaluatedData.Magnitude < 0.0f;

	const bool bIsPostureDamage =
		Data.EvaluatedData.Attribute == GetPostureAttribute()
		&& Data.EvaluatedData.ModifierOp == EGameplayModOp::Additive
		&& Data.EvaluatedData.Magnitude < 0.0f;

	// 治疗、恢复招架、体力等其他修改正常执行。
	if (!bIsHealthDamage && !bIsPostureDamage)
	{
		return true;
	}
	// 已经死亡的攻击来源不能通过迟到的命中回调继续造成近战伤害。
	const AActDemoGASCharacter* SourceCharacter = Cast<AActDemoGASCharacter>(
		Data.EffectSpec.GetContext().GetOriginalInstigator());
	if (SourceCharacter && SourceCharacter->GetCurrentHealth() <= 0.f)
	{
		return false;
	}

	UAbilitySystemComponent* ASC =
		GetOwningAbilitySystemComponent();

	const FGameplayTag InvulnerableTag =
		FGameplayTag::RequestGameplayTag(
			FName(TEXT("State.Combat.Invulnerable")));

	// 有无敌标签时，拒绝本次扣血。
	if (ASC && ASC->HasMatchingGameplayTag(InvulnerableTag))
	{
		return false;
	}

	AACTDEMOEnemyCharacter* Enemy = Cast<AACTDEMOEnemyCharacter>(GetOwningActor());
	if (bIsPostureDamage)
	{
		// T 技能等直接扣招架的效果不能清掉破防期间正在恢复的数值。
		const FGameplayTag RecoveryTag = FGameplayTag::RequestGameplayTag(
			FName(TEXT("State.Combat.GroundRecovery")));
		return !Enemy || (Enemy->GetCurrentHealth() > 0.f && !Enemy->IsPostureBroken() &&
			(!ASC || !ASC->HasMatchingGameplayTag(RecoveryTag)));
	}

	// 敌人在招架未破时完全挡住本次伤害。
	// 本次伤害量同时作为招架值损耗，达到 0 的这一下仍成功招架；后续命中才扣血。
	if (Enemy)
	{
		const float IncomingDamage = FMath::Abs(Data.EvaluatedData.Magnitude);
		AActor* AttackSource = Data.EffectSpec.GetContext().GetEffectCauser();
		if (!IsValid(AttackSource) || AttackSource == Enemy)
		{
			AttackSource = Data.EffectSpec.GetContext().GetOriginalInstigator();
		}
		if (Enemy->TryConsumePosture(IncomingDamage, AttackSource))
		{
			return false;
		}
	}

	return true;
}

void UActDemoAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		SetHealth(
			FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));
	}
	else if (Data.EvaluatedData.Attribute == GetPostureAttribute())
	{
		SetPosture(FMath::Clamp(GetPosture(), 0.0f, GetMaxPosture()));
	}
}
