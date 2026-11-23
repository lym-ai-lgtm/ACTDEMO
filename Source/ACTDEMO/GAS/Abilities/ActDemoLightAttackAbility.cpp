// Fill out your copyright notice in the Description page of Project Settings.


#include "ActDemoLightAttackAbility.h"
#include "Combat/ActDemoAttackFacing.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GAS/ActDemoAttributeSet.h"
#include "GAS/ACTDEMOEnemyCharacter.h"
#include "GameFramework/Actor.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"

namespace
{
    // 只暂停当前蒙太奇实例。计时结束时按实例 ID 恢复，避免影响随后开始的连段或翻滚。
    void ApplyLightHitStop(AActor* Actor, float Duration)
    {
        ACharacter* Character = Cast<ACharacter>(Actor);
        if (!IsValid(Character) || !Character->GetMesh() || !Character->GetWorld())
            return;

        UAnimInstance* Anim = Character->GetMesh()->GetAnimInstance();
        FAnimMontageInstance* Montage = Anim ? Anim->GetActiveMontageInstance() : nullptr;
        if (!Montage || FMath::IsNearlyZero(Montage->GetPlayRate()))
            return;

        const int32 InstanceID = Montage->GetInstanceID();
        const float OriginalRate = Montage->GetPlayRate();
        Montage->SetPlayRate(0.f);

        FTimerHandle Timer;
        Character->GetWorld()->GetTimerManager().SetTimer(Timer,
            FTimerDelegate::CreateWeakLambda(Anim, [Anim, InstanceID, OriginalRate]()
            {
                if (FAnimMontageInstance* Instance = Anim->GetMontageInstanceForID(InstanceID))
                {
                    if (FMath::IsNearlyZero(Instance->GetPlayRate()))
                        Instance->SetPlayRate(OriginalRate);
                }
            }), Duration, false);
    }
}

UActDemoLightAttackAbility::UActDemoLightAttackAbility()
{
    InstancingPolicy =
        EGameplayAbilityInstancingPolicy::InstancedPerActor;

    bRetriggerInstancedAbility = false;

    // 当前仍然按单机 Demo 实现。
    NetExecutionPolicy =
        EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

void UActDemoLightAttackAbility::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    if (!ActorInfo ||
        !ActorInfo->AvatarActor.IsValid() ||
        AttackMontages.IsEmpty() ||
        !HitEventTag.IsValid() ||
        !ComboInputEventTag.IsValid() ||
        HitEventTag == ComboInputEventTag ||
        !DamageEffectClass||
        !ComboChainEventTag.IsValid() ||
        ComboChainEventTag == HitEventTag ||
        ComboChainEventTag == ComboInputEventTag )
    {
        UE_LOG(LogTemp, Warning,
            TEXT("Light combo: missing or invalid configuration."));

        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 提前检查数组，避免打到第二段才发现没配置动画。
    for (const TObjectPtr<UAnimMontage>& Montage : AttackMontages)
    {
        if (!IsValid(Montage.Get()))
        {
            UE_LOG(LogTemp, Warning,
                TEXT("Light combo: an AttackMontages entry is empty."));

            EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
            return;
        }
    }

    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    ComboIndex = 0;
    bNextAttackRequested = false;
    bStagePlaying = false;
    bWaitingForNextInput = false;
    bHitProcessedThisStage = false;
    ComboWaitTask = nullptr;
    bCanChain = false;
    ActiveMontageTask = nullptr;

    // 整套连击期间持续接收命中事件。
    UAbilityTask_WaitGameplayEvent* HitTask =
        UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
            this,
            HitEventTag,
            nullptr,
            false,  // 不再只接收第一次。
            true
        );

    HitTask->EventReceived.AddDynamic(
        this,
        &UActDemoLightAttackAbility::OnHitEventReceived);

    HitTask->ReadyForActivation();

    // 整套连击期间持续接收玩家追加的左键点击。
    UAbilityTask_WaitGameplayEvent* InputTask =
        UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
            this,
            ComboInputEventTag,
            nullptr,
            false,
            true
        );

    InputTask->EventReceived.AddDynamic(
        this,
        &UActDemoLightAttackAbility::OnComboInputReceived);

    InputTask->ReadyForActivation();
	//先监听允许衔接下一段的事件，等到播放到允许衔接的阶段时再触发
    UAbilityTask_WaitGameplayEvent* ChainTask =
        UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
            this,
            ComboChainEventTag,
            nullptr,
            false,
            true);

    ChainTask->EventReceived.AddDynamic(
        this,
        &UActDemoLightAttackAbility::OnComboChainReceived);

    ChainTask->ReadyForActivation();

    PlayCurrentStage();
}

