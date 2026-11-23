#include "GAS/Abilities/ActDemoFinisherAbility.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "GAS/Abilities/Tasks/ActDemoAbilityTask_FinisherFollow.h"
#include "Combat/ActDemoAirSuspensionComponent.h"
#include "Combat/ActDemoAttackFacing.h"
#include "Combat/ActDemoLockCameraBoom.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GAS/ACTDEMOEnemyCharacter.h"
#include "GameFramework/Controller.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffect.h"

namespace
{
    FGameplayTag CombatTag(const TCHAR* Name)
    {
        return FGameplayTag::RequestGameplayTag(FName(Name));
    }
}

UActDemoFinisherAbility::UActDemoFinisherAbility()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

bool UActDemoFinisherAbility::CanActivateAbility(FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
    const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
    if (!DamageEffectClass || !Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags,
        OptionalRelevantTags))
        return false;

    const AActDemoGASCharacter* Player = ActorInfo
        ? Cast<AActDemoGASCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
    const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
    if (!ASC || !IsValid(Player) || Player->GetCurrentHealth() <= 0.f ||
        !Player->GetCharacterMovement()->IsFalling() || !Player->HasFullMomentum())
        return false;

    // 允许空中轻击和重击转入 Q，其余攻击仍保持互斥。
    const int32 AttackCount = ASC->GetTagCount(CombatTag(TEXT("State.Combat.Attacking")));
    return AttackCount == 0 || (AttackCount == 1 &&
        (ASC->HasMatchingGameplayTag(CombatTag(TEXT("State.Combat.AirAttacking"))) ||
         ASC->HasMatchingGameplayTag(CombatTag(TEXT("State.Combat.AirHeavyAttacking")))));
}

