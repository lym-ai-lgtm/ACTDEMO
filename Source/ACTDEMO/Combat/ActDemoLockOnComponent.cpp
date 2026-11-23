// Fill out your copyright notice in the Description page of Project Settings.


#include "ActDemoLockOnComponent.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GAS/ACTDEMOEnemyCharacter.h"
#include "GAS/ACTDEMOGASCharacter.h"

UActDemoLockOnComponent::UActDemoLockOnComponent()
{
    PrimaryComponentTick.bCanEverTick = true;

    // 每 0.1 秒检查一次目标是否仍然有效。
    PrimaryComponentTick.TickInterval = 0.1f;
}

bool UActDemoLockOnComponent::IsOwnerAlive() const
{
    const AActDemoGASCharacter* Character =
        Cast<AActDemoGASCharacter>(GetOwner());

    return IsValid(Character) &&
        Character->GetCurrentHealth() > 0.0f;
}

bool UActDemoLockOnComponent::IsEnemyAlive(AActor* Actor) const
{
    const AACTDEMOEnemyCharacter* Enemy =
        Cast<AACTDEMOEnemyCharacter>(Actor);

    return IsValid(Enemy) &&
        !Enemy->IsActorBeingDestroyed() &&
        Enemy->GetCurrentHealth() > 0.0f;
}

AACTDEMOEnemyCharacter*
UActDemoLockOnComponent::FindBestTarget() const
{
    UWorld* World = GetWorld();
    const AActor* Owner = GetOwner();

    if (!World || !IsValid(Owner) || !IsOwnerAlive())
    {
        return nullptr;
    }

    AACTDEMOEnemyCharacter* BestTarget = nullptr;

    const FVector OwnerLocation = Owner->GetActorLocation();
    const float RadiusSquared = FMath::Square(SearchRadius);

    float BestDistanceSquared = RadiusSquared;

    // 搜索关卡中继承敌人 C++ 基类的角色。
    for (TActorIterator<AACTDEMOEnemyCharacter> It(World); It; ++It)
    {
        AACTDEMOEnemyCharacter* Enemy = *It;

        if (Enemy == Owner || !IsEnemyAlive(Enemy))
        {
            continue;
        }

        const float DistanceSquared = FVector::DistSquared(
            OwnerLocation,
            Enemy->GetActorLocation()
        );

        if (DistanceSquared <= BestDistanceSquared)
        {
            BestDistanceSquared = DistanceSquared;
            BestTarget = Enemy;
        }
    }

    return BestTarget;
}

void UActDemoLockOnComponent::ToggleLock()
{
    if (bHasLock)
    {
        ClearLock();
        return;
    }

    AACTDEMOEnemyCharacter* NewTarget = FindBestTarget();

    if (!IsValid(NewTarget))
    {
        return;
    }

    LockedTarget = NewTarget;
    bHasLock = true;

    OnLockTargetChanged.Broadcast(NewTarget);
}

void UActDemoLockOnComponent::ClearLock()
{
    if (!bHasLock)
    {
        return;
    }

    LockedTarget.Reset();
    bHasLock = false;

    OnLockTargetChanged.Broadcast(nullptr);
}

bool UActDemoLockOnComponent::SwitchTarget(float Direction)
{
    AActor* CurrentTarget = LockedTarget.Get();
    const APawn* OwnerPawn = Cast<APawn>(GetOwner());
    APlayerController* Controller = OwnerPawn
        ? Cast<APlayerController>(OwnerPawn->GetController()) : nullptr;
    if (!IsLockedOn() || !IsOwnerAlive() || !IsEnemyAlive(CurrentTarget) ||
        !Controller || !GetWorld() || !FMath::IsFinite(Direction) ||
        FMath::IsNearlyZero(Direction))
    {
        return false;
    }

    FVector2D CurrentScreenPosition;
    if (!Controller->ProjectWorldLocationToScreen(
        CurrentTarget->GetActorLocation(), CurrentScreenPosition))
    {
        return false;
    }

    int32 ViewWidth = 0;
    int32 ViewHeight = 0;
    Controller->GetViewportSize(ViewWidth, ViewHeight);
    if (ViewWidth <= 0 || ViewHeight <= 0)
    {
        return false;
    }

    FVector ViewLocation;
    FRotator ViewRotation;
    Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
    const FVector ViewForward = ViewRotation.Vector();
    const FVector OwnerLocation = OwnerPawn->GetActorLocation();
    const float RadiusSquared = FMath::Square(SearchRadius);
    const float Side = Direction > 0.0f ? 1.0f : -1.0f;
    double BestHorizontalDistance = TNumericLimits<double>::Max();
    double BestScreenDistanceSquared = TNumericLimits<double>::Max();
    AACTDEMOEnemyCharacter* BestTarget = nullptr;

    for (TActorIterator<AACTDEMOEnemyCharacter> It(GetWorld()); It; ++It)
    {
        AACTDEMOEnemyCharacter* Enemy = *It;
        if (Enemy == CurrentTarget || Enemy == OwnerPawn || !IsEnemyAlive(Enemy))
        {
            continue;
        }

        const FVector Location = Enemy->GetActorLocation();
        if (FVector::DistSquared(OwnerLocation, Location) > RadiusSquared ||
            FVector::DotProduct(Location - ViewLocation, ViewForward) <= 0.0)
        {
            continue;
        }

        FVector2D ScreenPosition;
        if (!Controller->ProjectWorldLocationToScreen(Location, ScreenPosition) ||
            ScreenPosition.X < 0.0 || ScreenPosition.X > ViewWidth ||
            ScreenPosition.Y < 0.0 || ScreenPosition.Y > ViewHeight)
        {
            continue;
        }

        const double HorizontalDistance = (ScreenPosition.X - CurrentScreenPosition.X) * Side;
        if (HorizontalDistance <= 1.0)
        {
            continue;
        }

        const double ScreenDistanceSquared = FVector2D::DistSquared(ScreenPosition, CurrentScreenPosition);
        if (HorizontalDistance < BestHorizontalDistance ||
            (FMath::IsNearlyEqual(HorizontalDistance, BestHorizontalDistance) &&
                ScreenDistanceSquared < BestScreenDistanceSquared))
        {
            BestTarget = Enemy;
            BestHorizontalDistance = HorizontalDistance;
            BestScreenDistanceSquared = ScreenDistanceSquared;
        }
    }

    if (!BestTarget)
    {
        return false;
    }

    LockedTarget = BestTarget;
    OnLockTargetChanged.Broadcast(BestTarget);
    return true;
}

AActor* UActDemoLockOnComponent::GetLockedTarget() const
{
    return LockedTarget.Get();
}

bool UActDemoLockOnComponent::IsLockedOn() const
{
    return bHasLock && LockedTarget.IsValid();
}

void UActDemoLockOnComponent::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(
        DeltaTime,
        TickType,
        ThisTickFunction
    );

    if (!bHasLock)
    {
        return;
    }

    AActor* Owner = GetOwner();
    AActor* Target = LockedTarget.Get();

    if (!IsValid(Owner) ||
        !IsOwnerAlive() ||
        !IsEnemyAlive(Target))
    {
        ClearLock();
        return;
    }

    // 解除距离至少与搜索距离一样大，避免刚锁定就解除。
    const float AllowedDistance =
        FMath::Max(SearchRadius, BreakLockDistance);

    const float DistanceSquared = FVector::DistSquared(
        Owner->GetActorLocation(),
        Target->GetActorLocation()
    );

    if (DistanceSquared > FMath::Square(AllowedDistance))
    {
        ClearLock();
    }
}