void UActDemoLightAttackAbility::PlayCurrentStage()
{
    if (!IsActive())
    {
        return;
    }

    if (!AttackMontages.IsValidIndex(ComboIndex) ||
        !IsValid(AttackMontages[ComboIndex].Get()))
    {
        FinishCombo(true);
        return;
    }

    // 【统一攻击朝向】地面轻攻击每段及复用此类的 R 上挑，在播放前对准锁定目标。
    ActDemoAttackFacing::FaceLockedTarget(GetAvatarActorFromActorInfo());

    // 如果是等待期内续接，取消旧的超时任务。
    if (ComboWaitTask)
    {
        ComboWaitTask->EndTask();
        ComboWaitTask = nullptr;
    }

    bWaitingForNextInput = false;
    bNextAttackRequested = false;
    bHitProcessedThisStage = false;
    bStagePlaying = true;
    bCanChain = false;

    UE_LOG(LogTemp, Log,
        TEXT("Light combo: playing stage %d / %d"),
        ComboIndex + 1,
        AttackMontages.Num());

    UAbilityTask_PlayMontageAndWait* MontageTask =
        UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
            this,
            NAME_None,
            AttackMontages[ComboIndex].Get(),
            AttackPlayRate,
            NAME_None,
            true,
            1.0f,
            0.0f,
            true
        );

    if (!MontageTask)
    {
        FinishCombo(true);
        return;
    }

    MontageTask->OnCompleted.AddDynamic(
        this,
        &UActDemoLightAttackAbility::OnAttackCompleted);

    MontageTask->OnInterrupted.AddDynamic(
        this,
        &UActDemoLightAttackAbility::OnAttackInterrupted);

    MontageTask->OnCancelled.AddDynamic(
        this,
        &UActDemoLightAttackAbility::OnAttackInterrupted);

    ActiveMontageTask = MontageTask;
    MontageTask->ReadyForActivation();

}

void UActDemoLightAttackAbility::OnComboInputReceived(
    FGameplayEventData Payload)
{
    if (!IsActive() || AttackMontages.IsEmpty())
    {
        return;
    }

    // 上一段已经完整结束，目前处于续接等待期。
    if (bWaitingForNextInput)
    {
        AdvanceToNextStage();
        return;
    }

    if (!bStagePlaying)
    {
        return;
    }

    // 每段只缓存一次追加攻击。
    bNextAttackRequested = true;

    if (bCanChain)
    {
        AdvanceToNextStage();
    }
    else
    {
        UE_LOG(LogTemp, Log,
            TEXT("Light combo: next attack buffered."));
    }
}

void UActDemoLightAttackAbility::OnAttackCompleted()
{
    if (!IsActive())
    {
        return;
    }

    ActiveMontageTask = nullptr;
    bStagePlaying = false;
    bCanChain = false;

    // 优先处理追加点击，包括第三段回到第一段。
    if (bNextAttackRequested)
    {
        AdvanceToNextStage();
        return;
    }

    // 最后一段没有追加输入，正常结束连击。
    if (ComboIndex + 1 >= AttackMontages.Num())
    {
        FinishCombo(false);
        return;
    }

    
    // 没有提前输入，开始 0.5 秒续接等待期。
    bWaitingForNextInput = true;

    UE_LOG(LogTemp, Log,
        TEXT("Light combo: waiting %.2f seconds for next input."),
        ComboResetDelay);

    ComboWaitTask = UAbilityTask_WaitDelay::WaitDelay(
        this,
        FMath::Max(ComboResetDelay, 0.01f));

    ComboWaitTask->OnFinish.AddDynamic(
        this,
        &UActDemoLightAttackAbility::OnComboWaitExpired);

    ComboWaitTask->ReadyForActivation();
}

