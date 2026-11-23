// Fill out your copyright notice in the Description page of Project Settings.


#include "ActDemoGASCharacter.h"

#include "AbilitySystemComponent.h"
#include "ActDemoAttributeSet.h"
#include "EnhancedInputComponent.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayAbilitySpec.h"
#include "InputAction.h"
#include "UI/ActDemoHealthDisplay.h"
#include "Combat/ActDemoAirSuspensionComponent.h"
#include "Combat/ActDemoCombatInputBufferComponent.h"
#include "GAS/Katana/ActDemoKatanaAttributeSet.h"
#include "GAS/Katana/ActDemoKatanaEquipAbility.h"

AActDemoGASCharacter::AActDemoGASCharacter()
{
    // 【固定命名默认子对象】编译后玩家蓝图可直接获取并绑定缓冲事件。
    CombatInputBuffer = CreateDefaultSubobject<UActDemoCombatInputBufferComponent>(TEXT("CombatInputBuffer"));
    CreateDefaultSubobject<UActDemoAirSuspensionComponent>(TEXT("AirSuspension"));
    CreateDefaultSubobject<UActDemoHealthDisplayComponent>(TEXT("HealthDisplay"));
	//在构造函数内创建AbilitySystemComponent和AttributeSet组件
	AbilitySystemComponent =
		CreateDefaultSubobject<UAbilitySystemComponent>(
			TEXT("AbilitySystemComponent"));

	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(
		EGameplayEffectReplicationMode::Mixed);

	AttributeSet =
		CreateDefaultSubobject<UActDemoAttributeSet>(
			TEXT("AttributeSet"));
}

void AActDemoGASCharacter::BeginPlay()
{
    if (AbilitySystemComponent)
    {
        AbilitySystemComponent->InitAbilityActorInfo(this, this);
    }

    // 初始化一次。单机运行时 HasAuthority() 也为 true。
    if (HasAuthority() && AbilitySystemComponent && AttributeSet)
    {
        const float MaxHealthValue =
            FMath::Max(1.0f, InitialMaxHealth);

        const float MaxStaminaValue =
            FMath::Max(0.0f, InitialMaxStamina);

        // 先设置上限，再设置当前值。
        AbilitySystemComponent->SetNumericAttributeBase(
            UActDemoAttributeSet::GetMaxHealthAttribute(),
            MaxHealthValue);

        AbilitySystemComponent->SetNumericAttributeBase(
            UActDemoAttributeSet::GetHealthAttribute(),
            FMath::Clamp(InitialHealth, 0.0f, MaxHealthValue));

        AbilitySystemComponent->SetNumericAttributeBase(
            UActDemoAttributeSet::GetMaxStaminaAttribute(),
            MaxStaminaValue);

        AbilitySystemComponent->SetNumericAttributeBase(
            UActDemoAttributeSet::GetStaminaAttribute(),
            FMath::Clamp(InitialStamina, 0.0f, MaxStaminaValue));
    }

    if (HasAuthority() && AbilitySystemComponent && KatanaEquipAbilityClass)
    {
        UActDemoKatanaAttributeSet* KatanaAttributes =
            NewObject<UActDemoKatanaAttributeSet>(this, TEXT("KatanaAttributeSet"));
        KatanaAttributes->InitWalkSpeed(FMath::Max(0.f, InitialKatanaWalkSpeed));
        KatanaAttributes->InitRunSpeed(FMath::Max(0.f, InitialKatanaRunSpeed));
        AbilitySystemComponent->AddAttributeSetSubobject(KatanaAttributes);
        AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(KatanaEquipAbilityClass, 1, INDEX_NONE, this));
    }

    // 让蓝图 BeginPlay 执行时，GAS 初始属性已经准备好。
    Super::BeginPlay();

    // 保留原有的调试能力授予逻辑。
    if (HasAuthority() && AbilitySystemComponent && DebugAbilityClass)
    {
        AbilitySystemComponent->GiveAbility(
            FGameplayAbilitySpec(
                DebugAbilityClass, 1, INDEX_NONE, this));
    }
}



UAbilitySystemComponent*
AActDemoGASCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

float AActDemoGASCharacter::GetCurrentHealth() const
{
	return AttributeSet ? AttributeSet->GetHealth() : 0.0f;
}

float AActDemoGASCharacter::GetCurrentMaxHealth() const
{
	return AttributeSet ? AttributeSet->GetMaxHealth() : 0.0f;
}

float AActDemoGASCharacter::GetCurrentStamina() const
{
	return AttributeSet ? AttributeSet->GetStamina() : 0.0f;
}

float AActDemoGASCharacter::GetKatanaMovementSpeed(bool bSprinting) const
{
    const FGameplayAttribute SpeedAttribute = bSprinting
        ? UActDemoKatanaAttributeSet::GetRunSpeedAttribute()
        : UActDemoKatanaAttributeSet::GetWalkSpeedAttribute();
    if (AbilitySystemComponent && AbilitySystemComponent->HasAttributeSetForAttribute(SpeedAttribute))
    {
        return FMath::Max(0.f, AbilitySystemComponent->GetNumericAttribute(SpeedAttribute));
    }
    return FMath::Max(0.f, bSprinting ? InitialKatanaRunSpeed : InitialKatanaWalkSpeed);
}

void AActDemoGASCharacter::GainAirComboMomentum()
{
    if (HasAuthority() && GetCurrentHealth() > 0.f)
        Momentum = FMath::Clamp(Momentum + MomentumPerAirHit, 0.f, MaxMomentum);
}

bool AActDemoGASCharacter::SpendFullMomentum()
{
    if (!HasAuthority() || !HasFullMomentum())
        return false;

    Momentum = 0.f;
    return true;
}

void AActDemoGASCharacter::SetupPlayerInputComponent(
	UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInput =
		Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (DebugAbilityAction)
		{
			EnhancedInput->BindAction(
				DebugAbilityAction,
				ETriggerEvent::Started,
				this,
				&AActDemoGASCharacter::OnDebugAbilityPressed);
		}
	}
}

void AActDemoGASCharacter::OnDebugAbilityPressed()
{
	if (AbilitySystemComponent && DebugAbilityClass)
	{
		AbilitySystemComponent->TryActivateAbilityByClass(DebugAbilityClass);
	}
}


