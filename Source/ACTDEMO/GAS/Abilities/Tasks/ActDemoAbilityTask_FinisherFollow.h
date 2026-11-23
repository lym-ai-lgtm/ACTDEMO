#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "ActDemoAbilityTask_FinisherFollow.generated.h"

class AACTDEMOEnemyCharacter;
class UCharacterMovementComponent;
struct FRootMotionSource_ConstantForce;

/** 仅在 Q 实际命中后维持刀程；不搜敌、不扣血、不播放动画。 */
UCLASS()
class ACTDEMO_API UActDemoAbilityTask_FinisherFollow : public UAbilityTask
{
    GENERATED_BODY()
public:
    UActDemoAbilityTask_FinisherFollow();
    static UActDemoAbilityTask_FinisherFollow* FollowHitTarget(UGameplayAbility* Ability,
        AACTDEMOEnemyCharacter* Enemy, float Distance, float CorrectionSpeed, float BreakDistance);
    virtual void TickTask(float DeltaTime) override;
protected:
    virtual void OnDestroy(bool bAbilityEnded) override;
private:
    TWeakObjectPtr<AACTDEMOEnemyCharacter> Target;
    TWeakObjectPtr<UCharacterMovementComponent> TargetMovement;
    TSharedPtr<FRootMotionSource_ConstantForce> FollowSource;
    uint16 FollowSourceId = 0;
    float DesiredDistance = 115.f;
    float MaxCorrectionSpeed = 600.f;
    float MaxSeparation = 300.f;
    FVector FollowDirection = FVector::ForwardVector;
    float FollowHeightOffset = 0.f;
};
