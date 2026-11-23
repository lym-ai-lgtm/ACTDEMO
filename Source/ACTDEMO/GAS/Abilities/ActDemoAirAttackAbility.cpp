#include "GAS/Abilities/ActDemoAirAttackAbility.h"
#include "Combat/ActDemoAttackFacing.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GAS/ACTDEMOEnemyCharacter.h"
#include "GAS/ACTDEMOGASCharacter.h"
#include "GAS/ActDemoAttributeSet.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Combat/ActDemoAirSuspensionComponent.h"

UActDemoAirAttackAbility::UActDemoAirAttackAbility()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
    bRetriggerInstancedAbility = false;
}

bool UActDemoAirAttackAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
    const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
    if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
        return false;

    const AActDemoGASCharacter* Character = ActorInfo
        ? Cast<AActDemoGASCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
    return Character && Character->GetCurrentHealth() > 0.f && DamageEffectClass &&
        Character->GetCharacterMovement()->IsFalling();
}

void UActDemoAirAttackAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }
    if (!BeginAirAttackStage())
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }
    // 调用基类分发到蓝图 Event ActivateAbility；这里不播放任何蒙太奇。
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

bool UActDemoAirAttackAbility::BeginAirAttackStage()
{
    AActDemoGASCharacter* Attacker = Cast<AActDemoGASCharacter>(GetAvatarActorFromActorInfo());
    if (!IsActive() || !IsValid(Attacker) || !Attacker->HasAuthority() ||
        Attacker->GetCurrentHealth() <= 0.f || !Attacker->GetCharacterMovement()->IsFalling())
        return false;

    // 【统一攻击朝向】每一段空中轻攻击起手重新对准，未锁定时保持当前朝向。
    ActDemoAttackFacing::FaceLockedTarget(Attacker);
    bHitProcessed = false;
    bHeavyFollowupOpen = false;
    LandingCharacter = Attacker;
    Attacker->LandedDelegate.AddUniqueDynamic(this, &UActDemoAirAttackAbility::OnAttackLanded);
    UActDemoAirSuspensionComponent* Suspension = Attacker->FindComponentByClass<UActDemoAirSuspensionComponent>();
    if (Suspension && Suspension->SuspendFor(AttackerSuspensionSafetyDuration))
        AttackerSuspension = Suspension;
    return true;
}

// 落地结束当前空中连段，由能力任务停止蒙太奇。
void UActDemoAirAttackAbility::OnAttackLanded(const FHitResult& Hit)
{
    if (IsActive())
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UActDemoAirAttackAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility, bool bWasCancelled)
{
    if (ACharacter* Character = LandingCharacter.Get())
        Character->LandedDelegate.RemoveDynamic(this, &UActDemoAirAttackAbility::OnAttackLanded);
    LandingCharacter.Reset();
    if (UActDemoAirSuspensionComponent* Suspension = AttackerSuspension.Get())
        Suspension->ReleaseSuspension();
    AttackerSuspension.Reset();
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UActDemoAirAttackAbility::OpenHeavyFollowup()
{
    // 蓝图按段数筛选，蒙太奇通知决定切入时机，挥空也可以开放变体。
    if (IsActive())
        bHeavyFollowupOpen = true;
}

int32 UActDemoAirAttackAbility::ProcessAirAttackHit(bool bKnockDown, float SuspensionDurationOverride)
{
    AActDemoGASCharacter* Attacker = Cast<AActDemoGASCharacter>(GetAvatarActorFromActorInfo());
    UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
    if (!IsActive() || bHitProcessed || !IsValid(Attacker) || !Attacker->HasAuthority() ||
        Attacker->GetCurrentHealth() <= 0.f || !Attacker->GetCharacterMovement()->IsFalling() ||
        !SourceASC || !DamageEffectClass) return 0;

    bHitProcessed = true;
    const FVector Start = Attacker->GetActorLocation();
    const FVector End = Start + Attacker->GetActorForwardVector() * FMath::Max(1.f, AttackDistance);
    TArray<TEnumAsByte<EObjectTypeQuery>> Types;
    Types.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));
    TArray<AActor*> Ignore;
    Ignore.Add(Attacker);
    TArray<FHitResult> Hits;
    UKismetSystemLibrary::SphereTraceMultiForObjects(Attacker, Start, End,
        FMath::Max(1.f, AttackRadius), Types, false, Ignore, EDrawDebugTrace::None, Hits, true);

    TSet<AActor*> Processed;
    int32 DamagedCount = 0;
    for (const FHitResult& Hit : Hits)
    {
        AACTDEMOEnemyCharacter* Enemy = Cast<AACTDEMOEnemyCharacter>(Hit.GetActor());
        // 第一版只攻击仍处于挑飞状态的敌人，落地倒地流程不在这里重置。
        if (!IsValid(Enemy) || !Enemy->IsLaunched() || Enemy->GetCurrentHealth() <= 0.f ||
            Processed.Contains(Enemy)) continue;
        Processed.Add(Enemy);
        UAbilitySystemComponent* TargetASC = Enemy->GetAbilitySystemComponent();
        if (!TargetASC) continue;

        FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
        Context.AddSourceObject(Attacker);
        Context.AddHitResult(Hit);
        const FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(DamageEffectClass,
            GetAbilityLevel(), Context);
        if (!Spec.IsValid()) continue;

        const float Before = Enemy->GetCurrentHealth();
        SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
        const float After = Enemy->GetCurrentHealth();
        if (After >= Before) continue;
        ++DamagedCount;
        if (After > 0.f && !Enemy->IsAirKnockedDown())
        {
            if (bKnockDown)
                Enemy->KnockDownFromAir(KnockdownDownwardSpeed);
            else if (UActDemoAirSuspensionComponent* Suspension = Enemy->FindComponentByClass<UActDemoAirSuspensionComponent>())
            {
                const float Duration = FMath::IsFinite(SuspensionDurationOverride) &&
                    SuspensionDurationOverride > 0.f ? SuspensionDurationOverride : EnemySuspensionDuration;
                Suspension->SuspendFor(Duration);
            }
        }
        const FGameplayTag Feedback = After <= 0.f ? DeathEventTag : AirHitReactEventTag;
        if (Feedback.IsValid())
        {
            FGameplayEventData Event;
            Event.EventTag = Feedback;
            Event.Instigator = Attacker;
            Event.Target = Enemy;
            Event.EventMagnitude = Before - After;
            Event.ContextHandle = Context;
            UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Enemy, Feedback, Event);
        }
    }
    // 一段攻击扫到多名敌人也只积攒一次气势。
    if (DamagedCount > 0 && IsValid(Attacker))
        Attacker->GainAirComboMomentum();
    return DamagedCount;
}
