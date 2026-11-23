// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ActDemoLockOnComponent.generated.h"

class AACTDEMOEnemyCharacter;

// 锁定目标改变时，通知蓝图更新镜头、标记等表现。
// NewTarget 为空，表示解除锁定。
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOnLockTargetChanged,
    AActor*,
    NewTarget
);

UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class ACTDEMO_API UActDemoLockOnComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UActDemoLockOnComponent();

    // 未锁定时搜索目标；已锁定时解除。
    UFUNCTION(BlueprintCallable, Category = "Combat|Lock On")
    void ToggleLock();

    UFUNCTION(BlueprintCallable, Category = "Combat|Lock On")
    void ClearLock();

    // Direction < 0 向屏幕左侧切换，> 0 向右侧切换；没有候选时保持原目标。
    UFUNCTION(BlueprintCallable, Category = "Combat|Lock On")
    bool SwitchTarget(float Direction);

    UFUNCTION(BlueprintPure, Category = "Combat|Lock On")
    AActor* GetLockedTarget() const;

    UFUNCTION(BlueprintPure, Category = "Combat|Lock On")
    bool IsLockedOn() const;

    UPROPERTY(BlueprintAssignable, Category = "Combat|Lock On")
    FOnLockTargetChanged OnLockTargetChanged;

protected:
    // 开始锁定时，允许搜索的最大距离。
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Lock On",
        meta = (ClampMin = "1.0"))
    float SearchRadius = 1500.0f;

    // 锁定后超过这个距离，自动解除。
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Lock On",
        meta = (ClampMin = "1.0"))
    float BreakLockDistance = 2000.0f;

    virtual void TickComponent(
        float DeltaTime,
        ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

private:
    // 弱引用不会阻止敌人被销毁。
    UPROPERTY(Transient)
    TWeakObjectPtr<AActor> LockedTarget;

    // 单独记录是否有锁定状态，便于目标销毁后通知蓝图解除。
    bool bHasLock = false;

    AACTDEMOEnemyCharacter* FindBestTarget() const;

    bool IsOwnerAlive() const;

    bool IsEnemyAlive(AActor* Actor) const;
};
