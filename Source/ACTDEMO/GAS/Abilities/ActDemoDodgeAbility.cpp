// Fill out your copyright notice in the Description page of Project Settings.

#include "ActDemoDodgeAbility.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Combat/ActDemoCombatInputBufferComponent.h"
#include "Combat/ActDemoLockOnComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace
{
UAnimMontage* BuildDirectionalMontage(const UBlendSpace& Directions,
    const ACharacter& Character, const UAnimMontage& ReferenceMontage,
    float SideDistanceScale, float& OutYawOffset, float& OutTranslationScale)
{
    FVector Input = Character.GetPendingMovementInputVector();
    if (Input.IsNearlyZero())
    {
        Input = Character.GetLastMovementInputVector();
    }
    Input.Z = 0.0f;
    const FVector Local = Character.GetActorQuat().UnrotateVector(Input);
    const float Angle = Input.IsNearlyZero() ? 180.0f :
        FMath::RadiansToDegrees(FMath::Atan2(Local.Y, Local.X));

    const FBlendSample* Nearest = nullptr;
    const UAnimSequence* ForwardAnimation = nullptr;
    float Distance = TNumericLimits<float>::Max();
    for (const FBlendSample& Sample : Directions.GetBlendSamples())
    {
        if (FMath::IsNearlyZero(Sample.SampleValue.X))
        {
            ForwardAnimation = Sample.Animation.Get();
        }
        const float Delta = FMath::Abs(FMath::FindDeltaAngleDegrees(Angle, Sample.SampleValue.X));
        if (Sample.Animation && Delta < Distance)
        {
            Nearest = &Sample;
            Distance = Delta;
        }
    }
    if (!Nearest || ReferenceMontage.SlotAnimTracks.IsEmpty())
    {
        return nullptr;
    }

    // A reused forward clip needs to face the sample direction before applying root motion.
    const bool bReusesForwardRoll = ForwardAnimation && Directions.GetBlendSamples().ContainsByPredicate(
        [ForwardAnimation](const FBlendSample& Sample)
        {
            return Sample.Animation.Get() == ForwardAnimation && !FMath::IsNearlyZero(Sample.SampleValue.X);
        });
    if (bReusesForwardRoll)
    {
        if (Nearest->Animation.Get() == ForwardAnimation)
        {
            // Preserve the neutral forward roll for the standard set.
            OutYawOffset = Input.IsNearlyZero() ? 0.0f : Nearest->SampleValue.X;
        }
        else if (FMath::IsNearlyEqual(FMath::Abs(Nearest->SampleValue.X), 90.0))
        {
            OutTranslationScale = SideDistanceScale;
        }
    }

    UAnimMontage* Montage = UAnimMontage::CreateSlotAnimationAsDynamicMontage(
        Nearest->Animation.Get(), ReferenceMontage.SlotAnimTracks[0].SlotName, 0.06f, 0.10f);
    if (!Montage || ReferenceMontage.GetPlayLength() <= UE_SMALL_NUMBER)
    {
        return Montage;
    }

    // Keep the existing dodge notify windows at the same fraction of the new animation.
    const float TimeScale = Montage->GetPlayLength() / ReferenceMontage.GetPlayLength();
    for (const FAnimNotifyEvent& Original : ReferenceMontage.Notifies)
    {
        FAnimNotifyEvent& Event = Montage->Notifies.Add_GetRef(Original);
        Event.Link(Montage, Original.GetTime() * TimeScale);
        if (Original.NotifyStateClass)
        {
            Event.EndLink.Link(Montage, Original.EndLink.GetTime() * TimeScale);
            Event.SetDuration(Original.GetDuration() * TimeScale);
        }
    }
    return Montage;
}
}

