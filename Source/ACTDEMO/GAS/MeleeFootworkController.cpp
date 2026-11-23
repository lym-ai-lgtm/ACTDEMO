#include "GAS/MeleeFootworkController.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GAS/ACTDEMOEnemyCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Navigation/PathFollowingComponent.h"
#include "UObject/SoftObjectPtr.h"

AMeleeFootworkController::AMeleeFootworkController()
{
    PrimaryActorTick.bCanEverTick = true;
}

void AMeleeFootworkController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    SetPhase(EPhase::Approach);
    NextLeapTime = 0.0;
    bCheckedLeapOnApproach = false;
    OrbitSign = FMath::RandBool() ? 1.f : -1.f;
    ConsecutiveAttacks = 0;
    if (ACharacter* ControlledCharacter = Cast<ACharacter>(InPawn))
    {
        TInlineComponentArray<UStaticMeshComponent*> MeshComponents(ControlledCharacter);
        for (UStaticMeshComponent* Mesh : MeshComponents)
        {
            if (Mesh->GetFName() == FName(TEXT("EnemySword")))
            {
                Mesh->SetCollisionProfileName(FName(TEXT("NoCollision")));
                Mesh->SetGenerateOverlapEvents(false);
                Mesh->CanCharacterStepUpOn = ECB_No;
            }
        }
        ControlledCharacter->bUseControllerRotationYaw = false;
        UCharacterMovementComponent* Movement = ControlledCharacter->GetCharacterMovement();
        Movement->bOrientRotationToMovement = false;
        Movement->bUseControllerDesiredRotation = true;
    }
}

void AMeleeFootworkController::OnUnPossess()
{
    ClearFocus(EAIFocusPriority::Gameplay);
    Super::OnUnPossess();
}

void AMeleeFootworkController::SetPhase(EPhase Next)
{
    if (Phase == EPhase::Leap && Next != EPhase::Leap)
    {
        LeapAbilityHandle = FGameplayAbilitySpecHandle();
    }
    if (Next == EPhase::Approach && Phase != Next)
    {
        bCheckedLeapOnApproach = false;
    }
    Phase = Next;
    if (Next == EPhase::Retreat || Next == EPhase::Orbit || Next == EPhase::Approach)
        ConsecutiveAttacks = 0;
    PhaseTime = 0.f;
    UpdateTime = 0.f;
    bRetreatMoveRequested = false;
}

