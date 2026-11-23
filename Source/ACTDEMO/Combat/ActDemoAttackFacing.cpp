#include "Combat/ActDemoAttackFacing.h"

#include "Combat/ActDemoLockOnComponent.h"
#include "GAS/ACTDEMOGASCharacter.h"

void ActDemoAttackFacing::FaceLockedTarget(AActor* Attacker)
{
    // 【执行边界】沿用现有服务器攻击规则；受击、死亡或无锁定组件的角色不被强制旋转。
    const AActDemoGASCharacter* Character = Cast<AActDemoGASCharacter>(Attacker);
    if (!IsValid(Character) || !Character->HasAuthority() || Character->GetCurrentHealth() <= 0.f)
        return;

    const UActDemoLockOnComponent* LockOn = Character->FindComponentByClass<UActDemoLockOnComponent>();
    if (!LockOn || !LockOn->IsLockedOn()) return;

    AActor* Target = LockOn->GetLockedTarget();
    const AActDemoGASCharacter* TargetCharacter = Cast<AActDemoGASCharacter>(Target);
    if (!IsValid(Target) || Target == Attacker || Target->IsActorBeingDestroyed() ||
        (TargetCharacter && TargetCharacter->GetCurrentHealth() <= 0.f)) return;

    // 【水平对准】不改变位置、相机或控制器旋转。空中正上/正下方没有水平向量时保留原朝向。
    FVector Direction = Target->GetActorLocation() - Attacker->GetActorLocation();
    Direction.Z = 0.f;
    if (Direction.ContainsNaN() || Direction.IsNearlyZero()) return;

    const FRotator Current = Attacker->GetActorRotation();
    Attacker->SetActorRotation(FRotator(Current.Pitch, Direction.Rotation().Yaw, Current.Roll));
}
