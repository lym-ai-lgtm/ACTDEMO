// Fill out your copyright notice in the Description page of Project Settings.

#include "GAS/ACTDEMOEnemyCharacter.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffectExtension.h"
#include "GAS/ActDemoAttributeSet.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Combat/ActDemoAirSuspensionComponent.h"
#include "TimerManager.h"
#include "Templates/UnrealTemplate.h"

namespace
{
	constexpr float PostureRecoveryInterval = 0.1f;
}

AACTDEMOEnemyCharacter::AACTDEMOEnemyCharacter()
{
	// 待机测试敌人没有 Controller，也需要重力、地面检测和 LaunchCharacter。
	GetCharacterMovement()->bRunPhysicsWithNoController = true;
	// AI possession is selected per Blueprint so this base can also be previewed
	// or placed without requiring a particular AIController class.
}

void AACTDEMOEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();

	// 已有蓝图/关卡实例可能还保存着旧的 false 值，运行时确保一致。
	// 只在初始化恢复模式，死亡后的 DisableMovement 不会被这里重新开启。
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bRunPhysicsWithNoController = true;
	if (GetCurrentHealth() > 0.f && Movement->MovementMode == MOVE_None)
	{
		Movement->SetDefaultMovementMode();
	}
	if (HasAuthority() && AbilitySystemComponent && AttributeSet)
	{
		const float MaxPostureValue = FMath::Max(0.f, InitialMaxPosture);
		AbilitySystemComponent->SetNumericAttributeBase(
			UActDemoAttributeSet::GetMaxPostureAttribute(), MaxPostureValue);
		AbilitySystemComponent->SetNumericAttributeBase(
			UActDemoAttributeSet::GetPostureAttribute(),
			FMath::Clamp(InitialPosture, 0.f, MaxPostureValue));
	}

	// The inherited GAS character initializes ActorInfo and attributes in its
	// BeginPlay. Grant enemy abilities afterward, on the server/authority only.
	if (!HasAuthority() || !AbilitySystemComponent)
	{
		return;
	}

	for (const TSubclassOf<UGameplayAbility>& AbilityClass : StartupAbilities)
	{
		if (!AbilityClass)
		{
			continue;
		}

		AbilitySystemComponent->GiveAbility(
			FGameplayAbilitySpec(AbilityClass, FMath::Max(1, StartupAbilityLevel), INDEX_NONE, this));
	}
	HealthChangedHandle = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
		UActDemoAttributeSet::GetHealthAttribute()).AddUObject(this,
			&AACTDEMOEnemyCharacter::OnEnemyHealthChanged);
	PostureChangedHandle = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
		UActDemoAttributeSet::GetPostureAttribute()).AddUObject(this,
			&AACTDEMOEnemyCharacter::OnEnemyPostureChanged);
	if (GetCurrentHealth() <= 0.f)
	{
		EnterDeadState();
	}
	else if (GetCurrentPosture() < GetCurrentMaxPosture())
	{
		bPostureBroken = GetCurrentPosture() <= 0.f;
		SchedulePostureRecovery();
	}
}

float AACTDEMOEnemyCharacter::GetCurrentPosture() const
{
	return AttributeSet ? AttributeSet->GetPosture() : 0.f;
}

float AACTDEMOEnemyCharacter::GetCurrentMaxPosture() const
{
	return AttributeSet ? AttributeSet->GetMaxPosture() : 0.f;
}

bool AACTDEMOEnemyCharacter::IsAttackInGuardArc_Implementation(const FVector&) const
{
	// 没有方向蓝图的敌人继续沿用原招架行为。
	return true;
}

bool AACTDEMOEnemyCharacter::IsInGroundRecovery() const
{
	const FGameplayTag RecoveryTag = FGameplayTag::RequestGameplayTag(
		FName(TEXT("State.Combat.GroundRecovery")));
	return AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(RecoveryTag);
}

