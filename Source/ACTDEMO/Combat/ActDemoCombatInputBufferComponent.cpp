#include "Combat/ActDemoCombatInputBufferComponent.h"

#include "AbilitySystemComponent.h"
#include "GameplayAbilitySpec.h"
#include "GAS/ACTDEMOGASCharacter.h"
#include "Engine/World.h"
#include "TimerManager.h"

namespace
{
    FGameplayTag StateTag(const TCHAR* Name)
    {
        return FGameplayTag::RequestGameplayTag(FName(Name), false);
    }

    bool HasState(const UAbilitySystemComponent* ASC, const TCHAR* Name)
    {
        const FGameplayTag Tag = StateTag(Name);
        return ASC && Tag.IsValid() && ASC->HasMatchingGameplayTag(Tag);
    }

    bool IsFinishingAction(const UAbilitySystemComponent* ASC)
    {
        return HasState(ASC, TEXT("State.Combat.Attacking")) ||
            HasState(ASC, TEXT("State.Combat.Dodging"));
    }
}

UActDemoCombatInputBufferComponent::UActDemoCombatInputBufferComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

// 【中断监听】受击/死亡一旦进入就清空，不让此前的按键在硬直结束后意外发动。
void UActDemoCombatInputBufferComponent::BeginPlay()
{
    Super::BeginPlay();
    const AActDemoGASCharacter* Character = Cast<AActDemoGASCharacter>(GetOwner());
    UAbilitySystemComponent* ASC = Character ? Character->GetAbilitySystemComponent() : nullptr;
    if (!ASC) return;
    BoundASC = ASC;
    const FGameplayTag HitReact = StateTag(TEXT("State.Combat.HitReact"));
    const FGameplayTag Dead = StateTag(TEXT("State.Combat.Dead"));
    if (HitReact.IsValid())
        HitReactHandle = ASC->RegisterGameplayTagEvent(HitReact).AddUObject(this,
            &UActDemoCombatInputBufferComponent::OnBlockingStateChanged);
    if (Dead.IsValid())
        DeadHandle = ASC->RegisterGameplayTagEvent(Dead).AddUObject(this,
            &UActDemoCombatInputBufferComponent::OnBlockingStateChanged);
}

bool UActDemoCombatInputBufferComponent::CanAcceptInput(UAbilitySystemComponent* ASC) const
{
    const AActDemoGASCharacter* Character = Cast<AActDemoGASCharacter>(GetOwner());
    return IsValid(Character) && Character->HasAuthority() && Character->GetCurrentHealth() > 0.f &&
        ASC && !HasState(ASC, TEXT("State.Combat.Dead")) &&
        !HasState(ASC, TEXT("State.Combat.HitReact"));
}

bool UActDemoCombatInputBufferComponent::TryActivateOrBuffer(
    TSubclassOf<UGameplayAbility> AbilityClass, EActDemoBufferedCombatInput Input)
{
    // 【最新输入覆盖】先丢弃旧意图。成功的 R/F/翻滚等高优先动作不能残留旧攻击。
    ClearBufferedInput();
    UAbilitySystemComponent* ASC = BoundASC.Get();
    if (!CanAcceptInput(ASC) || !AbilityClass || !GetWorld()) return false;

    // 【已有连击优先】原蓝图已经先发 Input 事件。能力仍活跃时只让其处理续段，
    // 不额外保存一次点击，否则连段结束后会多打第一段。未授予的能力也不缓存。
    const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromClass(AbilityClass);
    if (!Spec || Spec->IsActive()) return false;

    // 【保持打断优先级】先按原 GAS 规则尝试，允许 R/F/翻滚的既有取消即时生效。
    if (ASC->TryActivateAbilityByClass(AbilityClass)) return true;

    // 【短时缓存】只有其它攻击/翻滚仍在进行时才等待；单纯站立冷却中不排队。
    // 等待期内冷却/空地等限制不会被忽略，重放仍走原蓝图与 GAS。
    if (!CanAcceptInput(ASC) || !IsFinishingAction(ASC) || !OnBufferedInputReady.IsBound())
        return false;

    BufferedInput = Input;
    const float Duration = FMath::IsFinite(BufferDuration) ? FMath::Clamp(BufferDuration, 0.01f, 0.5f) : 0.2f;
    ExpiresAt = GetWorld()->GetTimeSeconds() + Duration;
    bHasBufferedInput = true;
    GetWorld()->GetTimerManager().SetTimer(PollTimer, this,
        &UActDemoCombatInputBufferComponent::PollBufferedInput, 0.01f, true);
    return false;
}

// 【等待与消费】只在有缓存时每 0.01 秒检查，先清槽再发事件，避免重入重复执行。
void UActDemoCombatInputBufferComponent::PollBufferedInput()
{
    UWorld* World = GetWorld();
    UAbilitySystemComponent* ASC = BoundASC.Get();
    if (!bHasBufferedInput || !World || !CanAcceptInput(ASC) || World->GetTimeSeconds() >= ExpiresAt)
    {
        ClearBufferedInput();
        return;
    }
    if (IsFinishingAction(ASC)) return;

    const EActDemoBufferedCombatInput Input = BufferedInput;
    ClearBufferedInput();
    OnBufferedInputReady.Broadcast(Input);
}

void UActDemoCombatInputBufferComponent::ClearBufferedInput()
{
    bHasBufferedInput = false;
    ExpiresAt = 0.0;
    if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(PollTimer);
}

void UActDemoCombatInputBufferComponent::OnBlockingStateChanged(const FGameplayTag Tag, int32 NewCount)
{
    if (NewCount > 0) ClearBufferedInput();
}

// 【生命周期清理】销毁/切关卡解除 ASC 委托与定时器，不保留旧角色的输入。
void UActDemoCombatInputBufferComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ClearBufferedInput();
    if (UAbilitySystemComponent* ASC = BoundASC.Get())
    {
        const FGameplayTag HitReact = StateTag(TEXT("State.Combat.HitReact"));
        const FGameplayTag Dead = StateTag(TEXT("State.Combat.Dead"));
        if (HitReact.IsValid()) ASC->RegisterGameplayTagEvent(HitReact).Remove(HitReactHandle);
        if (Dead.IsValid()) ASC->RegisterGameplayTagEvent(Dead).Remove(DeadHandle);
    }
    BoundASC.Reset();
    Super::EndPlay(EndPlayReason);
}
