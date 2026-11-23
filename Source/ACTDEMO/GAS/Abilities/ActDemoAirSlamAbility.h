#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "ActDemoAirSlamAbility.generated.h"

class UGameplayEffect;
class UActDemoAirSuspensionComponent;
class ACharacter;
class UNiagaraSystem;
class UCameraShakeBase;
struct FHitResult;

/** 独立空中下砸：一次命中击落敌人，动画与 F 输入由蓝图配置。 */
UCLASS(Abstract, Blueprintable)
class ACTDEMO_API UActDemoAirSlamAbility : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UActDemoAirSlamAbility();

protected:
    // 【击落参数】命中时敌人与玩家使用此向下速度，实际落地才切换地面表现。
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Air Slam|Knockdown", meta=(ClampMin="1.0", Units="cm/s"))
    float DownwardSpeed = 1800.f;

    // 【跟随下砸】起手暂时悬停，命中通知到达后玩家也向下冲；关闭可保留旧表现。
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Air Slam|Descent")
    bool bFollowDownward = true;

    // 正常由 EndAbility 释放，超时作为蓝图遗漏结束的保护，不是永久滞空。
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Air Slam|Suspension", meta=(ClampMin="0.01", Units="s"))
    float AttackerSuspensionSafetyDuration = 3.f;

    // 【独立伤害配置】下砸蓝图单独选择 GE，不修改轻、重攻击的伤害资产。
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Air Slam|Damage")
    TSubclassOf<UGameplayEffect> DamageEffectClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Air Slam|Damage", meta=(ClampMin="1.0"))
    float AttackDistance = 200.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Air Slam|Damage", meta=(ClampMin="1.0"))
    float AttackRadius = 85.f;

    // 【反馈事件】存活且实际扣血后击落并发送受击事件；致死目标只走死亡。
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Air Slam|Feedback")
    FGameplayTag AirHitReactEventTag;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Air Slam|Feedback")
    FGameplayTag DeathEventTag;

    UPROPERTY(EditDefaultsOnly, Category="Air Slam|Landing Feedback")
    TObjectPtr<UNiagaraSystem> LandingDust;

    UPROPERTY(EditDefaultsOnly, Category="Air Slam|Landing Feedback")
    TSubclassOf<UCameraShakeBase> LandingShake;

    UPROPERTY(EditDefaultsOnly, Category="Air Slam|Landing Feedback", meta=(ClampMin="0.0"))
    float LandingShakeScale = 0.35f;

    // 【发动准备】单段技能内部初始化，不向蓝图暴露无意义的连段接口。
    bool PrepareAirSlam();

    // 【命中入口】蓝图收到 AN_AirSlamHit 后调用，一次技能最多处理一次命中。
    UFUNCTION(BlueprintCallable, Category="Air Slam")
    int32 ProcessAirSlamHit();

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
    virtual void OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
    void ClearLandingFeedback();
    // 【下冲与落地】只操作角色移动和能力生命周期，不在 C++ 中播放或切换蒙太奇。
    void StartDownwardFollow(ACharacter* Character);

    UFUNCTION()
    void OnAttackLanded(const FHitResult& Hit);

    // 【统一清理】结束/取消时解除落地监听并释放悬停，不把死亡的移动重新打开。
    TWeakObjectPtr<ACharacter> LandingCharacter;
    TWeakObjectPtr<UActDemoAirSuspensionComponent> AttackerSuspension;
    bool bHitProcessed = false;
    bool bDescentStarted = false;
    bool bLandingFeedbackPending = false;
};