void UActDemoLightAttackAbility::OnComboWaitExpired()
{
    if (!IsActive() || !bWaitingForNextInput)
    {
        return;
    }

    ComboWaitTask = nullptr;

    UE_LOG(LogTemp, Log,
        TEXT("Light combo: input timeout, reset combo."));

    FinishCombo(false);
}

void UActDemoLightAttackAbility::OnAttackInterrupted()
{
    FinishCombo(true);
}

void UActDemoLightAttackAbility::FinishCombo(bool bCancelled)
{
    if (!IsActive())
    {
        return;
    }

    EndAbility(
        CurrentSpecHandle,
        CurrentActorInfo,
        CurrentActivationInfo,
        true,
        bCancelled);
}

void UActDemoLightAttackAbility::EndAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility,
    bool bWasCancelled)
{
    if (ComboWaitTask)
    {
        ComboWaitTask->EndTask();
        ComboWaitTask = nullptr;
    }

    ComboIndex = 0;
    bNextAttackRequested = false;
    bStagePlaying = false;
    bWaitingForNextInput = false;
    bHitProcessedThisStage = false;
    bCanChain = false;

    if (ActiveMontageTask)
    {
        ActiveMontageTask->OnCompleted.Clear();
        ActiveMontageTask->OnInterrupted.Clear();
        ActiveMontageTask->OnCancelled.Clear();
    }

    Super::EndAbility(
        Handle,
        ActorInfo,
        ActivationInfo,
        bReplicateEndAbility,
        bWasCancelled);

    ActiveMontageTask = nullptr;

}

