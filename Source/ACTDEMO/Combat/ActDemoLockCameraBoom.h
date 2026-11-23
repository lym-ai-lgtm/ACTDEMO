#pragma once

#include "GameFramework/SpringArmComponent.h"
#include "ActDemoLockCameraBoom.generated.h"

UCLASS()
class ACTDEMO_API UActDemoLockCameraBoom : public USpringArmComponent
{
    GENERATED_BODY()

public:
    // 只偏转弹簧臂取景，不改控制器方向，避免镜头转动带偏攻击与锁定方向。
    UFUNCTION(BlueprintCallable, Category="Camera|Q Orbit")
    void BeginQSkillOrbit();

    UFUNCTION(BlueprintCallable, Category="Camera|Q Orbit")
    void EndQSkillOrbit();

protected:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Q Orbit", meta=(ClampMin="0.0", ClampMax="90.0", Units="deg"))
    float QOrbitRightAngle = 40.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Q Orbit", meta=(ClampMin="1.0", Units="deg/s"))
    float QOrbitSpeed = 120.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Q Orbit", meta=(ClampMin="1.0", Units="deg/s"))
    float QOrbitReturnSpeed = 180.f;

    UPROPERTY(EditAnywhere, Category="Camera|Air Lock", meta=(ClampMin="0.0", Units="cm"))
    float AirLockPullback = 300.f;

    UPROPERTY(EditAnywhere, Category="Camera|Air Lock", meta=(ClampMin="0.0", Units="cm"))
    float AirLockLift = 60.f;

    virtual void UpdateDesiredArmLocation(bool bDoTrace, bool bDoLocationLag,
        bool bDoRotationLag, float DeltaTime) override;

private:
    float AirFramingAlpha = 0.f;
    bool bQSkillOrbit = false;
    float CurrentQOrbitAngle = 0.f;
    bool bQCameraTagApplied = false;
};
