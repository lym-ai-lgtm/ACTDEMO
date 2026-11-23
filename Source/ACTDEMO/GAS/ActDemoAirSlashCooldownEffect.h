#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ActDemoAirSlashCooldownEffect.generated.h"

/** E 技能的独立冷却效果；时长与标签由 GAS 管理，不依赖动画完成。 */
UCLASS(Blueprintable)
class ACTDEMO_API UActDemoAirSlashCooldownEffect : public UGameplayEffect
{
    GENERATED_BODY()
public:
    UActDemoAirSlashCooldownEffect();
};