UActDemoDodgeAbility::UActDemoDodgeAbility()
{
    InstancingPolicy =
        EGameplayAbilityInstancingPolicy::InstancedPerActor;

    // 翻滚未结束时，再次按键不会重新播放本能力。
    bRetriggerInstancedAbility = false;

    // 和目前轻攻击保持一致，先用于单机 Demo。
    NetExecutionPolicy =
        EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

bool UActDemoDodgeAbility::TryActivateDirectionalDodge(UAbilitySystemComponent* AbilitySystem,
    TSubclassOf<UActDemoDodgeAbility> AbilityClass, UBlendSpace* Directions)
{
    if (!AbilitySystem || !AbilityClass || !Directions || !AbilitySystem->AbilityActorInfo.IsValid())
    {
        return false;
    }
    const FGameplayAbilitySpec* Spec = AbilitySystem->FindAbilitySpecFromClass(AbilityClass);
    if (AActor* Avatar = AbilitySystem->GetAvatarActor())
    {
        if (UActDemoCombatInputBufferComponent* Buffer =
            Avatar->FindComponentByClass<UActDemoCombatInputBufferComponent>())
        {
            Buffer->ClearBufferedInput();
        }
    }
    if (!Spec || Spec->IsActive())
    {
        return false;
    }

    FGameplayEventData Payload;
    Payload.OptionalObject = Directions;
    return AbilitySystem->TriggerAbilityFromGameplayEvent(Spec->Handle,
        AbilitySystem->AbilityActorInfo.Get(), FGameplayTag(), &Payload, *AbilitySystem);
}

bool UActDemoDodgeAbility::CanActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayTagContainer* SourceTags,
    const FGameplayTagContainer* TargetTags,
    FGameplayTagContainer* OptionalRelevantTags) const
{
    if (!Super::CanActivateAbility(
        Handle,
        ActorInfo,
        SourceTags,
        TargetTags,
        OptionalRelevantTags))
    {
        return false;
    }

    const ACharacter* Character = ActorInfo
        ? Cast<ACharacter>(ActorInfo->AvatarActor.Get())
        : nullptr;

    if (!Character || !IsValid(DodgeMontage.Get()))
    {
        return false;
    }

    const UCharacterMovementComponent* Movement =
        Character->GetCharacterMovement();

    return Movement && Movement->IsMovingOnGround();
}


void UActDemoDodgeAbility::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    ACharacter* Character = ActorInfo
        ? Cast<ACharacter>(ActorInfo->AvatarActor.Get())
        : nullptr;

    if (!Character || !IsValid(DodgeMontage.Get()))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    UCharacterMovementComponent* Movement =
        Character->GetCharacterMovement();

    // 第一版只允许在地面翻滚。
    if (!Movement || !Movement->IsMovingOnGround())
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    const UBlendSpace* Directions = TriggerEventData
        ? Cast<UBlendSpace>(TriggerEventData->OptionalObject.Get()) : nullptr;
    float YawOffset = 0.0f;
    float TranslationScale = 1.0f;
    UAnimMontage* Montage = Directions
        ? BuildDirectionalMontage(*Directions, *Character, *DodgeMontage,
            ReusedForwardSetSideDistanceScale, YawOffset, TranslationScale)
        : DodgeMontage.Get();
    if (!Montage || !CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    UAbilityTask_PlayMontageAndWait* MontageTask =
        UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
            this,
            NAME_None,
            Montage,
            DodgePlayRate,
            NAME_None,
            true,   // 能力结束时停止它播放的蒙太奇。
            TranslationScale,
            0.0f,
            true    // 淡出期间被打断，也接收中断回调。
        );

    if (!MontageTask)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    MontageTask->OnCompleted.AddDynamic(
        this,
        &UActDemoDodgeAbility::OnDodgeCompleted);

    MontageTask->OnInterrupted.AddDynamic(
        this,
        &UActDemoDodgeAbility::OnDodgeInterrupted);

    MontageTask->OnCancelled.AddDynamic(
        this,
        &UActDemoDodgeAbility::OnDodgeInterrupted);

    // 只改变锁定翻滚；普通翻滚保留原行为。不旋转 Controller，避免带动镜头。
    const UActDemoLockOnComponent* LockOn =
        Character->FindComponentByClass<UActDemoLockOnComponent>();
    if (!FMath::IsNearlyZero(YawOffset))
    {
        Character->SetActorRotation(FRotator(0.0f, Character->GetActorRotation().Yaw + YawOffset, 0.0f));
    }
    else if (!Directions && bDirectionalDodgeWhenLocked && LockOn && LockOn->IsLockedOn())
    {
        FVector DodgeDirection = Character->GetLastMovementInputVector();
        DodgeDirection.Z = 0.0f;
        if (DodgeDirection.IsNearlyZero())
        {
            DodgeDirection = Character->GetActorForwardVector() *
                (bDodgeBackwardWithoutInput ? -1.0f : 1.0f);
            DodgeDirection.Z = 0.0f;
        }

        if (!DodgeDirection.IsNearlyZero())
        {
            Character->SetActorRotation(FRotator(
                0.0f, DodgeDirection.Rotation().Yaw, 0.0f));
        }
    }

    // 回调全部绑定、翻滚朝向确定后，再启动播放任务。
    MontageTask->ReadyForActivation();
}

void UActDemoDodgeAbility::OnDodgeCompleted()
{
    FinishDodge(false);
}

void UActDemoDodgeAbility::OnDodgeInterrupted()
{
    FinishDodge(true);
}

void UActDemoDodgeAbility::FinishDodge(bool bCancelled)
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