void AMeleeFootworkController::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    auto* Enemy = Cast<AACTDEMOEnemyCharacter>(GetPawn());
    auto* Player = Cast<AActDemoGASCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    UAbilitySystemComponent* ASC = Enemy ? Enemy->GetAbilitySystemComponent() : nullptr;
    static const FGameplayTag Attack = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Combat.Attacking")));
    static const FGameplayTag HitReact = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Combat.HitReact")));
    static const FGameplayTag GroundRecovery = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Combat.GroundRecovery")));
    // 只等待实际的破防反应。起步和收步也可能使用蒙太奇，不能据此停止寻路。
    const UAnimInstance* EnemyAnim = IsValid(Enemy) ? Enemy->GetMesh()->GetAnimInstance() : nullptr;
    static const TSoftObjectPtr<UAnimMontage> PostureBreakMontage{FSoftObjectPath(
        TEXT("/Game/Demo/GAS/Montages/GuardBreak/EnemyReaction/AM_EnemyPostureBreak.AM_EnemyPostureBreak"))};
    const UAnimMontage* ReactionMontage = PostureBreakMontage.Get();
    const bool bPlayingPostureReaction = EnemyAnim && ReactionMontage &&
        EnemyAnim->Montage_IsPlaying(ReactionMontage);
    if (!IsValid(Enemy) || !IsValid(Player) || Enemy->GetCurrentHealth() <= 0.f ||
        Player->GetCurrentHealth() <= 0.f || Enemy->IsLaunched() || bPlayingPostureReaction ||
        (ASC && (ASC->HasMatchingGameplayTag(HitReact) || ASC->HasMatchingGameplayTag(GroundRecovery))))
    {
        StopMovement();
        if (Phase == EPhase::LeapBackstep && Enemy && LeapBackstepMontage.IsValid())
        {
            if (UAnimInstance* Anim = Enemy->GetMesh()->GetAnimInstance())
            {
                Anim->Montage_Stop(0.1f, LeapBackstepMontage.Get());
            }
            LeapBackstepMontage.Reset();
        }
        ClearFocus(EAIFocusPriority::Gameplay);
        SetPhase(EPhase::Approach);
        return;
    }

    SetFocus(Player);
    UCharacterMovementComponent* Movement = Enemy->GetCharacterMovement();
    const float Radius = FMath::Max(OrbitRadius, StrikeDistance + 125.f);
    if (Phase == EPhase::Leap)
    {
        UAnimInstance* Anim = Enemy->GetMesh()->GetAnimInstance();
        UAnimMontage* Montage = ASC ? ASC->GetCurrentMontage() : nullptr;
        if (Anim && Montage && Montage->GetFName() == FName(TEXT("AM_EnemySword_Leap")) &&
            Anim->Montage_IsPlaying(Montage))
        {
            const double Now = GetWorld()->GetTimeSeconds();
            // Jump vertically on frame 55. Capture the target only after the ascent.
            if (!bLeapLaunched && Anim->Montage_GetPosition(Montage) >= 55.f / 30.f)
            {
                bLeapLaunched = true;
                LeapLandingPoint = Enemy->GetActorLocation();
                const float JumpSpeed = FMath::Sqrt(2.f * FMath::Abs(Movement->GetGravityZ()) * LeapHeight);
                Enemy->LaunchCharacter(FVector(0.f, 0.f, JumpSpeed), true, true);
            }
            else if (bLeapLaunched && Movement->IsFalling())
            {
                if (!bLeapTargetLocked && Movement->Velocity.Z <= 0.f)
                {
                    bLeapTargetLocked = true;
                    const FVector Delta = Player->GetActorLocation() - Enemy->GetActorLocation();
                    const FVector Direction = Delta.GetSafeNormal2D();
                    const float LandingDistance = Enemy->GetCapsuleComponent()->GetScaledCapsuleRadius() +
                        Player->GetCapsuleComponent()->GetScaledCapsuleRadius() + LeapLandingGap;
                    // 射程只限制起手。到达最高点后按玩家当前位置计算完整落点。
                    const float Travel = FMath::Max(0.f, Delta.Size2D() - LandingDistance);
                    LeapLandingPoint = Enemy->GetActorLocation() + Direction * Travel;
                    LeapLandingPoint.Z = Player->GetActorLocation().Z
                        - Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()
                        + Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
                    const float DiveTime = FMath::Max(0.1f, LeapDiveTime);
                    LeapFlightEnd = Now + DiveTime;
                    const float Drop = FMath::Max(0.f, Enemy->GetActorLocation().Z - LeapLandingPoint.Z);
                    Movement->Velocity.Z = FMath::Min(-100.f, -Drop / DiveTime - 0.5f * Movement->GetGravityZ() * DiveTime);
                }
                if (bLeapTargetLocked)
                {
                    const float RemainingTime = FMath::Max(
                        FMath::Max(0.001f, DeltaSeconds), static_cast<float>(LeapFlightEnd - Now));
                    const FVector Remaining = LeapLandingPoint - Enemy->GetActorLocation();
                    const FVector Horizontal = FVector(Remaining.X, Remaining.Y, 0.f) / RemainingTime;
                    Movement->Velocity.X = Horizontal.X;
                    Movement->Velocity.Y = Horizontal.Y;
                }
            }
            else if (bLeapTargetLocked)
            {
                Movement->StopMovementImmediately();
            }
            return;
        }
        if (!Movement->IsMovingOnGround())
        {
            return;
        }
        // 跳斩已落地且不再播放。先结束仍在等待回调的跳斩能力，
        // 否则后跳结束后残留的 Attacking 标签会阻止绕行。
        if (ASC && LeapAbilityHandle.IsValid())
        {
            const FGameplayAbilitySpec* LeapSpec = ASC->FindAbilitySpecFromHandle(LeapAbilityHandle);
            if (LeapSpec && LeapSpec->IsActive())
            {
                ASC->CancelAbilityHandle(LeapAbilityHandle);
            }
        }
        if (bLeapLaunched && Anim)
        {
            static const TSoftObjectPtr<UAnimSequence> Backstep{FSoftObjectPath(
                TEXT("/Game/Animations/Sword/Sword_JumpBack_Seq.Sword_JumpBack_Seq"))};
            if (!BackstepInPlace)
            {
                if (UAnimSequence* Source = Backstep.LoadSynchronous())
                {
                    // 同时锁定动画中的根骨，防止网格在胶囊之外偏移后弹回。
                    BackstepInPlace = DuplicateObject<UAnimSequence>(Source, this);
                    BackstepInPlace->bEnableRootMotion = false;
                    BackstepInPlace->bForceRootLock = true;
                    BackstepInPlace->RootMotionRootLock = ERootMotionRootLock::RefPose;
                }
            }
            if (UAnimSequence* Sequence = BackstepInPlace.Get())
            {
                const FVector Offset = Enemy->GetActorLocation() - Player->GetActorLocation();
                const FVector Away = Offset.SizeSquared2D() < UE_KINDA_SMALL_NUMBER
                    ? -Player->GetActorForwardVector().GetSafeNormal2D()
                    : Offset.GetSafeNormal2D();
                if (!Away.IsNearlyZero())
                {
                    const float AirTime = FMath::Max(0.2f, LeapBackstepAirTime);
                    Enemy->SetActorRotation((-Away).Rotation());
                    LeapBackstepMontage = Anim->PlaySlotAnimationAsDynamicMontage(
                        Sequence, FName(TEXT("DefaultSlot")), 0.06f, 0.10f,
                        Sequence->GetPlayLength() / (AirTime + 0.10f));
                    if (LeapBackstepMontage.IsValid())
                    {
                        BackstepMontageInstanceId = INDEX_NONE;
                        // 蒙太奇负责姿势，角色移动组件负责带碰撞的后跳，避免根位移和寻路抢控制。
                        if (FAnimMontageInstance* Instance = Anim->GetActiveInstanceForMontage(LeapBackstepMontage.Get()))
                        {
                            Instance->PushDisableRootMotion();
                            BackstepMontageInstanceId = Instance->GetInstanceID();
                        }
                        StopMovement();
                        Movement->StopMovementImmediately();
                        // 后跳只拉开一小段距离，剩余距离交给落地后的地面寻路。
                        const float Travel = FMath::Min(FMath::Max(0.f, LeapBackstepDistance),
                            FMath::Max(0.f, Radius - Offset.Size2D()));
                        BackstepDirection = Away;
                        BackstepLandingPoint = Enemy->GetActorLocation() + Away * Travel;
                        BackstepSpeed = Travel / AirTime;
                        FVector JumpVelocity = Away * BackstepSpeed;
                        JumpVelocity.Z = -0.5f * Movement->GetGravityZ() * AirTime;
                        SetPhase(EPhase::LeapBackstep);
                        Enemy->LaunchCharacter(JumpVelocity, true, true);
                        return;
                    }
                }
            }
        }
        SetPhase(EPhase::Retreat);
    }
    if (Phase == EPhase::LeapBackstep)
    {
        if (!Movement->IsMovingOnGround())
        {
            if (Movement->IsFalling())
            {
                // 空中保持朝落点的水平速度，避开空中制动造成的落点偏短。
                const float Remaining = FVector::DotProduct(
                    BackstepLandingPoint - Enemy->GetActorLocation(), BackstepDirection);
                const FVector Horizontal = BackstepDirection * FMath::Clamp(
                    Remaining / FMath::Max(0.001f, DeltaSeconds), 0.f, BackstepSpeed);
                Movement->Velocity.X = Horizontal.X;
                Movement->Velocity.Y = Horizontal.Y;
            }
            return;
        }
        Movement->StopMovementImmediately();
        if (UAnimInstance* Anim = Enemy->GetMesh()->GetAnimInstance();
            Anim && BackstepMontageInstanceId != INDEX_NONE &&
            Anim->GetMontageInstanceForID(BackstepMontageInstanceId))
        {
            // 后跳播完之前保持落点，避免地面寻路和后跳姿势重叠。
            return;
        }
        LeapBackstepMontage.Reset();
        StopMovement();
        SetPhase(EPhase::Orbit);
    }
    if (!Enemy->GetCharacterMovement()->IsMovingOnGround())
    {
        StopMovement();
        return;
    }
    if (ASC && ASC->HasMatchingGameplayTag(Attack))
    {
        if (GetMoveStatus() != EPathFollowingStatus::Idle)
        {
            StopMovement();
        }
        return;
    }
    PhaseTime += DeltaSeconds;
    UpdateTime -= DeltaSeconds;
    if (UpdateTime > 0.f)
    {
        return;
    }
    UpdateTime = 0.22f;

    const FVector Center = Player->GetActorLocation();
    const FVector Offset = Enemy->GetActorLocation() - Center;
    const float Distance = Offset.Size2D();
    const FVector Radial = Distance > UE_KINDA_SMALL_NUMBER ? Offset.GetSafeNormal2D() : -Player->GetActorForwardVector().GetSafeNormal2D();
    const float ContactDistance = Enemy->GetCapsuleComponent()->GetScaledCapsuleRadius() +
        Player->GetCapsuleComponent()->GetScaledCapsuleRadius();
    const float Close = FMath::Max(StrikeDistance, ContactDistance + 15.f);

    const bool bBeyondLeapThreshold = Distance > Radius * 1.25f;
    if (bBeyondLeapThreshold && Phase != EPhase::Approach)
    {
        SetPhase(EPhase::Approach);
    }

    switch (Phase)
    {
    case EPhase::Approach:
        // 射程至少覆盖强制跳斩阈值之外的一段距离，避免调大绕行圈后永远无法触发。
        if (ASC && Distance <= FMath::Max(LeapMaxRange, Radius * 1.25f + 75.f) &&
            FMath::Abs(Offset.Z) <= 90.f && GetWorld()->GetTimeSeconds() >= NextLeapTime)
        {
            const bool bCanRollForLeap = !bBeyondLeapThreshold && !bCheckedLeapOnApproach &&
                Distance <= LeapMaxRange && Distance >= Close + 75.f;
            bCheckedLeapOnApproach |= bCanRollForLeap;
            if (bBeyondLeapThreshold || (bCanRollForLeap && FMath::FRand() < LeapChance))
            {
                static const TSoftClassPtr<UGameplayAbility> LeapAbility{FSoftObjectPath(
                    TEXT("/Game/Demo/GAS/Ability/Attack/GA_EnemySword_Leap.GA_EnemySword_Leap_C"))};
                if (UClass* AbilityClass = LeapAbility.Get())
                {
                    if (const auto* Spec = ASC->FindAbilitySpecFromClass(AbilityClass))
                    {
                        const FGameplayAbilitySpecHandle Handle = Spec->Handle;
                        if (ASC->TryActivateAbility(Handle))
                        {
                            StopMovement();
                            Movement->StopMovementImmediately();
                            LeapAbilityHandle = Handle;
                            bLeapLaunched = false;
                            bLeapTargetLocked = false;
                            LeapFlightEnd = 0.0;
                            NextLeapTime = GetWorld()->GetTimeSeconds() + LeapCooldown;
                            SetPhase(EPhase::Leap);
                            break;
                        }
                    }
                }
            }
        }
        if (!bBeyondLeapThreshold && Distance <= Radius + 45.f)
        {
            SetPhase(EPhase::Orbit);
        }
        else if (GetMoveStatus() == EPathFollowingStatus::Idle)
        {
            MoveToActor(Player, Radius, false);
        }
        break;

    case EPhase::Orbit:
    {
        // 留出走到绕行圈的时间，路径受阻时仍允许战斗继续。
        const bool bNearOrbitRadius = Distance >= Radius - 55.f;
        if (PhaseTime >= OrbitSeconds && (bNearOrbitRadius || PhaseTime >= OrbitSeconds + 4.f))
        {
            SetPhase(EPhase::Advance);
            MoveToActor(Player, Close, false);
            break;
        }
        if (PhaseTime > DeltaSeconds && GetMoveStatus() == EPathFollowingStatus::Moving)
        {
            const bool bStalled = PhaseTime - OrbitMoveRequestedAt > 0.65f &&
                FVector::DistSquared2D(Enemy->GetActorLocation(), OrbitMoveStart) < FMath::Square(15.f);
            if (!bStalled &&
                FVector::DistSquared2D(Enemy->GetActorLocation(), OrbitMoveGoal) > FMath::Square(70.f))
            {
                // 有实际进展时更新采样点，以便检测之后才发生的碰撞卡住。
                if (PhaseTime - OrbitMoveRequestedAt > 0.65f)
                {
                    OrbitMoveStart = Enemy->GetActorLocation();
                    OrbitMoveRequestedAt = PhaseTime;
                }
                break;
            }
            if (bStalled)
            {
                StopMovement();
                SetPhase(EPhase::Retreat);
                break;
            }
        }
        // 贴身受击后先留出胶囊通行空间，再逐步接到绕行圈。
        const float Clearance = Enemy->GetCapsuleComponent()->GetScaledCapsuleRadius() +
            Player->GetCapsuleComponent()->GetScaledCapsuleRadius() + 70.f;
        const float StepRadius = FMath::Max(Clearance, Distance + FMath::Clamp(Radius - Distance, -110.f, 110.f));
        const auto RequestOrbitMove = [&](float Direction)
        {
            // 贴身时先沿径向后退，避免切线方向的第一段路径挤入玩家胶囊。
            const FVector StepDirection = Distance < Clearance ? Radial
                : Radial.RotateAngleAxis(32.f * Direction, FVector::UpVector);
            OrbitMoveGoal = Center + StepDirection * StepRadius;
            OrbitMoveRequestedAt = PhaseTime;
            OrbitMoveStart = Enemy->GetActorLocation();
            return MoveToLocation(OrbitMoveGoal, 12.f, false, true, true) ==
                EPathFollowingRequestResult::RequestSuccessful;
        };
        if (!RequestOrbitMove(OrbitSign))
        {
            OrbitSign *= -1.f;
            if (!RequestOrbitMove(OrbitSign)) SetPhase(EPhase::Advance);
        }
        break;
    }

    case EPhase::Advance:
        if (Distance <= Close + 10.f)
        {
            StopMovement();
            SetPhase(EPhase::Hold);
            if (ASC && FMath::Abs(Offset.Z) <= 110.f)
            {
                static const TCHAR* Variants[] = {
                    TEXT("/Game/Demo/GAS/Ability/Attack/GA_EnemySword_A.GA_EnemySword_A_C"),
                    TEXT("/Game/Demo/GAS/Ability/Attack/GA_EnemySword_B.GA_EnemySword_B_C"),
                    TEXT("/Game/Demo/GAS/Ability/Attack/GA_EnemySword_C.GA_EnemySword_C_C")
                };
                TArray<FGameplayAbilitySpecHandle, TInlineAllocator<3>> Choices;
                for (const TCHAR* Path : Variants)
                {
                    const TSoftClassPtr<UGameplayAbility> Ability{FSoftObjectPath(Path)};
                    if (UClass* AbilityClass = Ability.Get())
                    {
                        if (const auto* Spec = ASC->FindAbilitySpecFromClass(AbilityClass))
                        {
                            Choices.Add(Spec->Handle);
                        }
                    }
                }
                if (!Choices.IsEmpty())
                {
                    // 只有成功发动的整套攻击计数，激活失败不消耗连续压制次数。
                    if (ASC->TryActivateAbility(Choices[FMath::RandHelper(Choices.Num())]))
                        ++ConsecutiveAttacks;
                }
            }
        }
        else if (PhaseTime > 3.f || (GetMoveStatus() == EPathFollowingStatus::Idle &&
            MoveToActor(Player, Close, false) == EPathFollowingRequestResult::Failed))
        {
            SetPhase(EPhase::Retreat);
        }
        break;

    case EPhase::Hold:
        if (PhaseTime >= CloseHoldSeconds)
        {
            // Attacking 标签存在时在上方已返回，此处只在上一套真正结束后决定续攻。
            // 玩家已跑远则不无限追打，沿用原来的后退、绕行与远距离跳斩流程。
            const bool bContinuePressure = ConsecutiveAttacks > 0 &&
                ConsecutiveAttacks < FMath::Max(1, MaxConsecutiveAttacks) &&
                Distance <= Close + 120.f && FMath::Abs(Offset.Z) <= 110.f &&
                FMath::FRand() < FMath::Clamp(ContinueAttackChance, 0.f, 1.f);
            SetPhase(bContinuePressure ? EPhase::Advance : EPhase::Retreat);
            if (bContinuePressure && Distance > Close + 10.f)
                MoveToActor(Player, Close, false);
        }
        break;

    case EPhase::Retreat:
        if (Distance >= Radius - 45.f || PhaseTime >= 2.f)
        {
            OrbitSign *= -1.f;
            SetPhase(EPhase::Orbit);
            break;
        }
        // 保留已提交的后退路径。反复 MoveTo 会中止旧请求，使起步/收步反复切换。
        // 到达或失败后交还绕行决策，而不是在同一位置持续重新发起移动。
        if (!bRetreatMoveRequested)
        {
            bRetreatMoveRequested = true;
            if (MoveToLocation(Center + Radial * Radius, 30.f, false, true, true) !=
                EPathFollowingRequestResult::RequestSuccessful)
            {
                SetPhase(EPhase::Orbit);
            }
        }
        else if (GetMoveStatus() == EPathFollowingStatus::Idle)
        {
            SetPhase(EPhase::Orbit);
        }
        break;

    case EPhase::Leap:
    case EPhase::LeapBackstep:
        break;
    }
}