void UActDemoFinisherAbility::ActivateAbility(FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    AActDemoGASCharacter* Player = Cast<AActDemoGASCharacter>(GetAvatarActorFromActorInfo());
    UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
    UActDemoAirSuspensionComponent* PlayerHold = Player
        ? Player->FindComponentByClass<UActDemoAirSuspensionComponent>() : nullptr;
    if (!PlayerHold || !ASC || !Player->HasAuthority() || Player->GetCurrentHealth() <= 0.f ||
        !Player->GetCharacterMovement()->IsFalling() || !Player->HasFullMomentum() ||
        !DamageEffectClass || !CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // Q 接管无重力期间的推进。未命中敌人时，不刷新敌人的悬停计时。

    FGameplayTagContainer CancelTags;
    CancelTags.AddTag(CombatTag(TEXT("Ability.Combat.AirAttack")));
    CancelTags.AddTag(CombatTag(TEXT("Ability.Combat.AirHeavyAttack")));
    ASC->CancelAbilities(&CancelTags, nullptr, this);
    if (!IsActive())
        return;

    if (PlayerHold->SuspendFor(SuspensionSafetyDuration))
        PlayerSuspension = PlayerHold;
    if (!PlayerSuspension.IsValid() || !Player->SpendFullMomentum())
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    ActDemoAttackFacing::FaceLockedTarget(Player);
    // 输入锁仅防止 WASD 打偏演出，位移由蒙太奇原始根运动驱动。
    if (AController* Controller = Player->GetController())
    {
        Controller->SetIgnoreMoveInput(true);
        MovementInputController = Controller;
    }
    LandingCharacter = Player;
    Player->LandedDelegate.AddUniqueDynamic(this, &UActDemoFinisherAbility::OnAttackerLanded);

    // 镜头表现交给相机组件；不改变玩家朝向、锁定状态或动画根运动。
    OrbitCameraBoom = Player->FindComponentByClass<UActDemoLockCameraBoom>();
    if (UActDemoLockCameraBoom* Boom = OrbitCameraBoom.Get())
    {
        Boom->BeginQSkillOrbit();
    }

    // 独立末段通知只触发一次，挥空也不会把前面的普通命中当成上挑。
    UAbilityTask_WaitGameplayEvent* FinalHitTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
        this, CombatTag(TEXT("Event.Combat.Finisher.LaunchHit")), nullptr, true, true);
    FinalHitTask->EventReceived.AddDynamic(this, &UActDemoFinisherAbility::OnFinalUppercutEvent);
    FinalHitTask->ReadyForActivation();
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UActDemoFinisherAbility::ExecuteFinisherHit()
{
    ProcessFinisherHit(false);
}

void UActDemoFinisherAbility::OnFinalUppercutEvent(FGameplayEventData Payload)
{
    // 按末段通知回正，挥空或本刀致死也不能把镜头留在侧面。
    if (UActDemoLockCameraBoom* Boom = OrbitCameraBoom.Get())
        Boom->EndQSkillOrbit();
    ProcessFinisherHit(true);
}

void UActDemoFinisherAbility::ProcessFinisherHit(bool bFinalUppercut)
{
    AActor* Attacker = GetAvatarActorFromActorInfo();
    UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
    if (!IsActive() || !IsValid(Attacker) || !Attacker->HasAuthority() ||
        !SourceASC || !DamageEffectClass)
        return;

    // 每刀按当前的位置和朝向判定，前一刀挥空不影响后续通知。
    const FVector Start = Attacker->GetActorLocation();
    const FVector End = Start + Attacker->GetActorForwardVector() * MaxRange;
    const TArray<TEnumAsByte<EObjectTypeQuery>> Types = {UEngineTypes::ConvertToObjectType(ECC_Pawn)};
    const TArray<AActor*> Ignore = {Attacker};
    TArray<FHitResult> Hits;
    UKismetSystemLibrary::SphereTraceMultiForObjects(Attacker, Start, End, HitRadius,
        Types, false, Ignore, EDrawDebugTrace::None, Hits, true);

    const FHitResult* TargetHit = Hits.FindByPredicate([this, Attacker](const FHitResult& Hit)
    {
        const AACTDEMOEnemyCharacter* Enemy = Cast<AACTDEMOEnemyCharacter>(Hit.GetActor());
        return IsValid(Enemy) && Enemy->GetCurrentHealth() > 0.f &&
            FVector::Dist(Attacker->GetActorLocation(), Enemy->GetActorLocation()) <= MaxContactDistance &&
            Enemy->IsLaunched() && !Enemy->IsAirKnockedDown() &&
            Enemy->GetCharacterMovement()->IsFalling() && Enemy->GetAbilitySystemComponent();
    });
    if (!TargetHit)
        return;

    AACTDEMOEnemyCharacter* Enemy = CastChecked<AACTDEMOEnemyCharacter>(TargetHit->GetActor());
    UAbilitySystemComponent* TargetASC = Enemy->GetAbilitySystemComponent();

    FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
    Context.AddSourceObject(Attacker);
    Context.AddHitResult(*TargetHit);
    const FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(DamageEffectClass, GetAbilityLevel(), Context);
    if (!Spec.IsValid())
        return;
    const float Before = Enemy->GetCurrentHealth();
    SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
    const float After = Enemy->GetCurrentHealth();
    if (After >= Before)
        return;

    if (After <= 0.f && IsValid(TargetFollowTask))
    {
        TargetFollowTask->EndTask();
        TargetFollowTask = nullptr;
    }

    const bool bRelaunch = After > 0.f && bFinalUppercut &&
        FMath::IsFinite(FinalLaunchUpwardSpeed) && FinalLaunchUpwardSpeed > 0.f;
    if (bRelaunch)
    {
        if (IsValid(TargetFollowTask)) TargetFollowTask->EndTask();
        TargetFollowTask = nullptr;
        // 必须先恢复重力并清除旧悬停计时器，才能重新获得自然的上升与下落。
        if (UActDemoAirSuspensionComponent* Suspension = Enemy->FindComponentByClass<UActDemoAirSuspensionComponent>())
            Suspension->ReleaseSuspension();
        const float ForwardSpeed = FMath::IsFinite(FinalLaunchForwardSpeed)
            ? FMath::Max(0.f, FinalLaunchForwardSpeed) : 0.f;
        const FVector Velocity = Attacker->GetActorForwardVector().GetSafeNormal2D() * ForwardSpeed +
            FVector(0.f, 0.f, FinalLaunchUpwardSpeed);
        // 目标已处于击飞状态，保留现有 Airborne、落地与起身交接，仅替换速度。
        Enemy->LaunchCharacter(Velocity, true, true);
    }
    else if (After > 0.f && EnemyHitSuspensionDuration > 0.f)
    {
        if (UActDemoAirSuspensionComponent* Suspension = Enemy->FindComponentByClass<UActDemoAirSuspensionComponent>())
        {
            // 每刀刷新悬停；只对实际中刀者维持刀程，不改变原本的挥空判定。
            if (Suspension->SuspendFor(EnemyHitSuspensionDuration) &&
                (!IsValid(TargetFollowTask) || TargetFollowTask->GetState() == EGameplayTaskState::Finished))
            {
                TargetFollowTask = UActDemoAbilityTask_FinisherFollow::FollowHitTarget(this, Enemy,
                    EnemyFollowDistance, EnemyFollowCorrectionSpeed, EnemyFollowBreakDistance);
                TargetFollowTask->ReadyForActivation();
            }
        }
    }

    FGameplayEventData Event;
    Event.EventTag = CombatTag(TEXT("Event.Combat.Air.HitReact"));
    if (After <= 0.f)
        Event.EventTag = CombatTag(TEXT("Event.Combat.Death"));
    else if (bRelaunch)
        Event.EventTag = CombatTag(TEXT("Event.Combat.Launch.HitReact"));
    Event.Instigator = Attacker;
    Event.Target = Enemy;
    Event.EventMagnitude = Before - After;
    Event.ContextHandle = Context;
    UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Enemy, Event.EventTag, Event);
}

void UActDemoFinisherAbility::OnAttackerLanded(const FHitResult& Hit)
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UActDemoFinisherAbility::EndAbility(FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility, bool bWasCancelled)
{
    if (!IsEndAbilityValid(Handle, ActorInfo))
        return;

    // 中断、落地与提前结束同样回正，避免遗留偏转。
    if (UActDemoLockCameraBoom* Boom = OrbitCameraBoom.Get())
        Boom->EndQSkillOrbit();
    OrbitCameraBoom.Reset();

    if (IsValid(TargetFollowTask)) TargetFollowTask->EndTask();
    TargetFollowTask = nullptr;

    if (ACharacter* Player = LandingCharacter.Get())
        Player->LandedDelegate.RemoveDynamic(this, &UActDemoFinisherAbility::OnAttackerLanded);
    if (AController* Controller = MovementInputController.Get())
        Controller->SetIgnoreMoveInput(false);
    MovementInputController.Reset();
    if (UActDemoAirSuspensionComponent* Hold = PlayerSuspension.Get())
        Hold->ReleaseSuspension();
    LandingCharacter.Reset();
    PlayerSuspension.Reset();
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
