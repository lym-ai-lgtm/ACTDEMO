// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "ACTDEMOCharacter.h"
#include "ActDemoGASCharacter.generated.h"

class UAbilitySystemComponent;
class UActDemoAttributeSet;
class UActDemoKatanaEquipAbility;
class UGameplayAbility;
class UInputAction;
class UActDemoCombatInputBufferComponent;

UCLASS()
class ACTDEMO_API AActDemoGASCharacter
	: public AACTDEMOCharacter
	, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AActDemoGASCharacter();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION(BlueprintPure, Category = "GAS|Attributes")
	float GetCurrentHealth() const;

	UFUNCTION(BlueprintPure, Category = "GAS|Attributes")
	float GetCurrentMaxHealth() const;

	UFUNCTION(BlueprintPure, Category = "GAS|Attributes")
	float GetCurrentStamina() const;

	UFUNCTION(BlueprintPure, Category = "GAS|Katana")
	float GetKatanaMovementSpeed(bool bSprinting) const;

	UFUNCTION(BlueprintPure, Category="Combat|Momentum")
	float GetMomentum() const { return Momentum; }

	UFUNCTION(BlueprintPure, Category="Combat|Momentum")
	float GetMaxMomentum() const { return MaxMomentum; }

	void GainAirComboMomentum();
	bool HasFullMomentum() const { return Momentum >= MaxMomentum; }
	bool SpendFullMomentum();

protected:
	UPROPERTY(EditDefaultsOnly, Category="Combat|Momentum", meta=(ClampMin="1.0"))
	float MaxMomentum = 100.f;

	UPROPERTY(EditDefaultsOnly, Category="Combat|Momentum", meta=(ClampMin="0.0"))
	float MomentumPerAirHit = 20.f;

	// 【跨动作输入】单独组件负责短时缓存，具体按键/动作选择仍在玩家蓝图。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Input Buffer")
	TObjectPtr<UActDemoCombatInputBufferComponent> CombatInputBuffer;

	// 每种角色蓝图可以分别设置自己的初始属性。

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
		Category = "GAS|Initial Attributes",
		meta = (ClampMin = "1.0"))
	float InitialMaxHealth = 500.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
		Category = "GAS|Initial Attributes",
		meta = (ClampMin = "0.0"))
	float InitialHealth = 500.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
		Category = "GAS|Initial Attributes",
		meta = (ClampMin = "0.0"))
	float InitialMaxStamina = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
		Category = "GAS|Initial Attributes",
		meta = (ClampMin = "0.0"))
	float InitialStamina = 100.0f;

	//添加技能和输入属性
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Test")
	TSubclassOf<UGameplayAbility> DebugAbilityClass;

	// 在支持太刀的角色蓝图里指定。服务器在蓝图 BeginPlay 前注册属性并授予能力。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Katana")
	TSubclassOf<UActDemoKatanaEquipAbility> KatanaEquipAbilityClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Katana", meta = (ClampMin = "0.0", Units = "cm/s"))
	float InitialKatanaWalkSpeed = 180.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Katana", meta = (ClampMin = "0.0", Units = "cm/s"))
	float InitialKatanaRunSpeed = 440.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> DebugAbilityAction;

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	void OnDebugAbilityPressed();

protected:
	virtual void BeginPlay() override;
	//能力系统组件
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;
	//属性集组件
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UActDemoAttributeSet> AttributeSet;

private:
	float Momentum = 0.f;
};
