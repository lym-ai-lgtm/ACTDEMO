// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "ActDemoAttributeSet.generated.h"

/**
 * 
 */
UCLASS()
class ACTDEMO_API UActDemoAttributeSet : public UAttributeSet
{
	GENERATED_BODY()
	
public:
	//构建属性集
	UActDemoAttributeSet();

	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS_BASIC(UActDemoAttributeSet, Health)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS_BASIC(UActDemoAttributeSet, MaxHealth)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData Stamina;
	ATTRIBUTE_ACCESSORS_BASIC(UActDemoAttributeSet, Stamina)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData MaxStamina;
	ATTRIBUTE_ACCESSORS_BASIC(UActDemoAttributeSet, MaxStamina)

	// 敌人招架值。招架值归零前，收到的伤害会被拦截并转为招架值损耗。
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Posture")
	FGameplayAttributeData Posture;
	ATTRIBUTE_ACCESSORS_BASIC(UActDemoAttributeSet, Posture)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Posture")
	FGameplayAttributeData MaxPosture;
	ATTRIBUTE_ACCESSORS_BASIC(UActDemoAttributeSet, MaxPosture)


	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	// GameplayEffect 修改属性前执行。
	// 返回 false 时，拒绝这次属性修改。扣血前检查，即无敌期间拦截伤害
	virtual bool PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data) override;
	//扣血后的生命值限制
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
};
