#pragma once

#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "ActDemoKatanaAttributeSet.generated.h"

UCLASS()
class ACTDEMO_API UActDemoKatanaAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UActDemoKatanaAttributeSet();

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_WalkSpeed, Category = "Katana|Movement")
	FGameplayAttributeData WalkSpeed;
	ATTRIBUTE_ACCESSORS_BASIC(UActDemoKatanaAttributeSet, WalkSpeed)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_RunSpeed, Category = "Katana|Movement")
	FGameplayAttributeData RunSpeed;
	ATTRIBUTE_ACCESSORS_BASIC(UActDemoKatanaAttributeSet, RunSpeed)

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION()
	void OnRep_WalkSpeed(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_RunSpeed(const FGameplayAttributeData& OldValue);
};
