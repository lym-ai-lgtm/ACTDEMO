#include "GAS/ActDemoAirSlashCooldownEffect.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "NativeGameplayTags.h"

// 【统一状态】技能冷却校验与 UI 共同读取此标签，不维护两套倒计时。
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AirSlashCooldown, "Cooldown.Skill.AirSlash");

UActDemoAirSlashCooldownEffect::UActDemoAirSlashCooldownEffect()
{
    // 【2 秒冷却】发动时由 CommitAbility 施加，取消动画不会移除该独立效果。
    DurationPolicy = EGameplayEffectDurationType::HasDuration;
    DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(2.f));

    // 【构造期子对象】使用固定名称创建默认子对象，不能在构造函数中调用
    // FindOrAddComponent：其内部 NewObject(NAME_None) 会导致编辑器启动崩溃。
    UTargetTagsGameplayEffectComponent* TargetTags =
        CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("AirSlashCooldownTargetTags"));
    GEComponents.Add(TargetTags);

    // 【UE5 GE 组件】向效果持有者授予冷却标签；到期时 GAS 自动移除。
    FInheritedTagContainer Tags;
    Tags.Added.AddTag(TAG_AirSlashCooldown);
    TargetTags->SetAndApplyTargetTagChanges(Tags);
}
