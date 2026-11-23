#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "ActDemoAirSlashAbility.generated.h"

class UGameplayEffect;

/** 独立 E 挑飞斩：地面发动，C++ 管规则/伤害/挑飞，蓝图管动画和输入。 */
UCLASS(Abstract, Blueprintable)
class ACTDEMO_API UActDemoAirSlashAbility : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UActDemoAirSlashAbility();

protected:
    // 【独立伤害配置】使用本技能自己的 GE，不修改普通攻击的伤害资产。
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Air Slash|Damage")
    TSubclassOf<UGameplayEffect> DamageEffectClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Air Slash|Damage", meta=(ClampMin="1.0", Units="cm"))
    float AttackDistance = 220.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Air Slash|Damage", meta=(ClampMin="1.0", Units="cm"))
    float AttackRadius = 85.f;

    // 【挑飞参数】与已验证的上挑高度保持相同初值，蓝图中可独立调节。
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Air Slash|Launch", meta=(ClampMin="1.0", Units="cm/s"))
    float LaunchUpwardSpeed = 1600.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Air Slash|Launch", meta=(ClampMin="0.0", Units="cm/s"))
    float LaunchForwardSpeed = 100.f;

    // 【反馈事件】成功挑飞、普通命中、致死分别交给已有蓝图表现，不在这里播放蒙太奇。
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Air Slash|Feedback")
    FGameplayTag LaunchReactEventTag;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Air Slash|Feedback")
    FGameplayTag HitReactEventTag;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Air Slash|Feedback")
    FGameplayTag DeathEventTag;

    // 【通知入口】E 技能命中通知调用；同一次技能最多检测一次，R 取消后不能继续命中。
    UFUNCTION(BlueprintCallable, Category="Air Slash")
    int32 ProcessAirSlashHit();

    virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayTagContainer* SourceTags = nullptr,
        const FGameplayTagContainer* TargetTags = nullptr,
        FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;

private:
    // 【单次命中状态】激活重置，不跨技能复用轻/重攻击的命中或连击变量。
    bool bHitProcessed = false;
};
