#pragma once

#include "CoreMinimal.h"

class AActor;

/** 所有玩家攻击共用的朝向规则，不负责动画、追击位移或伤害。 */
namespace ActDemoAttackFacing
{
    // 【每段出招】锁定有效存活目标时只对齐水平朝向；未锁定或方向退化时保持原朝向。
    void FaceLockedTarget(AActor* Attacker);
}
