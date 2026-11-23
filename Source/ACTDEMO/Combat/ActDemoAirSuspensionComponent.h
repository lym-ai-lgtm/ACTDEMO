#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ActDemoAirSuspensionComponent.generated.h"

class UCharacterMovementComponent;

/** 可刷新时限的空中暂停。仅管理速度/重力，不播放动画、不接管死亡移动模式。 */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class ACTDEMO_API UActDemoAirSuspensionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UActDemoAirSuspensionComponent();

    UFUNCTION(BlueprintCallable, Category="Combat|Air Suspension")
    bool SuspendFor(float Duration = 1.f);

    UFUNCTION(BlueprintCallable, Category="Combat|Air Suspension")
    void ReleaseSuspension();

    UFUNCTION(BlueprintPure, Category="Combat|Air Suspension")
    bool IsSuspended() const { return bSuspended; }

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    TWeakObjectPtr<UCharacterMovementComponent> SuspendedMovement;
    FTimerHandle ReleaseTimer;
    float SavedGravityScale = 1.f;
    bool bSuspended = false;
};
