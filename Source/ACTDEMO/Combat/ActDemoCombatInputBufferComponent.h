#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "ActDemoCombatInputBufferComponent.generated.h"

class UAbilitySystemComponent;
class UGameplayAbility;

// 【输入意图】缓存按键的含义；重放时由蓝图重新判断空地、武器和锁定目标。
UENUM(BlueprintType)
enum class EActDemoBufferedCombatInput : uint8
{
    LightAttack,
    AirHeavyAttack,
    LaunchOrChase,
    AirSlam,
    AirSlash,
    Dodge
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FActDemoBufferedInputReady,
    EActDemoBufferedCombatInput, Input);

/** 单槽、短时输入缓冲；不播放动画，不绕过 GAS，也不接管已有连击缓存。 */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class ACTDEMO_API UActDemoCombatInputBufferComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UActDemoCombatInputBufferComponent();

    // 【原输入入口】替换原 TryActivateAbilityByClass，前置蓝图条件和连击事件保留。
    // 返回值仅表示是否立即激活；失败且正处攻击/翻滚时可能缓存，不代表已发动。
    UFUNCTION(BlueprintCallable, Category="Combat|Input Buffer")
    bool TryActivateOrBuffer(TSubclassOf<UGameplayAbility> AbilityClass,
        EActDemoBufferedCombatInput Input);

    // 【重放出口】蓝图绑定此事件，再进入原按键的条件分支；不能直接无条件播放动画。
    UPROPERTY(BlueprintAssignable, Category="Combat|Input Buffer")
    FActDemoBufferedInputReady OnBufferedInputReady;

    UFUNCTION(BlueprintCallable, Category="Combat|Input Buffer")
    void ClearBufferedInput();

    UFUNCTION(BlueprintPure, Category="Combat|Input Buffer")
    bool HasBufferedInput() const { return bHasBufferedInput; }

    // 【可调手感】只记住最近一次输入，超过时间丢弃。无逐帧轮询，无无限重试。
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Input Buffer",
        meta=(ClampMin="0.01", ClampMax="0.5"))
    float BufferDuration = 0.2f;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void PollBufferedInput();
    void OnBlockingStateChanged(const FGameplayTag Tag, int32 NewCount);
    bool CanAcceptInput(UAbilitySystemComponent* ASC) const;

    TWeakObjectPtr<UAbilitySystemComponent> BoundASC;
    FDelegateHandle HitReactHandle;
    FDelegateHandle DeadHandle;
    FTimerHandle PollTimer;
    EActDemoBufferedCombatInput BufferedInput = EActDemoBufferedCombatInput::LightAttack;
    double ExpiresAt = 0.0;
    bool bHasBufferedInput = false;
};
