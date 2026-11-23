#include "GAS/Abilities/ActDemoAirHeavyAttackAbility.h"
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

namespace
{
    FGameplayAbilitySpecHandle FindLightFollowup(const UAbilitySystemComponent& ASC)
    {
        for (const FGameplayAbilitySpec& Spec : ASC.GetActivatableAbilities())
        {
            const UActDemoAirAttackAbility* Light = Cast<UActDemoAirAttackAbility>(Spec.GetPrimaryInstance());
            if (Spec.IsActive() && Light && Light->CanChainToHeavy())
                return Spec.Handle;
        }
        return {};
    }
}

// 【实例策略】沿用单机 Demo 的服务器执行方式，重复右键由蓝图缓存，不重启能力。
UActDemoAirHeavyAttackAbility::UActDemoAirHeavyAttackAbility()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
    bRetriggerInstancedAbility = false;
}

// 【激活校验】先遵循 GAS 标签互斥，再检查角色存活、空中状态及独立伤害效果配置。
bool UActDemoAirHeavyAttackAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
    const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
    if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
        return false;

    const AActDemoGASCharacter* Character = ActorInfo
        ? Cast<AActDemoGASCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
    const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
    if (!ASC)
        return false;

    const FGameplayTag AttackingTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Combat.Attacking")));
    const int32 AttackCount = ASC->GetTagCount(AttackingTag);
    // 只有当前唯一攻击是已打开衔接窗口的空中轻击，才允许在攻击中发动。
    if (AttackCount > 0 && (AttackCount != 1 || !FindLightFollowup(*ASC).IsValid()))
        return false;

    return Character && Character->GetCurrentHealth() > 0.f && DamageEffectClass &&
        Character->GetCharacterMovement()->IsFalling();
}

// 【能力入口】整套重攻击只提交一次成本；动画和四段切换交给蓝图 Event ActivateAbility。
void UActDemoAirHeavyAttackAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }
    // 先结束轻击，让它释放自己的悬停，再由重击重新申请悬停。
    InitialHeavyComboIndex = 0;
    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
    {
        const FGameplayAbilitySpecHandle LightHandle = FindLightFollowup(*ASC);
        if (LightHandle.IsValid())
        {
            InitialHeavyComboIndex = 1;
            ASC->CancelAbilityHandle(LightHandle);
        }
    }
    if (!BeginAirHeavyAttackStage())
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }
    // 调用基类分发到蓝图 Event ActivateAbility；这里不播放任何蒙太奇。
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

// 【每段初始化】允许本段命中一次，并刷新玩家出招悬停；不替敌人刷新未命中的悬停。
bool UActDemoAirHeavyAttackAbility::BeginAirHeavyAttackStage()
{
    AActDemoGASCharacter* Attacker = Cast<AActDemoGASCharacter>(GetAvatarActorFromActorInfo());
    if (!IsActive() || !IsValid(Attacker) || !Attacker->HasAuthority() ||
        Attacker->GetCurrentHealth() <= 0.f || !Attacker->GetCharacterMovement()->IsFalling())
        return false;

    // 【统一攻击朝向】每一段空中重攻击起手重新对准，未锁定时保持当前朝向。
    ActDemoAttackFacing::FaceLockedTarget(Attacker);
    bHitProcessed = false;
    LandingCharacter = Attacker;
    Attacker->LandedDelegate.AddUniqueDynamic(this, &UActDemoAirHeavyAttackAbility::OnAttackLanded);
    UActDemoAirSuspensionComponent* Suspension = Attacker->FindComponentByClass<UActDemoAirSuspensionComponent>();
    if (Suspension && Suspension->SuspendFor(AttackerSuspensionSafetyDuration))
        AttackerSuspension = Suspension;
    return true;
}

// 【统一清理】正常结束、受击或死亡取消都释放玩家悬停，不修改死亡移动模式。
// 落地结束当前空中连段，由能力任务停止蒙太奇。
void UActDemoAirHeavyAttackAbility::OnAttackLanded(const FHitResult& Hit)
{
    if (IsActive())
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UActDemoAirHeavyAttackAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility, bool bWasCancelled)
{
    if (ACharacter* Character = LandingCharacter.Get())
        Character->LandedDelegate.RemoveDynamic(this, &UActDemoAirHeavyAttackAbility::OnAttackLanded);
    LandingCharacter.Reset();
    if (UActDemoAirSuspensionComponent* Suspension = AttackerSuspension.Get())
        Suspension->ReleaseSuspension();
    AttackerSuspension.Reset();
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// 【命中入口】仅由重攻击命中通知调用；每段最多检测一次，防止重复通知重复扣血。
int32 UActDemoAirHeavyAttackAbility::ProcessAirHeavyAttackHit()
{
    AActDemoGASCharacter* Attacker = Cast<AActDemoGASCharacter>(GetAvatarActorFromActorInfo());
    UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
    if (!IsActive() || bHitProcessed || !IsValid(Attacker) || !Attacker->HasAuthority() ||
        Attacker->GetCurrentHealth() <= 0.f || !Attacker->GetCharacterMovement()->IsFalling() ||
        !SourceASC || !DamageEffectClass) return 0;

    bHitProcessed = true;
    // 【空间检测】沿角色前方球形扫掠 Pawn，距离/半径在重攻击蓝图中独立调节。
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
        // 【目标筛选】只处理存活的挑飞敌人，并按 Actor 去重，多个碰撞体只扣一次血。
        if (!IsValid(Enemy) || !Enemy->IsLaunched() || Enemy->GetCurrentHealth() <= 0.f ||
            Processed.Contains(Enemy)) continue;
        Processed.Add(Enemy);
        UAbilitySystemComponent* TargetASC = Enemy->GetAbilitySystemComponent();
        if (!TargetASC) continue;

        // 【GAS 伤害】保留攻击来源与 HitResult，通过 GE 扣血，遵循已有无敌/属性钳制。
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
        // 【实际命中反馈】只有扣血才刷新悬停；被专属技能击落中的目标不重新悬停。
        if (After > 0.f && !Enemy->IsAirKnockedDown())
        {
            if (UActDemoAirSuspensionComponent* Suspension = Enemy->FindComponentByClass<UActDemoAirSuspensionComponent>())
                Suspension->SuspendFor(EnemySuspensionDuration);
        }
        // 【动画事件】死亡优先，不在 C++ 播放蒙太奇，也不在第四段自动执行击落。
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
