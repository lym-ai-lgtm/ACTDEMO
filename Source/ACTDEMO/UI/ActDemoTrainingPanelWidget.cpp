#include "UI/ActDemoTrainingPanelWidget.h"
#include "Blueprint/DragDropOperation.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Training/ActDemoTrainingSubsystem.h"
#include "Engine/World.h"

void UActDemoTrainingPanelWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    if (auto* Button = Cast<UButton>(GetWidgetFromName(TEXT("AddDummyButton"))))
        Button->OnClicked.AddUniqueDynamic(this, &UActDemoTrainingPanelWidget::ShowDummyCard);
    if (auto* Card = GetWidgetFromName(TEXT("DummyCard"))) Card->SetVisibility(ESlateVisibility::Collapsed);
    if (auto* Button = Cast<UButton>(GetWidgetFromName(TEXT("AddActiveEnemyButton"))))
        Button->OnClicked.AddUniqueDynamic(this, &UActDemoTrainingPanelWidget::ShowActiveEnemyCard);
    if (auto* Card = GetWidgetFromName(TEXT("ActiveEnemyCard"))) Card->SetVisibility(ESlateVisibility::Collapsed);
    if (auto* Drop = GetWidgetFromName(TEXT("PlacementDropSurface"))) Drop->SetVisibility(ESlateVisibility::Collapsed);
}

void UActDemoTrainingPanelWidget::ShowDummyCard()
{
    if (auto* Card = GetWidgetFromName(TEXT("DummyCard"))) Card->SetVisibility(ESlateVisibility::Visible);
    if (auto* Tip = Cast<UTextBlock>(GetWidgetFromName(TEXT("PlacementInstructions"))))
        Tip->SetText(FText::FromString(TEXT("将假人图标拖到场景地面后松开\n无效位置不会生成")));
}

FReply UActDemoTrainingPanelWidget::NativeOnPreviewMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
    for (const FName Name : {FName(TEXT("DummyCard")), FName(TEXT("ActiveEnemyCard"))})
    {
        auto* Card = GetWidgetFromName(Name);
        if (Card && Card->GetVisibility() != ESlateVisibility::Collapsed &&
            Card->GetCachedGeometry().IsUnderLocation(Event.GetScreenSpacePosition()))
        {
            bDraggingActiveEnemy = Name == FName(TEXT("ActiveEnemyCard"));
            return UWidgetBlueprintLibrary::DetectDragIfPressed(Event, this, EKeys::LeftMouseButton).NativeReply;
        }
    }
    return Super::NativeOnPreviewMouseButtonDown(Geometry, Event);
}

void UActDemoTrainingPanelWidget::NativeOnDragDetected(const FGeometry& Geometry, const FPointerEvent& Event, UDragDropOperation*& Operation)
{
    Operation = NewObject<UDragDropOperation>(this);
    Operation->Payload = this;
    Operation->Tag = bDraggingActiveEnemy ? TEXT("TrainingActiveEnemy") : TEXT("TrainingDummy");
    auto* Visual = NewObject<UTextBlock>(this);
    Visual->SetText(FText::FromString(bDraggingActiveEnemy ? TEXT("◈ 主动攻击敌人") : TEXT("◈ 假人")));
    FSlateFontInfo Font = Visual->GetFont(); Font.Size = 28; Visual->SetFont(Font);
    Visual->SetColorAndOpacity(FSlateColor(FLinearColor(0.25f,0.8f,1.f)));
    Operation->DefaultDragVisual = Visual;
    Operation->Pivot = EDragPivot::CenterCenter;
    if (auto* Drop = GetWidgetFromName(TEXT("PlacementDropSurface"))) Drop->SetVisibility(ESlateVisibility::Visible);
    if (auto* System = GetWorld()->GetSubsystem<UActDemoTrainingSubsystem>()) System->BeginDummyPlacement();
}

bool UActDemoTrainingPanelWidget::NativeOnDrop(const FGeometry& Geometry, const FDragDropEvent& Event, UDragDropOperation* Operation)
{
    if (!Operation || Operation->Payload != this ||
        (Operation->Tag != TEXT("TrainingDummy") && Operation->Tag != TEXT("TrainingActiveEnemy")))
        return Super::NativeOnDrop(Geometry, Event, Operation);
    // 拖回工具栏不生成；只接受工具栏外的场景位置。
    auto* Controls = GetWidgetFromName(TEXT("TrainingControls"));
    bool bCreated = false;
    if (!Controls || !Controls->GetCachedGeometry().IsUnderLocation(Event.GetScreenSpacePosition()))
        if (auto* System = GetWorld()->GetSubsystem<UActDemoTrainingSubsystem>())
            bCreated = System->TryPlaceDummyAtScreenPosition(Event.GetScreenSpacePosition(), Operation->Tag == TEXT("TrainingActiveEnemy"));
    FinishDrag();
    if (auto* Tip = Cast<UTextBlock>(GetWidgetFromName(TEXT("PlacementInstructions"))))
        Tip->SetText(FText::FromString(bCreated ? TEXT("已生成，可继续拖动添加") : TEXT("无法放置：请拖到附近的平坦空地")));
    return true;
}

void UActDemoTrainingPanelWidget::FinishDrag()
{
    if (auto* Drop = GetWidgetFromName(TEXT("PlacementDropSurface"))) Drop->SetVisibility(ESlateVisibility::Collapsed);
    if (auto* System = GetWorld()->GetSubsystem<UActDemoTrainingSubsystem>()) System->CancelDummyPlacement();
}

void UActDemoTrainingPanelWidget::ShowActiveEnemyCard()
{
    if (auto* Card = GetWidgetFromName(TEXT("ActiveEnemyCard"))) Card->SetVisibility(ESlateVisibility::Visible);
    if (auto* Tip = Cast<UTextBlock>(GetWidgetFromName(TEXT("PlacementInstructions"))))
        Tip->SetText(FText::FromString(TEXT("拖动主动敌人图标到地面\n生成后会主动追击和攻击玩家")));
}

void UActDemoTrainingPanelWidget::NativeOnDragCancelled(const FDragDropEvent& Event, UDragDropOperation* Operation)
{
    FinishDrag();
    Super::NativeOnDragCancelled(Event, Operation);
}