bool AACTDEMOEnemyCharacter::TryConsumePosture(float IncomingDamage, AActor* AttackSource)
{
	if (!HasAuthority() || !AttributeSet || bPostureBroken || GetCurrentHealth() <= 0.f ||
		!FMath::IsFinite(IncomingDamage) || IncomingDamage <= 0.f ||
		GetCurrentPosture() <= 0.f || IsInGroundRecovery() ||
		!IsValid(AttackSource) || AttackSource == this ||
		!IsAttackInGuardArc(AttackSource->GetActorLocation()))
	{
		return false;
	}

	const float PostureDamage = IncomingDamage;
	const float RemainingPosture = FMath::Max(0.f, GetCurrentPosture() - PostureDamage);
	{
		TGuardValue<TWeakObjectPtr<AActor>> SourceGuard(PostureDamageSource, TWeakObjectPtr<AActor>(AttackSource));
		AttributeSet->SetPosture(RemainingPosture);
	}
	if (RemainingPosture > 0.f ||
		!GetClass()->IsFunctionImplementedInScript(GET_FUNCTION_NAME_CHECKED(AACTDEMOEnemyCharacter, OnEnemyPostureBroken)))
	{
		OnEnemyPostureBlocked(PostureDamage, RemainingPosture);
	}
	return true;
}

void AACTDEMOEnemyCharacter::OnEnemyHealthChanged(const FOnAttributeChangeData& Data)
{
	if (Data.NewValue <= 0.f)
	{
		EnterDeadState();
	}
	else if (Data.NewValue < Data.OldValue)
	{
		if (!bPostureBroken && GetCurrentPosture() < GetCurrentMaxPosture())
		{
			SchedulePostureRecovery();
		}
	}
}

void AACTDEMOEnemyCharacter::OnEnemyPostureChanged(const FOnAttributeChangeData& Data)
{
	if (bDeathHandled || GetCurrentHealth() <= 0.f)
	{
		return;
	}

	if (Data.NewValue >= GetCurrentMaxPosture())
	{
		bPostureBroken = false;
		GetWorldTimerManager().ClearTimer(PostureRecoveryTimer);
	}
	else if (Data.NewValue < Data.OldValue)
	{
		// 普通招架每次掉量重新计时。归零后立即开始慢速恢复。
		const bool bJustBroke = !bPostureBroken && Data.OldValue > 0.f && Data.NewValue <= 0.f;
		bPostureBroken = bPostureBroken || bJustBroke;
		SchedulePostureRecovery();

		if (bJustBroke)
		{
			TWeakObjectPtr<AActor> Attacker = PostureDamageSource;
			if (!Attacker.IsValid() && Data.GEModData)
			{
				const FGameplayEffectContextHandle& Context = Data.GEModData->EffectSpec.GetContext();
				Attacker = Context.GetOriginalInstigator();
				if (!Attacker.IsValid() || Attacker.Get() == this)
				{
					Attacker = Context.GetEffectCauser();
				}
			}

			// 等当前 Gameplay Effect 结算完，再让蓝图发事件或启动其他能力。
			PostureBreakReactionTimer = GetWorldTimerManager().SetTimerForNextTick(
				FTimerDelegate::CreateWeakLambda(this, [this, Attacker]()
				{
					if (bPostureBroken && !bDeathHandled && GetCurrentHealth() > 0.f)
					{
						OnEnemyPostureBroken(Attacker.Get());
					}
				}));
		}
	}
}

void AACTDEMOEnemyCharacter::SchedulePostureRecovery()
{
	const float FirstTickDelay = bPostureBroken
		? PostureRecoveryInterval
		: FMath::Max(0.f, PostureRecoveryDelay);
	GetWorldTimerManager().SetTimer(PostureRecoveryTimer, this,
		&AACTDEMOEnemyCharacter::RecoverPosture, PostureRecoveryInterval, true, FirstTickDelay);
}

void AACTDEMOEnemyCharacter::RecoverPosture()
{
	const float Maximum = GetCurrentMaxPosture();
	if (!AttributeSet || GetCurrentHealth() <= 0.f || Maximum <= 0.f || GetCurrentPosture() >= Maximum)
	{
		GetWorldTimerManager().ClearTimer(PostureRecoveryTimer);
		return;
	}

	const float PercentPerSecond = bPostureBroken
		? BrokenPostureRecoveryPercentPerSecond : NormalPostureRecoveryPercentPerSecond;
	const float Restored = Maximum * FMath::Max(1.f, PercentPerSecond) * 0.01f * PostureRecoveryInterval;
	const float NextPosture = GetCurrentPosture() + Restored;
	AttributeSet->SetPosture(NextPosture >= Maximum - KINDA_SMALL_NUMBER ? Maximum : NextPosture);
}

