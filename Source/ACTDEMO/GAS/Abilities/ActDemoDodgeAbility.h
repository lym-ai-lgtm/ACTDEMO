// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "ActDemoDodgeAbility.generated.h"

class UAnimMontage;
class UBlendSpace;
class UAbilitySystemComponent;

UCLASS()
class ACTDEMO_API UActDemoDodgeAbility : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UActDemoDodgeAbility();

    UFUNCTION(BlueprintCallable, Category = "Dodge")
    static bool TryActivateDirectionalDodge(UAbilitySystemComponent* AbilitySystem,
        TSubclassOf<UActDemoDodgeAbility> AbilityClass, UBlendSpace* Directions);

protected:
    // 在蓝图子类中指定翻滚蒙太奇。
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dodge")
    TObjectPtr<UAnimMontage> DodgeMontage;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dodge",
        meta = (ClampMin = "0.1"))
    float DodgePlayRate = 1.0f;

    // 复用朝角色前方位移的翻滚动画，锁定时按世界空间输入调整朝向。
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dodge|Direction")
    bool bDirectionalDodgeWhenLocked = true;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dodge|Direction")
    bool bDodgeBackwardWithoutInput = true;

    // 仅对复用前滚动画的方向组放大纯左、纯右动作的根运动位移。
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dodge|Direction",
        meta = (ClampMin = "0.1"))
    float ReusedForwardSetSideDistanceScale = 1.5f;

    // 在能力正式激活前判断是否允许翻滚。
    virtual bool CanActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayTagContainer* SourceTags = nullptr,
        const FGameplayTagContainer* TargetTags = nullptr,
        FGameplayTagContainer* OptionalRelevantTags = nullptr
    ) const override;

    virtual void ActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;

private:
    UFUNCTION()
    void OnDodgeCompleted();

    UFUNCTION()
    void OnDodgeInterrupted();

    void FinishDodge(bool bCancelled);
};
