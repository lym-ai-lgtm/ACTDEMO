#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ActDemoTrainingPanelWidget.generated.h"

/** 设计器提供图标；本类只处理标准UMG拖放，不轮询鼠标按键。 */
UCLASS()
class ACTDEMO_API UActDemoTrainingPanelWidget : public UUserWidget
{
    GENERATED_BODY()
protected:
    virtual void NativeOnInitialized() override;
    virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry&, const FPointerEvent&) override;
    virtual void NativeOnDragDetected(const FGeometry&, const FPointerEvent&, UDragDropOperation*&) override;
    virtual bool NativeOnDrop(const FGeometry&, const FDragDropEvent&, UDragDropOperation*) override;
    virtual void NativeOnDragCancelled(const FDragDropEvent&, UDragDropOperation*) override;
private:
    UFUNCTION() void ShowDummyCard();
    UFUNCTION() void ShowActiveEnemyCard();
    bool bDraggingActiveEnemy = false;
    void FinishDrag();
};
