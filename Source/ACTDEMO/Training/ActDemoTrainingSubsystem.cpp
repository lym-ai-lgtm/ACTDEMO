#include "Training/ActDemoTrainingSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GAS/ACTDEMOEnemyCharacter.h"
#include "Combat/ActDemoLockOnComponent.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/IInputProcessor.h"
#include "TimerManager.h"

namespace
{
    // 仅训练世界注册，先于UI与PIE快捷键处理Esc，不改全局编辑器快捷键设置。
    class FTrainingExitInputProcessor final : public IInputProcessor
    {
    public:
        explicit FTrainingExitInputProcessor(UActDemoTrainingSubsystem* InOwner) : Owner(InOwner) {}
        virtual void Tick(float, FSlateApplication&, TSharedRef<ICursor>) override {}
        virtual bool HandleKeyDownEvent(FSlateApplication&, const FKeyEvent& Event) override
        {
            return Event.GetKey() == EKeys::Escape && Owner.IsValid() && Owner->ReturnToMainMenu();
        }
    private:
        TWeakObjectPtr<UActDemoTrainingSubsystem> Owner;
    };
}

void UActDemoTrainingSubsystem::OnWorldBeginPlay(UWorld& World)
{
    Super::OnWorldBeginPlay(World);
    bTraining = World.IsGameWorld() && World.GetNetMode() != NM_DedicatedServer &&
        UGameplayStatics::GetCurrentLevelName(&World, true) == TEXT("Lvl_Training");
    bMenu = World.IsGameWorld() && World.GetNetMode() != NM_DedicatedServer &&
        UGameplayStatics::GetCurrentLevelName(&World, true) == TEXT("Lvl_MainMenu");
}

void UActDemoTrainingSubsystem::InitializePanel()
{
    Controller = GetWorld()->GetFirstPlayerController();
    if (!Controller || !Controller->IsLocalController()) return;
    auto* UIClass = LoadClass<UUserWidget>(nullptr, bMenu
        ? TEXT("/Game/Demo/UI/MainMenu/WBP_MainMenu.WBP_MainMenu_C")
        : TEXT("/Game/Demo/UI/Training/WBP_TrainingPanel.WBP_TrainingPanel_C"));
    DummyClass = LoadClass<ACharacter>(nullptr, TEXT("/Game/Demo/GAS/BP_GASPassiveEnemy.BP_GASPassiveEnemy_C"));
    ActiveEnemyClass = LoadClass<ACharacter>(nullptr, TEXT("/Game/Demo/GAS/BP_GASMeleeEnemy.BP_GASMeleeEnemy_C"));
    if (!UIClass || !DummyClass) return;
    Panel = CreateWidget<UUserWidget>(Controller, UIClass);
    if (!Panel) return;
    // 训练地图复用测试场景几何，开局清除模板自带敌人，之后仅生成玩家选择的假人。
    for (TActorIterator<AACTDEMOEnemyCharacter> It(GetWorld()); It; ++It)
        It->Destroy();
    Panel->AddToViewport(bMenu ? 100 : 20);
    Controller->bShowMouseCursor = true;
    if (bMenu)
    {
        UWidgetBlueprintLibrary::SetInputMode_UIOnlyEx(Controller, nullptr, EMouseLockMode::DoNotLock);
        if (APawn* Pawn = Controller->GetPawn())
        {
            Pawn->SetActorHiddenInGame(true);
            Pawn->DisableInput(Controller);
        }
    }
    else
        RefreshTrainingInputMode();
    if (bTraining && FSlateApplication::IsInitialized() && !ExitInputProcessor.IsValid())
    {
        ExitInputProcessor = MakeShared<FTrainingExitInputProcessor>(this);
        FSlateApplication::Get().RegisterInputPreProcessor(ExitInputProcessor, 0);
    }
}

bool UActDemoTrainingSubsystem::ReturnToMainMenu()
{
    if (!bTraining || !Controller || !GetWorld() || GetWorld()->bIsTearingDown) return false;
    if (bReturningToMenu) return true;
    bReturningToMenu = true;
    if (FSlateApplication::IsInitialized()) FSlateApplication::Get().CancelDragDrop();
    CancelDummyPlacement();
    // 不在Slate按键分发栈内销毁UI；下一帧再切图，并用弱委托防止旧世界回调。
    GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
    {
        UGameplayStatics::OpenLevel(this, FName(TEXT("/Game/Demo/Maps/Lvl_MainMenu")));
    }));
    return true;
}