void AACTDEMOEnemyCharacter::EnterDeadState()
{
	if (bDeathHandled || !AbilitySystemComponent) return;
	bDeathHandled = true;
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	bWaitingForDeathLanding = Movement->IsFalling() || Movement->IsFlying();
	const double FallingSpeed = FMath::Min(0.0, Movement->Velocity.Z);
	RestoreRadialLaunchBraking();
	GetWorldTimerManager().ClearTimer(PostureRecoveryTimer);
	GetWorldTimerManager().ClearTimer(PostureBreakReactionTimer);
	bAirKnockdownActive = false;
	if (UActDemoAirSuspensionComponent* Suspension = FindComponentByClass<UActDemoAirSuspensionComponent>())
		Suspension->ReleaseSuspension();
	if (bLaunchActive)
	{
		bLaunchActive = false;
		AbilitySystemComponent->RemoveLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Combat.Airborne"))));
	}
	// 持久死亡状态独立于死亡蒙太奇能力，结束动画不能让敌人复活。
	AbilitySystemComponent->AddLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Combat.Dead"))));
	AbilitySystemComponent->CancelAllAbilities();
	Movement->StopMovementImmediately();
	if (AAIController* AI = Cast<AAIController>(GetController()))
	{
		AI->StopMovement();
		if (UBrainComponent* Brain = AI->GetBrainComponent()) Brain->StopLogic(TEXT("Enemy died"));
		// 当前 AI 使用控制器自定义 Timer；解除占有也让它无法继续取得敌人 Pawn。
		AI->UnPossess();
	}
	if (bWaitingForDeathLanding)
	{
		// 停止水平位移和上升，保留下落速度。覆盖未执行的旧 LaunchCharacter 冲量，
		// 防止同帧死亡后仍被旧招式向上推，同时让无 Controller 的敌人继续落地。
		Movement->bRunPhysicsWithNoController = true;
		Movement->SetMovementMode(MOVE_Falling);
		LaunchCharacter(FVector(0.f, 0.f, FallingSpeed), true, true);
	}
	else
	{
		Movement->DisableMovement();
	}
}

bool AACTDEMOEnemyCharacter::LaunchFromAttack(AActor* Attacker, float UpwardSpeed, float ForwardSpeed)
{
	if (!IsValid(Attacker) || !FMath::IsFinite(ForwardSpeed))
	{
		return false;
	}
	const FVector Direction = Attacker->GetActorForwardVector().GetSafeNormal2D();
	return StartAttackLaunch(Direction * FMath::Max(0.f, ForwardSpeed), UpwardSpeed);
}

bool AACTDEMOEnemyCharacter::LaunchAwayFromAttack(AActor* Attacker, float UpwardSpeed, float OutwardSpeed)
{
	if (!IsValid(Attacker) || !FMath::IsFinite(OutwardSpeed))
	{
		return false;
	}

	FVector Direction = (GetActorLocation() - Attacker->GetActorLocation()).GetSafeNormal2D();
	if (Direction.IsNearlyZero())
	{
		Direction = Attacker->GetActorForwardVector().GetSafeNormal2D();
	}
	if (!StartAttackLaunch(Direction * FMath::Max(0.f, OutwardSpeed), UpwardSpeed))
	{
		return false;
	}

	// 空中制动会很快吃掉水平速度。径向抛飞期间保留这段水平位移。
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	SavedFallingBrakingDeceleration = Movement->BrakingDecelerationFalling;
	SavedFallingLateralFriction = Movement->FallingLateralFriction;
	Movement->BrakingDecelerationFalling = 0.f;
	Movement->FallingLateralFriction = 0.f;
	return true;
}

