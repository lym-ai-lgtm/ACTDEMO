#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "ActDemoSkillCooldownWidget.generated.h"

class UImage;
class UTextBlock;

/** 技能 UI 数据读取。布局/图标在 Widget 蓝图里设计，C++ 不创建控件或固定位置。 */
UCLASS(Abstract, Blueprintable)
class ACTDEMO_API UActDemoSkillCooldownWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    // 【蓝图接口】可供自定义材质、遮罩等显示读取，不参与技能冷却计算。
    UFUNCTION(BlueprintPure, Category="Skill Cooldown")
    float GetCooldownRemaining() const;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    // 【可设计控件】Widget 蓝图中使用这些控件名，可选绑定，不强制布局层级。
    UPROPERTY(meta=(BindWidgetOptional))
    TObjectPtr<UImage> SkillIcon;

    UPROPERTY(meta=(BindWidgetOptional))
    TObjectPtr<UTextBlock> CooldownText;

    // 【显示配置】未指定标签时读取 E 技能冷却；以后其它技能可复用此 UI 类。
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill Cooldown")
    FGameplayTag CooldownTag;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill Cooldown", meta=(ClampMin="0.0", ClampMax="1.0"))
    float CoolingOpacity = 0.3f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill Cooldown", meta=(ClampMin="0.0", ClampMax="1.0"))
    float ReadyOpacity = 1.f;

private:
    void RefreshCooldownDisplay();
};
