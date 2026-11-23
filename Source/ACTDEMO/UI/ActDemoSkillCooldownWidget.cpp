#include "UI/ActDemoSkillCooldownWidget.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "GameFramework/Pawn.h"

// 【数据来源】每次读取当前本地玩家 ASC，避免重生/换 Pawn 后仍引用旧角色。
float UActDemoSkillCooldownWidget::GetCooldownRemaining() const
{
    APawn* Pawn = GetOwningPlayerPawn();
    UAbilitySystemComponent* ASC = Pawn ? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Pawn) : nullptr;
    const FGameplayTag Tag = CooldownTag.IsValid() ? CooldownTag :
        FGameplayTag::RequestGameplayTag(FName(TEXT("Cooldown.Skill.AirSlash")), false);
    if (!ASC || !Tag.IsValid()) return 0.f;

    // 【剩余时长】直接查询有效 GE；即使收招被取消也与真正的冷却保持一致。
    const FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(Tag));
    const TArray<float> Times = ASC->GetActiveEffectsTimeRemaining(Query);
    float Remaining = 0.f;
    for (float Time : Times) Remaining = FMath::Max(Remaining, Time);
    return Remaining;
}

void UActDemoSkillCooldownWidget::NativeConstruct()
{
    Super::NativeConstruct();
    RefreshCooldownDisplay();
}

void UActDemoSkillCooldownWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    RefreshCooldownDisplay();
}

// 【视觉更新】只修改图标明暗、倒计时文本；不覆盖设计器的位置、尺寸、图片与颜色。
void UActDemoSkillCooldownWidget::RefreshCooldownDisplay()
{
    const float Remaining = GetCooldownRemaining();
    const bool bCooling = Remaining > 0.f;
    if (SkillIcon) SkillIcon->SetRenderOpacity(bCooling ? CoolingOpacity : ReadyOpacity);
    if (CooldownText)
    {
        CooldownText->SetText(bCooling ? FText::FromString(FString::Printf(TEXT("%.1f"),
            FMath::Max(0.1f, Remaining))) : FText::GetEmpty());
        CooldownText->SetVisibility(bCooling ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }
}
