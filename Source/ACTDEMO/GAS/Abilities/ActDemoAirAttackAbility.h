#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "ActDemoAirAttackAbility.generated.h"

class UGameplayEffect;
class ACharacter;
class UActDemoAirSuspensionComponent;

/** 空中攻击的规则与伤害。蒙太奇、命中通知监听和播放节奏由蓝图子类负责。 */
UCLASS(Abstract, Blueprintable)
class ACTDEMO_API UActDemoAirAttackAbility : public UGameplayAbility
{
    GENERATED_BODY()

public:
    bool CanChainToHeavy() const { return IsActive() && bHeavyFollowupOpen && CanBeCanceled(); }
    UActDemoAirAttackAbility();

protected:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Air Attack|Knockdown", meta=(ClampMin="1.0", Units="cm/s"))
    float KnockdownDownwardSpeed = 1600.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Air Attack|Suspension", meta=(ClampMin="0.01", Units="s"))
    float EnemySuspensionDuration = 1.f;

    // 正常由 EndAbility 释放，超时作为蓝图遗漏结束的保护，不是永久滞空。
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Air Attack|Suspension", meta=(ClampMin="0.01", Units="s"))
    float AttackerSuspensionSafetyDuration = 3.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Air Attack|Damage")
    TSubclassOf<UGameplayEffect> DamageEffectClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Air Attack|Damage", meta=(ClampMin="1.0"))
    float AttackDistance = 180.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Air Attack|Damage", meta=(ClampMin="1.0"))
    float AttackRadius = 75.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Air Attack|Feedback")
    FGameplayTag AirHitReactEventTag;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Air Attack|Feedback")
    FGameplayTag DeathEventTag;

    // 蓝图每次开始一段空中攻击时调用，刷新本段命中资格和出招悬停。
    UFUNCTION(BlueprintCallable, Category="Air Attack|Combo")
    bool BeginAirAttackStage();

    // 命中通知调用。悬停覆盖值为正数时仅用于本次命中，0 使用能力默认值。
    UFUNCTION(BlueprintCallable, Category="Air Attack")
    int32 ProcessAirAttackHit(bool bKnockDown = false, float SuspensionDurationOverride = 0.f);

    // 由轻转重通知调用，段数由蓝图按当前连段判断。
    UFUNCTION(BlueprintCallable, Category="Air Attack")
    void OpenHeavyFollowup();

    virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayTagContainer* SourceTags = nullptr,
        const FGameplayTagContainer* TargetTags = nullptr,
        FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;

    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        bool bReplicateEndAbility, bool bWasCancelled) override;

private:
    UFUNCTION()
    void OnAttackLanded(const FHitResult& Hit);

    TWeakObjectPtr<ACharacter> LandingCharacter;
    TWeakObjectPtr<UActDemoAirSuspensionComponent> AttackerSuspension;
    bool bHitProcessed = false;
    bool bHeavyFollowupOpen = false;
};
