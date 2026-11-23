#include "Combat/ActDemoLockCameraBoom.h"
#include "Combat/ActDemoLockOnComponent.h"
#include "GameFramework/Actor.h"
#include "Templates/UnrealTemplate.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"

void UActDemoLockCameraBoom::BeginQSkillOrbit()
{
    bQSkillOrbit = true;
    if (!bQCameraTagApplied)
        if (UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner()))
        {
            ASC->AddLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Combat.FinisherCamera"))));
            bQCameraTagApplied = true;
        }
}

void UActDemoLockCameraBoom::EndQSkillOrbit()
{
    bQSkillOrbit = false;
}

void UActDemoLockCameraBoom::UpdateDesiredArmLocation(bool bDoTrace, bool bDoLocationLag,
    bool bDoRotationLag, float DeltaTime)
{
    // 偏转独立于蓝图写入的锁定控制角，最后一刀平滑归零后自动回到原跟随。
    CurrentQOrbitAngle = FMath::FInterpConstantTo(CurrentQOrbitAngle,
        bQSkillOrbit ? QOrbitRightAngle : 0.f, DeltaTime,
        bQSkillOrbit ? QOrbitSpeed : QOrbitReturnSpeed);
    // 回正完成再交还锁定蓝图，避免回正的残余角度继续参与相机位置反馈。
    if (!bQSkillOrbit && FMath::IsNearlyZero(CurrentQOrbitAngle) && bQCameraTagApplied)
    {
        if (UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner()))
            ASC->RemoveLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Combat.FinisherCamera"))));
        bQCameraTagApplied = false;
    }
    const AActor* Owner = GetOwner();
    const UActDemoLockOnComponent* LockOn = Owner
        ? Owner->FindComponentByClass<UActDemoLockOnComponent>() : nullptr;
    const FRotator BaseAim = GetTargetRotation().GetNormalized();
    FRotator Aim = BaseAim;
    // 弹簧臂位于朝向的反方向：负 Yaw 让相机位置绕到角色右侧。
    Aim.Yaw -= CurrentQOrbitAngle;
    const AActor* Target = LockOn && LockOn->IsLockedOn() ? LockOn->GetLockedTarget() : nullptr;
    const float DesiredAlpha = IsValid(Target)
        ? FMath::GetMappedRangeValueClamped(FVector2D(100.f, 350.f), FVector2D(0.f, 1.f),
            Target->GetActorLocation().Z - Owner->GetActorLocation().Z)
        : 0.f;
    AirFramingAlpha = FMath::FInterpTo(AirFramingAlpha, DesiredAlpha, DeltaTime, 7.f);
    if (FMath::IsNearlyZero(AirFramingAlpha))
        AirFramingAlpha = 0.f;

    if ((IsValid(Target) && Aim.Pitch > 0.f) || AirFramingAlpha > 0.f ||
        !FMath::IsNearlyZero(CurrentQOrbitAngle))
    {
        // 抬镜时限制镜头向下绕，并按高度差增加后退和抬升量。
        FRotator PositionRotation = Aim;
        if (IsValid(Target))
            PositionRotation.Pitch = FMath::Min(Aim.Pitch, 0.f);
        const FVector ArmOffset = SocketOffset -
            FVector(TargetArmLength + AirLockPullback * AirFramingAlpha, 0.f, 0.f);
        const FVector WorldOffset = PositionRotation.RotateVector(ArmOffset) +
            FVector(0.f, 0.f, AirLockLift * AirFramingAlpha);
        // Super 仍使用原锁定角度，通过临时偏移补偿实际绕镜位置，保留弹簧臂碰撞。
        const FVector CompensatedOffset = BaseAim.UnrotateVector(WorldOffset) +
            FVector(TargetArmLength, 0.f, 0.f);
        TGuardValue<FVector> OffsetGuard(SocketOffset, CompensatedOffset);

        // 锁定图已经对视角做插值。此处使用同一角度计算补偿和弹簧臂位置。
        Super::UpdateDesiredArmLocation(bDoTrace, bDoLocationLag, false, DeltaTime);

        FRotator Framing = Aim;
        if (IsValid(Target) && AirFramingAlpha > 0.f)
        {
            // 从碰撞处理后的实际镜头位置取景，在敌人与玩家上身之间取一个瞄点。
            const FVector PlayerFocus = Owner->GetActorLocation() + FVector(0.f, 0.f, 50.f);
            const FVector Focus = FMath::Lerp(PlayerFocus, Target->GetActorLocation(), 0.55f);
            const FVector CameraLocation = GetComponentTransform().TransformPosition(RelativeSocketLocation);
            Framing.Pitch = Aim.Pitch +
                FMath::FindDeltaAngleDegrees(Aim.Pitch, (Focus - CameraLocation).Rotation().Pitch) * AirFramingAlpha;
        }
        RelativeSocketRotation = GetComponentQuat().Inverse() * Framing.Quaternion();
        UpdateChildTransforms();
        return;
    }

    Super::UpdateDesiredArmLocation(bDoTrace, bDoLocationLag, bDoRotationLag, DeltaTime);
}
