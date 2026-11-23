// Fill out your copyright notice in the Description page of Project Settings.
//C++实施检测与施加效果，播放蒙太奇动画。蓝图负责选择伤害效果、调整距离和范围
#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "ActDemoLightAttackAbility.generated.h"

class UAnimMontage;
class UGameplayEffect;
class UAbilityTask_WaitDelay;
class UAbilityTask_PlayMontageAndWait;

UCLASS()
class ACTDEMO_API UActDemoLightAttackAbility : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UActDemoLightAttackAbility();

protected:
    // 数组顺序就是攻击顺序：0 第一段、1 第二段、2 第三段。攻击动画映射数组
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Light Attack")
    TArray<TObjectPtr<UAnimMontage>> AttackMontages;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Light Attack",
        meta = (ClampMin = "0.01"))
    float AttackPlayRate = 1.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Light Attack|Feedback",
        meta=(ClampMin="0.0", ClampMax="0.15", Units="s"))
    float HitStopDuration = 0.04f;

    // 当前段完全结束后，允许追加下一段的等待时间。
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Light Attack|Combo",
        meta = (ClampMin = "0.01"))
    float ComboResetDelay = 0.5f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Light Attack|Combo")
    FGameplayTag ComboInputEventTag;

    // 动画到达允许衔接下一段的位置时发送此事件。
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,Category = "Light Attack|Combo")
    FGameplayTag ComboChainEventTag;


	//受击目标进行游戏效果反馈，进行扣血并播放受击动画（通过事件标签进行GA游戏能力触发，然后播放蒙太奇）
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Light Attack|Damage")
    TSubclassOf<UGameplayEffect> DamageEffectClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Light Attack|Damage")
    FGameplayTag HitEventTag;
    
    // 命中并造成实际伤害后，发送给受击目标的事件。
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Hit Reaction")
    FGameplayTag HitReactEventTag;

    //生命值归0后播放死亡动画
    // 目标生命值归零时发送的事件。
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Death")
    FGameplayTag DeathEventTag;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Light Attack|Damage",
        meta = (ClampMin = "0.0"))
    float AttackDistance = 160.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Light Attack|Damage",
        meta = (ClampMin = "1.0"))
    float AttackRadius = 45.0f;

    // 普通轻攻击保持关闭，独立挑飞能力蓝图开启。
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Launch")
    bool bLaunchEnemyOnHit = false;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Launch", meta=(ClampMin="1.0"))
    float LaunchUpwardSpeed = 900.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Launch", meta=(ClampMin="0.0"))
    float LaunchForwardSpeed = 100.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Launch")
    FGameplayTag LaunchReactEventTag;

    virtual void ActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;

    virtual void EndAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        bool bReplicateEndAbility,
        bool bWasCancelled) override;

private:
    // 当前播放的数组下标。
    int32 ComboIndex = 0;

    // 本段期间是否收到了追加点击。
    bool bNextAttackRequested = false;

    // 当前是否正在播放一段攻击。
    bool bStagePlaying = false;

    // 是否处于段与段之间的 0.5 秒等待期。
    bool bWaitingForNextInput = false;

    // 当前段是否已经执行过命中检测。
    bool bHitProcessedThisStage = false;

    UPROPERTY(Transient)
    TObjectPtr<UAbilityTask_WaitDelay> ComboWaitTask;

    void PlayCurrentStage();
    void FinishCombo(bool bCancelled);

    UFUNCTION()
    void OnComboInputReceived(FGameplayEventData Payload);

    UFUNCTION()
    void OnAttackCompleted();

    UFUNCTION()
    void OnAttackInterrupted();

    UFUNCTION()
    void OnComboWaitExpired();

    UFUNCTION()
    void OnHitEventReceived(FGameplayEventData Payload);

    // 当前段是否已经进入允许衔接的阶段。
    bool bCanChain = false;

    // 保存当前蒙太奇任务，切段前解除旧任务的回调。
    UPROPERTY(Transient)
    TObjectPtr<UAbilityTask_PlayMontageAndWait> ActiveMontageTask;

    void AdvanceToNextStage();

    UFUNCTION()
    void OnComboChainReceived(FGameplayEventData Payload);
};

