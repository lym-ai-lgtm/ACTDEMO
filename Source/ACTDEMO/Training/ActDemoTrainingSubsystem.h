#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ActDemoTrainingSubsystem.generated.h"
class UUserWidget;
class APlayerController;
class ACharacter;
class IInputProcessor;

/** 仅训练地图运行：UMG负责外观，本模块负责选点、碰撞预览与生成。 */
UCLASS()
class ACTDEMO_API UActDemoTrainingSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual void OnWorldBeginPlay(UWorld& World) override;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(ActDemoTraining, STATGROUP_Tickables); }
    virtual bool IsTickable() const override { return (bTraining || bMenu) && !IsTemplate(); }
    virtual void Deinitialize() override;
    UFUNCTION(BlueprintCallable, Category="Training") void BeginDummyPlacement();
    UFUNCTION(BlueprintCallable, Category="Training") void CancelDummyPlacement();
    UFUNCTION(BlueprintCallable, Category="Training") bool TryPlaceDummyAtScreenPosition(FVector2D ScreenPosition, bool bActiveEnemy = false);
    UFUNCTION(BlueprintCallable, Category="Training") bool ReturnToMainMenu();
private:
    void InitializePanel();
    void RefreshTrainingInputMode();
    bool bTraining = false;
    bool bMenu = false;
    bool bPlacing = false;
    bool bTrainingInputInitialized = false;
    bool bCombatInput = false;
    bool bReturningToMenu = false;
    TSharedPtr<IInputProcessor> ExitInputProcessor;
    UPROPERTY(Transient) TObjectPtr<UUserWidget> Panel;
    UPROPERTY(Transient) TObjectPtr<APlayerController> Controller;
    UPROPERTY(Transient) TSubclassOf<ACharacter> DummyClass;
    UPROPERTY(Transient) TSubclassOf<ACharacter> ActiveEnemyClass;
};
