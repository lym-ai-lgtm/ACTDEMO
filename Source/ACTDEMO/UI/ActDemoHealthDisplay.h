#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ActorComponent.h"
#include "ActDemoHealthDisplay.generated.h"
class AActDemoGASCharacter;
class UProgressBar;
class UTextBlock;
class UWidgetComponent;
class UAbilitySystemComponent;
struct FOnAttributeChangeData;

// 可派生 Widget Blueprint，自定义视觉时保留 HealthBar、HealthText 的控件名。
UCLASS(Blueprintable)
class ACTDEMO_API UActDemoHealthWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void SetHealthSource(AActDemoGASCharacter* Source);
    void RefreshHealth();
protected:
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UProgressBar> HealthBar;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UProgressBar> PostureBar;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> HealthText;
private:
    TWeakObjectPtr<AActDemoGASCharacter> HealthSource;
};

UCLASS(ClassGroup=(UI), meta=(BlueprintSpawnableComponent))
class ACTDEMO_API UActDemoHealthDisplayComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UActDemoHealthDisplayComponent();
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* TickFunction) override;
    UPROPERTY(EditAnywhere, Category="Health UI") bool bShowHealthUI = true;
    UPROPERTY(EditAnywhere, Category="Health UI") TSubclassOf<UActDemoHealthWidget> HealthWidgetClass;
    UPROPERTY(EditAnywhere, Category="Health UI") float EnemyBarHeight = 130.f;
    UPROPERTY(EditAnywhere, Category="Health UI") FVector2D EnemyBarSize = FVector2D(150,36);
    // 关闭后恢复存活敌人的血条常显。
    UPROPERTY(EditAnywhere, Category="Health UI|Enemy Visibility")
    bool bShowEnemyOnlyWhenDamagedOrLocked = true;
    UPROPERTY(EditAnywhere, Category="Health UI|Enemy Visibility", meta=(ClampMin="0.0"))
    float EnemyHealthDisplayDuration = 3.f;
private:
    void OnEnemyHealthChanged(const FOnAttributeChangeData& Data);
    void OnEnemyPostureChanged(const FOnAttributeChangeData& Data);
    TWeakObjectPtr<UAbilitySystemComponent> ObservedASC;
    FDelegateHandle HealthChangedHandle;
    FDelegateHandle PostureChangedHandle;
    double LastDamageTime = -1.0e30;
    UPROPERTY(Transient) TObjectPtr<UActDemoHealthWidget> PlayerHUD;
    UPROPERTY(Transient) TObjectPtr<UWidgetComponent> EnemyBar;
};
