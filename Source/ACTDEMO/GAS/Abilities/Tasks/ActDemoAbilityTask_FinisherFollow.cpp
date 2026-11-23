#include "GAS/Abilities/Tasks/ActDemoAbilityTask_FinisherFollow.h"
#include "Abilities/GameplayAbility.h"

#include "Combat/ActDemoAirSuspensionComponent.h"
#include "GAS/ACTDEMOEnemyCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"

UActDemoAbilityTask_FinisherFollow::UActDemoAbilityTask_FinisherFollow()
{
    bTickingTask = true;
}

UActDemoAbilityTask_FinisherFollow* UActDemoAbilityTask_FinisherFollow::FollowHitTarget(
    UGameplayAbility* Ability, AACTDEMOEnemyCharacter* Enemy, float Distance,
    float CorrectionSpeed, float BreakDistance)
{
    auto* Task = NewAbilityTask<UActDemoAbilityTask_FinisherFollow>(Ability);
    Task->Target = Enemy;
    Task->DesiredDistance = Distance;
    Task->MaxCorrectionSpeed = CorrectionSpeed;
    Task->MaxSeparation = BreakDistance;
    if (const AActor* Player = Task->GetAvatarActor())
    {
        // 只记录首次中刀时的空间关系，不追踪动画骨骼或相机朝向。
        Task->FollowDirection = Player->GetActorForwardVector().GetSafeNormal2D();
        Task->FollowHeightOffset = FMath::Clamp(
            Enemy->GetActorLocation().Z - Player->GetActorLocation().Z, -60.f, 60.f);
    }
    return Task;
}

void UActDemoAbilityTask_FinisherFollow::TickTask(float DeltaTime)
{
    Super::TickTask(DeltaTime);
    ACharacter* Player = Cast<ACharacter>(GetAvatarActor());
    AACTDEMOEnemyCharacter* Enemy = Target.Get();
    const auto* Hold = Enemy ? Enemy->FindComponentByClass<UActDemoAirSuspensionComponent>() : nullptr;
    // 悬停到期、末段上挑、死亡或脱离刀程立即断开，不把落地目标拉回空中。
    if (!Player || !Enemy || Enemy->GetCurrentHealth() <= 0.f || !Hold || !Hold->IsSuspended() ||
        !Enemy->GetCharacterMovement()->IsFalling() ||
        FVector::Dist(Player->GetActorLocation(), Enemy->GetActorLocation()) > MaxSeparation)
    {
        EndTask();
        return;
    }

    if (!FollowSource.IsValid())
    {
        TargetMovement = Enemy->GetCharacterMovement();
        FollowSource = MakeShared<FRootMotionSource_ConstantForce>();
        FollowSource->InstanceName = FName(TEXT("FinisherHitFollow"));
        FollowSource->Priority = 1000;
        FollowSource->AccumulateMode = ERootMotionAccumulateMode::Override;
        FollowSource->Duration = -1.f;
        FollowSource->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::SetVelocity;
        FollowSource->FinishVelocityParams.SetVelocity = FVector::ZeroVector;
        FollowSourceId = TargetMovement->ApplyRootMotionSource(FollowSource);
    }

    // 受击动画胸骨会来回摆动，不能拿它反向驱动整个胶囊，否则每刀都会抽动。
    const FVector DesiredLocation = Player->GetActorLocation() + FollowDirection * DesiredDistance +
        FVector(0.f, 0.f, FollowHeightOffset);
    // 跟随玩家实际根运动速度，附加有限速度的位置纠偏；不瞬移、不穿透碰撞。
    const float Response = DeltaTime > SMALL_NUMBER ? (1.f - FMath::Exp(-6.f * DeltaTime)) / DeltaTime : 0.f;
    const FVector Correction = ((DesiredLocation - Enemy->GetActorLocation()) * Response)
        .GetClampedToMaxSize(MaxCorrectionSpeed);
    FollowSource->Force = Player->GetVelocity() + Correction;
}

void UActDemoAbilityTask_FinisherFollow::OnDestroy(bool bAbilityEnded)
{
    if (auto* Movement = TargetMovement.Get())
        if (FollowSourceId != 0)
            Movement->RemoveRootMotionSourceByID(FollowSourceId);
    FollowSource.Reset();
    Super::OnDestroy(bAbilityEnded);
}