void UActDemoLightAttackAbility::OnHitEventReceived(
    FGameplayEventData Payload)
{
    // 只有当前正在播放的攻击段可以造成伤害。
// 每段最多处理一次命中通知。
    if (!IsActive() || !bStagePlaying || bHitProcessedThisStage)
    {
        return;
    }

    bHitProcessedThisStage = true;

    AActor* Attacker = GetAvatarActorFromActorInfo();

    UAbilitySystemComponent* SourceASC =
        GetAbilitySystemComponentFromActorInfo();

    if (!IsValid(Attacker) || !SourceASC ||
        !Attacker->HasAuthority() || !DamageEffectClass)
    {
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("Light attack: Hit event received."));

    // Character 的 ActorLocation 通常位于胶囊中心，
    // 第一版先以这个高度向前检测。
    const FVector Start = Attacker->GetActorLocation();
    const FVector End =
        Start + Attacker->GetActorForwardVector() * AttackDistance;

    TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
    ObjectTypes.Add(
        UEngineTypes::ConvertToObjectType(ECC_Pawn));

    TArray<AActor*> ActorsToIgnore;
    ActorsToIgnore.Add(Attacker);

    TArray<FHitResult> Hits;

    UKismetSystemLibrary::SphereTraceMultiForObjects(
        Attacker,
        Start,
        End,
        AttackRadius,
        ObjectTypes,
        false,
        ActorsToIgnore,
        EDrawDebugTrace::None,
        Hits,
        true,
        FLinearColor::Red,
        FLinearColor::Green,
        1.5f
    );

    // 同一个角色可能有多个碰撞组件，只允许扣一次血。
    TSet<AActor*> DamagedActors;
    const bool bUseHitStop = !bLaunchEnemyOnHit && HitStopDuration > 0.f;
    bool bAttackerHitStopApplied = false;

    for (const FHitResult& Hit : Hits)
    {
        AActor* TargetActor = Hit.GetActor();

        if (!IsValid(TargetActor) ||
            TargetActor == Attacker ||
            DamagedActors.Contains(TargetActor))
        {
            continue;
        }

        UAbilitySystemComponent* TargetASC =
            UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(
                TargetActor);

        if (!TargetASC ||
            !TargetASC->HasAttributeSetForAttribute(
                UActDemoAttributeSet::GetHealthAttribute()))
        {
            continue;
        }

        DamagedActors.Add(TargetActor);

        FGameplayEffectContextHandle Context =
            SourceASC->MakeEffectContext();

        Context.AddSourceObject(Attacker);
        Context.AddHitResult(Hit);

        FGameplayEffectSpecHandle Spec =
            SourceASC->MakeOutgoingSpec(
                DamageEffectClass,
                GetAbilityLevel(),
                Context);

        if (!Spec.IsValid())
        {
            continue;
        }

        const float HealthBefore = TargetASC->GetNumericAttribute(
            UActDemoAttributeSet::GetHealthAttribute());

        // 尸体不再接受本次轻攻击伤害。
        if (HealthBefore <= 0.0f)
        {
            continue;
        }

        SourceASC->ApplyGameplayEffectSpecToTarget(
            *Spec.Data.Get(),
            TargetASC);

        const float HealthAfter = TargetASC->GetNumericAttribute(
            UActDemoAttributeSet::GetHealthAttribute());

   
        // 只有实际减少生命值，才发送反馈事件。
        if (HealthAfter < HealthBefore)
        {
            // 致死伤害播放死亡，其余伤害播放普通受击。
            FGameplayTag FeedbackEventTag = HealthAfter <= 0.0 ? DeathEventTag : HitReactEventTag;
            if (HealthAfter > 0.f && bLaunchEnemyOnHit && LaunchReactEventTag.IsValid())
            {
                if (AACTDEMOEnemyCharacter* Enemy = Cast<AACTDEMOEnemyCharacter>(TargetActor))
                {
                    if (Enemy->LaunchFromAttack(Attacker, LaunchUpwardSpeed, LaunchForwardSpeed))
                    {
                        FeedbackEventTag = LaunchReactEventTag;
                    }
                }
            }
               
            if (FeedbackEventTag.IsValid())
            {
                FGameplayEventData Payload;
                Payload.EventTag = FeedbackEventTag;
                Payload.Instigator = Attacker;
                Payload.Target = TargetActor;
                Payload.EventMagnitude = HealthBefore - HealthAfter;
                Payload.ContextHandle = Context;

                UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
                    TargetActor,
                    FeedbackEventTag,
                    Payload);
            }

            // 先让受击事件启动蒙太奇，再暂停；一次扫掠命中多人时攻击者只停一次。
            if (bUseHitStop)
            {
                if (!bAttackerHitStopApplied && IsActive())
                {
                    ApplyLightHitStop(Attacker, HitStopDuration);
                    bAttackerHitStopApplied = true;
                }
                ApplyLightHitStop(TargetActor, HitStopDuration);
            }
        }


        UE_LOG(LogTemp, Log,
            TEXT("Light attack hit %s: Health %.1f -> %.1f"),
            *GetNameSafe(TargetActor),
            HealthBefore,
            HealthAfter);
    }
}

void UActDemoLightAttackAbility::AdvanceToNextStage()
{
    if (!IsActive() || AttackMontages.IsEmpty())
    {
        return;
    }

    // 主动切段时，解除上一段任务的回调，
    // 避免旧蒙太奇的中断回调结束整个能力。
    if (ActiveMontageTask)
    {
        ActiveMontageTask->OnCompleted.Clear();
        ActiveMontageTask->OnInterrupted.Clear();
        ActiveMontageTask->OnCancelled.Clear();

        ActiveMontageTask->EndTask();
        ActiveMontageTask = nullptr;
    }

    // 例如三个蒙太奇：0 → 1 → 2 → 0。
    ComboIndex = (ComboIndex + 1) % AttackMontages.Num();

    PlayCurrentStage();
}

void UActDemoLightAttackAbility::OnComboChainReceived(
    FGameplayEventData Payload)
{
    if (!IsActive() || !bStagePlaying)
    {
        return;
    }

    bCanChain = true;

    UE_LOG(LogTemp, Log,
        TEXT("Light combo: chain window opened at stage %d."),
        ComboIndex + 1);

    if (bNextAttackRequested)
    {
        AdvanceToNextStage();
    }
}
