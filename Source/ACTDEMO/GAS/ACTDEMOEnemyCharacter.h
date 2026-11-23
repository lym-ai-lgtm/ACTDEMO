// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "GAS/ACTDEMOGASCharacter.h"
#include "ACTDEMOEnemyCharacter.generated.h"

class UGameplayAbility;
struct FOnAttributeChangeData;

/**
 * GAS-enabled enemy base. Configure starting abilities and inherited initial
 * attributes on each enemy Blueprint child. Animation and AI behavior stay in BP.
 */
UCLASS(Blueprintable)
class ACTDEMO_API AACTDEMOEnemyCharacter : public AActDemoGASCharacter
{
	GENERATED_BODY()

public:
	AACTDEMOEnemyCharacter();

	// 只处理挑飞状态和 CharacterMovement，动画通过 Gameplay Event 由蓝图负责。
	UFUNCTION(BlueprintCallable, Category="Combat|Airborne")
	bool LaunchFromAttack(AActor* Attacker, float UpwardSpeed, float ForwardSpeed);

	// 震荡波用攻击者到敌人的水平向量，和原上挑共享落地恢复流程。
	UFUNCTION(BlueprintCallable, Category="Combat|Airborne")
	bool LaunchAwayFromAttack(AActor* Attacker, float UpwardSpeed, float OutwardSpeed);

	UFUNCTION(BlueprintPure, Category="Combat|Airborne")
	bool IsLaunched() const { return bLaunchActive; }

	UFUNCTION(BlueprintCallable, Category="Combat|Airborne")
	bool KnockDownFromAir(float DownwardSpeed = 1600.f);

	UFUNCTION(BlueprintPure, Category="Combat|Airborne")
	bool IsAirKnockedDown() const { return bAirKnockdownActive; }

	// 供 AttributeSet 在实际扣血前检查并消耗敌人的招架值。
	bool TryConsumePosture(float IncomingDamage, AActor* AttackSource);

	// 蓝图使用敌人前向与来袭方向的水平点积判断防御角度。
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category="Combat|Defense")
	bool IsAttackInGuardArc(const FVector& AttackSourceLocation) const;
	virtual bool IsAttackInGuardArc_Implementation(const FVector& AttackSourceLocation) const;

	UFUNCTION(BlueprintPure, Category="GAS|Posture")
	float GetCurrentPosture() const;

	UFUNCTION(BlueprintPure, Category="GAS|Posture")
	float GetCurrentMaxPosture() const;

	// 破防状态持续到招架值回满，不能用当前招架值是否大于 0 判断。
	UFUNCTION(BlueprintPure, Category="GAS|Posture")
	bool IsPostureBroken() const { return bPostureBroken; }


	// 蓝图仅处理招架动画表现；伤害拦截和招架值消耗由 C++ 共用逻辑完成。
	UFUNCTION(BlueprintImplementableEvent, Category="Combat|Posture")
	void OnEnemyPostureBlocked(float PostureDamage, float RemainingPosture);

	// 气势从正数降至零时触发一次，蓝图负责破防动画、特效和镜头。
	UFUNCTION(BlueprintImplementableEvent, Category="Combat|Posture")
	void OnEnemyPostureBroken(AActor* Attacker);

protected:
	/** Abilities granted to this enemy on authority when play begins. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GAS|Enemy Abilities")
	TArray<TSubclassOf<UGameplayAbility>> StartupAbilities;

	/** Level used when granting each StartupAbilities entry. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Enemy Abilities", meta = (ClampMin = "1"))
	int32 StartupAbilityLevel = 1;

	/** 每个敌人蓝图可单独调整的招架值上限。设为 0 可关闭招架。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Initial Attributes|Posture", meta = (ClampMin = "0.0"))
	float InitialMaxPosture = 100.0f;

	/** 关卡开始时的招架值，默认满值。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Initial Attributes|Posture", meta = (ClampMin = "0.0"))
	float InitialPosture = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GAS|Posture|Recovery", meta=(ClampMin="0.0", Units="s"))
	float PostureRecoveryDelay = 2.f;

	// 每秒恢复的数值以招架上限的百分比计。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GAS|Posture|Recovery", meta=(ClampMin="1.0"))
	float NormalPostureRecoveryPercentPerSecond = 25.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GAS|Posture|Recovery", meta=(ClampMin="1.0"))
	float BrokenPostureRecoveryPercentPerSecond = 10.f;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Landed(const FHitResult& Hit) override;

private:
	bool IsInGroundRecovery() const;
	bool StartAttackLaunch(const FVector& HorizontalVelocity, float UpwardSpeed);
	void RestoreRadialLaunchBraking();
	TOptional<float> SavedFallingBrakingDeceleration;
	TOptional<float> SavedFallingLateralFriction;
	void OnEnemyHealthChanged(const FOnAttributeChangeData& Data);
	void OnEnemyPostureChanged(const FOnAttributeChangeData& Data);
	void SchedulePostureRecovery();
	void RecoverPosture();
	void EnterDeadState();
	FDelegateHandle HealthChangedHandle;
	FDelegateHandle PostureChangedHandle;
	FTimerHandle PostureRecoveryTimer;
	FTimerHandle PostureBreakReactionTimer;
	// 普通招架直接设置属性，属性变化回调需要从当前扣量操作取得来源。
	TWeakObjectPtr<AActor> PostureDamageSource;
	bool bPostureBroken = false;
	bool bDeathHandled = false;
	// 已死亡但尚未触地时继续运行重力，Landed 再通知死亡能力播放动画。
	bool bWaitingForDeathLanding = false;
	bool bLaunchActive = false;
	bool bAirKnockdownActive = false;
};
