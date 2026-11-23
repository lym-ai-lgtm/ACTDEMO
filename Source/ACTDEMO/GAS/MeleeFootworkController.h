#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpecHandle.h"
#include "AIController.h"
#include "MeleeFootworkController.generated.h"

UCLASS(Blueprintable)
class ACTDEMO_API AMeleeFootworkController : public AAIController
{
    GENERATED_BODY()

public:
    AMeleeFootworkController();
    virtual void Tick(float DeltaSeconds) override;

protected:
    virtual void OnPossess(APawn* InPawn) override;
    virtual void OnUnPossess() override;

    UPROPERTY(EditDefaultsOnly, Category="Footwork", meta=(ClampMin="100", Units="cm"))
    float OrbitRadius = 650.f;

    UPROPERTY(EditDefaultsOnly, Category="Footwork", meta=(ClampMin="50", Units="cm"))
    float StrikeDistance = 145.f;

    UPROPERTY(EditDefaultsOnly, Category="Footwork", meta=(ClampMin="0.1", Units="s"))
    float OrbitSeconds = 0.75f;

    UPROPERTY(EditDefaultsOnly, Category="Footwork", meta=(ClampMin="0.1", Units="s"))
    float CloseHoldSeconds = 0.25f;

    // 每套普攻结束后决定是否继续压制，达到上限后仍会退回绕行。
    UPROPERTY(EditDefaultsOnly, Category="Footwork|Attack Pressure", meta=(ClampMin="0", ClampMax="1"))
    float ContinueAttackChance = 0.75f;

    UPROPERTY(EditDefaultsOnly, Category="Footwork|Attack Pressure", meta=(ClampMin="1", ClampMax="10"))
    int32 MaxConsecutiveAttacks = 3;

    UPROPERTY(EditDefaultsOnly, Category="Footwork|Leap", meta=(ClampMin="0", ClampMax="1"))
    float LeapChance = 0.35f;

    UPROPERTY(EditDefaultsOnly, Category="Footwork|Leap", meta=(ClampMin="0", Units="s"))
    float LeapCooldown = 8.f;

    UPROPERTY(EditDefaultsOnly, Category="Footwork|Leap", meta=(ClampMin="100", Units="cm"))
    float LeapMaxRange = 1200.f;

    UPROPERTY(EditDefaultsOnly, Category="Footwork|Leap", meta=(ClampMin="0", Units="cm"))
    float LeapLandingGap = 12.f;

    UPROPERTY(EditDefaultsOnly, Category="Footwork|Leap", meta=(ClampMin="80", Units="cm"))
    float LeapHeight = 155.f;

    UPROPERTY(EditDefaultsOnly, Category="Footwork|Leap", meta=(ClampMin="0.1", Units="s"))
    float LeapDiveTime = 0.26f;

    UPROPERTY(EditDefaultsOnly, Category="Footwork|Leap", meta=(ClampMin="0.2", Units="s"))
    float LeapBackstepAirTime = 0.55f;

    UPROPERTY(EditDefaultsOnly, Category="Footwork|Leap", meta=(ClampMin="0", Units="cm"))
    float LeapBackstepDistance = 300.f;

private:
    enum class EPhase : uint8 { Approach, Orbit, Advance, Hold, Retreat, Leap, LeapBackstep };
    void SetPhase(EPhase Next);

    EPhase Phase = EPhase::Approach;
    float PhaseTime = 0.f;
    float UpdateTime = 0.f;
    float OrbitSign = 1.f;
    int32 ConsecutiveAttacks = 0;
    double NextLeapTime = 0.0;
    bool bLeapLaunched = false;
    bool bLeapTargetLocked = false;
    bool bCheckedLeapOnApproach = false;
    FVector LeapLandingPoint = FVector::ZeroVector;
    double LeapFlightEnd = 0.0;
    FVector OrbitMoveGoal = FVector::ZeroVector;
    FVector OrbitMoveStart = FVector::ZeroVector;
    float OrbitMoveRequestedAt = 0.f;
    // 后退路径在进入状态时提交一次，避免每次决策都中止并重建同一路径。
    bool bRetreatMoveRequested = false;
    FGameplayAbilitySpecHandle LeapAbilityHandle;
    FVector BackstepDirection = FVector::ZeroVector;
    FVector BackstepLandingPoint = FVector::ZeroVector;
    float BackstepSpeed = 0.f;
    int32 BackstepMontageInstanceId = INDEX_NONE;
    TWeakObjectPtr<class UAnimMontage> LeapBackstepMontage;

    UPROPERTY(Transient)
    TObjectPtr<class UAnimSequence> BackstepInPlace;
};
