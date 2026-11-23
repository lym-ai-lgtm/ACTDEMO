#include "Combat/ActDemoAirSuspensionComponent.h"

#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GAS/ACTDEMOGASCharacter.h"
#include "TimerManager.h"

UActDemoAirSuspensionComponent::UActDemoAirSuspensionComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

bool UActDemoAirSuspensionComponent::SuspendFor(float Duration)
{
    ACharacter* Character = Cast<ACharacter>(GetOwner());
    const AActDemoGASCharacter* GASCharacter = Cast<AActDemoGASCharacter>(Character);
    UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
    if (!Character || !Character->HasAuthority() || !GetWorld() || !Movement ||
        !Movement->IsFalling() || !FMath::IsFinite(Duration) || Duration <= 0.f ||
        (GASCharacter && GASCharacter->GetCurrentHealth() <= 0.f)) return false;

    // 后续命中只刷新计时，不能把已经被改成 0 的重力当作原值再保存。
    if (!bSuspended)
    {
        SavedGravityScale = Movement->GravityScale;
        SuspendedMovement = Movement;
        bSuspended = true;
    }
    Movement->StopMovementImmediately();
    Movement->GravityScale = 0.f;
    GetWorld()->GetTimerManager().SetTimer(ReleaseTimer, this,
        &UActDemoAirSuspensionComponent::ReleaseSuspension, FMath::Max(0.01f, Duration), false);
    return true;
}

void UActDemoAirSuspensionComponent::ReleaseSuspension()
{
    if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(ReleaseTimer);
    if (bSuspended)
    {
        if (UCharacterMovementComponent* Movement = SuspendedMovement.Get())
        {
            Movement->GravityScale = SavedGravityScale;
        }
    }
    // 从未切换 Falling/Flying/None；死亡的 DisableMovement 不会被释放逻辑重开。
    bSuspended = false;
    SuspendedMovement.Reset();
}

void UActDemoAirSuspensionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ReleaseSuspension();
    Super::EndPlay(EndPlayReason);
}
