#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "ActDemoFinisherAbility.generated.h"

class ACharacter;
class AController;
class UActDemoAbilityTask_FinisherFollow;
class UActDemoAirSuspensionComponent;
class UActDemoLockCameraBoom;
class UGameplayEffect;
struct FHitResult;

UCLASS(Abstract, Blueprintable)
class ACTDEMO_API UActDemoFinisherAbility : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UActDemoFinisherAbility();

protected:
    UPROPERTY(EditDefaultsOnly, Category="Air Super|Damage", meta=(ClampMin="1.0", Units="cm"))
    float MaxRange = 300.f;

    UPROPERTY(EditDefaultsOnly, Category="Air Super|Damage", meta=(ClampMin="1.0", Units="cm"))
    float HitRadius = 85.f;

    UPROPERTY(EditDefaultsOnly, Category="Air Super|Damage")
    TSubclassOf<UGameplayEffect> DamageEffectClass;

    UPROPERTY(EditDefaultsOnly, Category="Air Super|Suspension", meta=(ClampMin="0.01", Units="s"))
    float SuspensionSafetyDuration = 6.f;

    // Q 实际扣血后才刷新目标悬停，覆盖相邻刀之间的间隔。
    UPROPERTY(EditDefaultsOnly, Category="Air Super|Suspension", meta=(ClampMin="0.0", Units="s"))
    float EnemyHitSuspensionDuration = 1.3f;

    // 首次实际命中后维持刀程；只调整受击者，玩家位移来自蒙太奇根运动。
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Air Super|Target Follow", meta=(ClampMin="60.0", Units="cm"))
    float EnemyFollowDistance = 115.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Air Super|Target Follow", meta=(ClampMin="0.0", Units="cm/s"))
    float EnemyFollowCorrectionSpeed = 600.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Air Super|Target Follow", meta=(ClampMin="100.0", Units="cm"))
    float EnemyFollowBreakDistance = 300.f;

    // 即使宽扫掠碰到了远处胶囊，也不能隔空结算伤害。
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Air Super|Target Follow", meta=(ClampMin="60.0", Units="cm"))
    float MaxContactDistance = 220.f;

    // 末段上挑从目标当前位置再次击飞，速度略低于 R 技能当前的 1600。
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Air Super|Final Uppercut", meta=(ClampMin="1.0", Units="cm/s"))
    float FinalLaunchUpwardSpeed = 1400.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Air Super|Final Uppercut", meta=(ClampMin="0.0", Units="cm/s"))
    float FinalLaunchForwardSpeed = 50.f;

    // 每个命中通知独立执行一次扫掠。
    UFUNCTION(BlueprintCallable, Category="Air Super")
    void ExecuteFinisherHit();

    virtual bool CanActivateAbility(FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr,
        const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
    virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo,
        bool bReplicateEndAbility, bool bWasCancelled) override;

private:
    // 普通命中与末段共用检测、扣血和死亡结算，通知决定本次反应。
    void ProcessFinisherHit(bool bFinalUppercut);

    UFUNCTION()
    void OnFinalUppercutEvent(FGameplayEventData Payload);

    UFUNCTION()
    void OnAttackerLanded(const FHitResult& Hit);

    UPROPERTY(Transient)
    TObjectPtr<UActDemoAbilityTask_FinisherFollow> TargetFollowTask;

    TWeakObjectPtr<ACharacter> LandingCharacter;
    TWeakObjectPtr<AController> MovementInputController;
    TWeakObjectPtr<UActDemoAirSuspensionComponent> PlayerSuspension;
    TWeakObjectPtr<UActDemoLockCameraBoom> OrbitCameraBoom;
};
