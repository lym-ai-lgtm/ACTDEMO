#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "ActDemoAirHeavyAttackAbility.generated.h"

class UGameplayEffect;
class ACharacter;
class UActDemoAirSuspensionComponent;

/** 独立空中重攻击：不继承轻攻击能力。四段动画、输入缓存和切段在蓝图中配置。 */
UCLASS(Abstract, Blueprintable)
class ACTDEMO_API UActDemoAirHeavyAttackAbility : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UActDemoAirHeavyAttackAbility();

protected:
    // 04 对应索引 0，轻转重从 05 对应的索引 1 起手。
    UPROPERTY(BlueprintReadOnly, Category="Air Heavy Attack|Combo")
    int32 InitialHeavyComboIndex = 0;

    // 【悬停参数】实际命中刷新敌人停留时间；玩家出招期间由组件维持悬停。
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Air Heavy Attack|Suspension", meta=(ClampMin="0.01", Units="s"))
    float EnemySuspensionDuration = 1.f;

    // 正常由 EndAbility 释放，超时作为蓝图遗漏结束的保护，不是永久滞空。
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Air Heavy Attack|Suspension", meta=(ClampMin="0.01", Units="s"))
    float AttackerSuspensionSafetyDuration = 3.f;

    // 【独立伤害配置】重攻击蓝图单独选择 GE，不修改轻攻击的伤害资产。
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Air Heavy Attack|Damage")
    TSubclassOf<UGameplayEffect> DamageEffectClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Air Heavy Attack|Damage", meta=(ClampMin="1.0"))
    float AttackDistance = 180.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Air Heavy Attack|Damage", meta=(ClampMin="1.0"))
    float AttackRadius = 75.f;

    // 【反馈事件】存活目标播放空中受击；致死目标只走死亡，重攻击不自动击落。
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Air Heavy Attack|Feedback")
    FGameplayTag AirHitReactEventTag;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Air Heavy Attack|Feedback")
    FGameplayTag DeathEventTag;

    // 蓝图每次开始一段空中攻击时调用，刷新本段命中资格和出招悬停。
    UFUNCTION(BlueprintCallable, Category="Air Heavy Attack|Combo")
    bool BeginAirHeavyAttackStage();

    // 由蓝图在命中通知事件到达时调用；每段最多处理一次命中。
    UFUNCTION(BlueprintCallable, Category="Air Heavy Attack")
    int32 ProcessAirHeavyAttackHit();

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
    // 【本次能力状态】结束时释放自身悬停；每段命中标记由 BeginAirHeavyAttackStage 重置。
    TWeakObjectPtr<UActDemoAirSuspensionComponent> AttackerSuspension;
    bool bHitProcessed = false;
};