void UActDemoTrainingSubsystem::RefreshTrainingInputMode()
{
    if (!bTraining || !Controller) return;
    const APawn* Pawn = Controller->GetPawn();
    const auto* LockOn = Pawn ? Pawn->FindComponentByClass<UActDemoLockOnComponent>() : nullptr;
    // 拖动期间优先保留UI鼠标；普通锁定战斗切为游戏输入，解除锁定恢复工具栏交互。
    const bool bWantsCombat = !bPlacing && LockOn && LockOn->IsLockedOn();
    if (bTrainingInputInitialized && bCombatInput == bWantsCombat) return;
    bTrainingInputInitialized = true;
    bCombatInput = bWantsCombat;
    Controller->bShowMouseCursor = !bCombatInput;
    if (bCombatInput)
        UWidgetBlueprintLibrary::SetInputMode_GameOnly(Controller, false);
    else
        UWidgetBlueprintLibrary::SetInputMode_GameAndUIEx(Controller, nullptr, EMouseLockMode::DoNotLock, false);
}

void UActDemoTrainingSubsystem::BeginDummyPlacement()
{
    if (!bTraining || !Controller || bPlacing) return;
    bPlacing = true;
    RefreshTrainingInputMode();
    Controller->SetIgnoreLookInput(true);
    Controller->SetIgnoreMoveInput(true);
    if (APawn* Pawn = Controller->GetPawn()) Pawn->DisableInput(Controller);
}

void UActDemoTrainingSubsystem::CancelDummyPlacement()
{
    if (!bPlacing) return;
    bPlacing = false;
    if (Controller)
    {
        Controller->SetIgnoreLookInput(false);
        Controller->SetIgnoreMoveInput(false);
        if (APawn* Pawn = Controller->GetPawn()) Pawn->EnableInput(Controller);
    }
    RefreshTrainingInputMode();
}

void UActDemoTrainingSubsystem::Tick(float DeltaTime)
{
    if (!Panel) InitializePanel();
    else RefreshTrainingInputMode();
}

bool UActDemoTrainingSubsystem::TryPlaceDummyAtScreenPosition(FVector2D ScreenPosition, bool bActiveEnemy)
{
    const TSubclassOf<ACharacter> SpawnClass = bActiveEnemy ? ActiveEnemyClass : DummyClass;
    if (!bTraining || !bPlacing || !Controller || !SpawnClass || !Controller->GetPawn()) return false;
    // 将桌面绝对坐标转换为视口像素，兼容独立窗口与DPI缩放。
    FVector2D Pixel, Viewport;
    USlateBlueprintLibrary::AbsoluteToViewport(this, ScreenPosition, Pixel, Viewport);
    FVector Origin, Direction;
    if (!Controller->DeprojectScreenPositionToWorld(Pixel.X, Pixel.Y, Origin, Direction)) return false;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(TrainingDummyDrop), false);
    Query.AddIgnoredActor(Controller->GetPawn());
    for (TActorIterator<AACTDEMOEnemyCharacter> It(GetWorld()); It; ++It) Query.AddIgnoredActor(*It);
    FHitResult Hit;
    if (!GetWorld()->LineTraceSingleByChannel(Hit, Origin, Origin+Direction*100000.f, ECC_Visibility, Query) ||
        Hit.ImpactNormal.Z < 0.7f) return false;
    ACharacter* Defaults = SpawnClass->GetDefaultObject<ACharacter>();
    const float HalfHeight = Defaults->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const float Radius = Defaults->GetCapsuleComponent()->GetScaledCapsuleRadius();
    const FVector Position = Hit.ImpactPoint + FVector(0,0,HalfHeight+3.f);
    FCollisionQueryParams OverlapQuery;
    if (FVector::Dist(Controller->GetPawn()->GetActorLocation(), Position) > 3000.f ||
        GetWorld()->OverlapBlockingTestByProfile(Position, FQuat::Identity,
            Defaults->GetCapsuleComponent()->GetCollisionProfileName(),
            FCollisionShape::MakeCapsule(Radius, HalfHeight), OverlapQuery)) return false;
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
    const float Yaw = (Controller->GetPawn()->GetActorLocation()-Position).Rotation().Yaw;
    ACharacter* Spawned = GetWorld()->SpawnActor<ACharacter>(SpawnClass, Position, FRotator(0,Yaw,0), Params);
    // 部分旧敌人蓝图只对关卡中预放置的角色自动占有，动态生成时补齐AI控制器。
    if (Spawned && bActiveEnemy && !Spawned->GetController())
        Spawned->SpawnDefaultController();
    return Spawned != nullptr;
}

void UActDemoTrainingSubsystem::Deinitialize()
{
    if (ExitInputProcessor.IsValid() && FSlateApplication::IsInitialized())
        FSlateApplication::Get().UnregisterInputPreProcessor(ExitInputProcessor);
    ExitInputProcessor.Reset();
    CancelDummyPlacement();
    if (Panel) Panel->RemoveFromParent();
    Panel = nullptr;
    Controller = nullptr;
    bTraining = false;
    bMenu = false;
    bTrainingInputInitialized = false;
    Super::Deinitialize();
}
