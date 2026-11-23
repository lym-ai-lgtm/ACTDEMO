#include "GAS/Abilities/ActDemoAirSlamAbility.h"
#include "Combat/ActDemoAttackFacing.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GAS/ACTDEMOEnemyCharacter.h"
#include "GAS/ACTDEMOGASCharacter.h"
#include "GAS/ActDemoAttributeSet.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraShakeBase.h"
#include "NiagaraFunctionLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Combat/ActDemoAirSuspensionComponent.h"

// 【实例策略】沿用单机 Demo 的服务器执行方式，F 键发动单次下砸，不缓存连击、不重复激活。
UActDemoAirSlamAbility::UActDemoAirSlamAbility()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
    bRetriggerInstancedAbility = false;
}

// 【激活校验】先遵循 GAS 标签互斥，再检查角色存活、Falling 和独立伤害效果配置。
bool UActDemoAirSlamAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
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

// 【能力入口】单次下砸提交一次成本；动画交给蓝图 Event ActivateAbility，C++ 不播放蒙太奇。
void UActDemoAirSlamAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    ClearLandingFeedback();
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }
    if (!PrepareAirSlam())
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }
    // 调用基类分发到蓝图 Event ActivateAbility；这里不播放任何蒙太奇。
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

// 【发动初始化】只允许一次命中，并维持玩家出招悬停；未命中不改变敌人。
bool UActDemoAirSlamAbility::PrepareAirSlam()
{
    AActDemoGASCharacter* Attacker = Cast<AActDemoGASCharacter>(GetAvatarActorFromActorInfo());
    if (!IsActive() || !IsValid(Attacker) || !Attacker->HasAuthority() ||
        Attacker->GetCurrentHealth() <= 0.f || !Attacker->GetCharacterMovement()->IsFalling())
        return false;

    // 【统一攻击朝向】F 下砸起手朝锁定目标；实际下冲仍由原命中通知触发。
    ActDemoAttackFacing::FaceLockedTarget(Attacker);
    bHitProcessed = false;
    bDescentStarted = false;
    LandingCharacter = Attacker;
    Attacker->LandedDelegate.AddUniqueDynamic(this, &UActDemoAirSlamAbility::OnAttackLanded);
    UActDemoAirSuspensionComponent* Suspension = Attacker->FindComponentByClass<UActDemoAirSuspensionComponent>();
    if (Suspension && Suspension->SuspendFor(AttackerSuspensionSafetyDuration))
        AttackerSuspension = Suspension;
    return true;
}

// 【开始下冲】命中通知帧解除自身悬停，使用 CharacterMovement 碰撞下落，不附着或瞬移到敌人。
void UActDemoAirSlamAbility::StartDownwardFollow(ACharacter* Character)
{
    const AActDemoGASCharacter* GASCharacter = Cast<AActDemoGASCharacter>(Character);
    if (!bFollowDownward || bDescentStarted || !IsActive() || !IsValid(Character) ||
        !Character->HasAuthority() || !GASCharacter || GASCharacter->GetCurrentHealth() <= 0.f ||
        !Character->GetCharacterMovement()->IsFalling() ||
        !FMath::IsFinite(DownwardSpeed) || DownwardSpeed <= 0.f) return;

    bDescentStarted = true;
    bLandingFeedbackPending = true;
    if (UActDemoAirSuspensionComponent* Suspension = AttackerSuspension.Get())
        Suspension->ReleaseSuspension();
    AttackerSuspension.Reset();

    Character->GetCharacterMovement()->StopMovementImmediately();
    Character->LaunchCharacter(FVector(0.f, 0.f, -DownwardSpeed), true, true);
}

// 【真实落地】结束空中技能；蓝图原有 StopWhenAbilityEnds 负责退出下砸蒙太奇。
void UActDemoAirSlamAbility::OnAttackLanded(const FHitResult& Hit)
{
    const bool bPlayFeedback = bLandingFeedbackPending;
    AActDemoGASCharacter* Character = Cast<AActDemoGASCharacter>(LandingCharacter.Get());
    ClearLandingFeedback();

    if (bPlayFeedback && IsValid(Character) && Character->GetCurrentHealth() > 0.f)
    {
        if (LandingDust)
        {
            UNiagaraFunctionLibrary::SpawnSystemAtLocation(Character, LandingDust,
                Hit.ImpactPoint + Hit.ImpactNormal * 2.f,
                FRotationMatrix::MakeFromZ(Hit.ImpactNormal).Rotator());
        }

        APlayerController* Controller = Cast<APlayerController>(Character->GetController());
        if (Controller && LandingShake && LandingShakeScale > 0.f)
            Controller->ClientStartCameraShake(LandingShake, LandingShakeScale);
    }

    if (IsActive())
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UActDemoAirSlamAbility::ClearLandingFeedback()
{
    if (ACharacter* Character = LandingCharacter.Get())
        Character->LandedDelegate.RemoveDynamic(this, &UActDemoAirSlamAbility::OnAttackLanded);
    LandingCharacter.Reset();
    bLandingFeedbackPending = false;
}

void UActDemoAirSlamAbility::OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilitySpec& Spec)
{
    ClearLandingFeedback();
    Super::OnRemoveAbility(ActorInfo, Spec);
}

// 【统一清理】正常结束、受击或死亡取消都解绑落地监听、释放悬停，不修改死亡移动模式。
void UActDemoAirSlamAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility, bool bWasCancelled)
{
    // 蒙太奇可能先播完。正常结束时保留已开始下冲的反馈，等实际触地再消费。
    if (bWasCancelled || !bLandingFeedbackPending)
        ClearLandingFeedback();
    bDescentStarted = false;
    if (UActDemoAirSuspensionComponent* Suspension = AttackerSuspension.Get())
        Suspension->ReleaseSuspension();
    AttackerSuspension.Reset();
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// 【命中入口】仅由独立下砸命中通知调用；最多检测一次，避免重复通知重复扣血。
int32 UActDemoAirSlamAbility::ProcessAirSlamHit()
{
    AActDemoGASCharacter* Attacker = Cast<AActDemoGASCharacter>(GetAvatarActorFromActorInfo());
    UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
    if (!IsActive() || bHitProcessed || !IsValid(Attacker) || !Attacker->HasAuthority() ||
        Attacker->GetCurrentHealth() <= 0.f || !Attacker->GetCharacterMovement()->IsFalling() ||
        !SourceASC || !DamageEffectClass) return 0;

    bHitProcessed = true;
    // 【空间检测】沿角色前方球形扫掠 Pawn，距离/半径在下砸蓝图中独立调节。
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
        // 【击落】只有实际扣血且敌人存活才释放悬停并向下运动；不提前触发落地。
        if (After > 0.f) Enemy->KnockDownFromAir(DownwardSpeed);
        // 【动画事件】死亡优先；存活敌人保留原空中受击，真实落地后复用倒地/起身。
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
    // 【通知驱动移动】在同一个接触帧开始下冲；挥空也会下落，但不会凭空击落敌人。
    StartDownwardFollow(Attacker);
    return DamagedCount;
}