void AACTDEMOEnemyCharacter::RestoreRadialLaunchBraking()
{
	if (SavedFallingBrakingDeceleration.IsSet() && SavedFallingLateralFriction.IsSet())
	{
		UCharacterMovementComponent* Movement = GetCharacterMovement();
		Movement->BrakingDecelerationFalling = SavedFallingBrakingDeceleration.GetValue();
		Movement->FallingLateralFriction = SavedFallingLateralFriction.GetValue();
		SavedFallingBrakingDeceleration.Reset();
		SavedFallingLateralFriction.Reset();
	}
}

bool AACTDEMOEnemyCharacter::StartAttackLaunch(const FVector& HorizontalVelocity, float UpwardSpeed)
{
	if (!HasAuthority() || bDeathHandled || bLaunchActive || IsInGroundRecovery() || GetCurrentHealth() <= 0.f ||
		!AbilitySystemComponent || !FMath::IsFinite(UpwardSpeed) || UpwardSpeed <= 0.f ||
		!GetCharacterMovement()->IsMovingOnGround()) return false;

	bLaunchActive = true;
	bAirKnockdownActive = false;
	AbilitySystemComponent->AddLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Combat.Airborne"))));
	AbilitySystemComponent->CancelAllAbilities();
	if (AAIController* AI = Cast<AAIController>(GetController()))
	{
		AI->StopMovement();
		if (UBrainComponent* Brain = AI->GetBrainComponent()) Brain->PauseLogic(TEXT("Enemy launched"));
	}
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	LaunchCharacter(HorizontalVelocity + FVector(0.f, 0.f, UpwardSpeed), true, true);

	return true;
}

bool AACTDEMOEnemyCharacter::KnockDownFromAir(float DownwardSpeed)
{
	if (!HasAuthority() || bDeathHandled || !bLaunchActive || bAirKnockdownActive ||
		GetCurrentHealth() <= 0.f || !FMath::IsFinite(DownwardSpeed) || DownwardSpeed <= 0.f ||
		!GetCharacterMovement()->IsFalling()) return false;

	bAirKnockdownActive = true;
	if (UActDemoAirSuspensionComponent* Suspension = FindComponentByClass<UActDemoAirSuspensionComponent>())
		Suspension->ReleaseSuspension();
	GetCharacterMovement()->StopMovementImmediately();
	LaunchCharacter(FVector(0.f, 0.f, -DownwardSpeed), true, true);
	// 不清除 Airborne 或恢复 AI；实际落地仍由 Landed 交接倒地、起身。
	return true;
}

void AACTDEMOEnemyCharacter::Landed(const FHitResult& Hit)
{
	RestoreRadialLaunchBraking();
	Super::Landed(Hit);
	if (UActDemoAirSuspensionComponent* Suspension = FindComponentByClass<UActDemoAirSuspensionComponent>())
		Suspension->ReleaseSuspension();
	if (bWaitingForDeathLanding)
	{
		// 死亡落地只交接死亡动画，不进入存活目标的倒地、起身流程。
		bWaitingForDeathLanding = false;
		GetCharacterMovement()->StopMovementImmediately();
		GetCharacterMovement()->DisableMovement();
		FGameplayEventData Event;
		Event.EventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Combat.EnemyDeath.Landed")));
		Event.Target = this;
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(this, Event.EventTag, Event);
		return;
	}
	if (!bLaunchActive || bDeathHandled) return;
	bLaunchActive = false;
	bAirKnockdownActive = false;
	AbilitySystemComponent->RemoveLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Combat.Airborne"))));
	FGameplayEventData Event;
	Event.EventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Combat.Launch.Landed")));
	Event.Target = this;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(this, Event.EventTag, Event);
	if (AAIController* AI = Cast<AAIController>(GetController()))
	{
		if (UBrainComponent* Brain = AI->GetBrainComponent()) Brain->ResumeLogic(TEXT("Enemy landed"));
	}
}

void AACTDEMOEnemyCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RestoreRadialLaunchBraking();
	GetWorldTimerManager().ClearTimer(PostureRecoveryTimer);
	GetWorldTimerManager().ClearTimer(PostureBreakReactionTimer);
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
			UActDemoAttributeSet::GetHealthAttribute()).Remove(HealthChangedHandle);
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
			UActDemoAttributeSet::GetPostureAttribute()).Remove(PostureChangedHandle);
	}
	Super::EndPlay(EndPlayReason);
}
