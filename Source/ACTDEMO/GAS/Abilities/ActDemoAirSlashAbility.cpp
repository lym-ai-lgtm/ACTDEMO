#include "GAS/Abilities/ActDemoAirSlashAbility.h"
#include "Combat/ActDemoAttackFacing.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GAS/ACTDEMOGASCharacter.h"
#include "GAS/ACTDEMOEnemyCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GAS/ActDemoAirSlashCooldownEffect.h"

// 【实例策略】单机 Demo 的服务器能力；按住 E 不重启，R 追击通过标签取消此能力。
UActDemoAirSlashAbility::UActDemoAirSlashAbility()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
    bRetriggerInstancedAbility = false;
    // 【技能冷却】复用 GAS 的 CheckCooldown/CommitAbility，不写动画计时器。
    CooldownGameplayEffectClass = UActDemoAirSlashCooldownEffect::StaticClass();
}

// 【发动限制】GAS 负责状态互斥；本类额外要求存活、配置伤害效果并位于地面。
bool UActDemoAirSlashAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
    const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
    if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
        return false;
    const AActDemoGASCharacter* Character = ActorInfo
        ? Cast<AActDemoGASCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
    return Character && Character->GetCurrentHealth() > 0.f && DamageEffectClass &&
        Character->GetCharacterMovement()->IsMovingOnGround();
}

// 【能力入口】只提交一次、重置命中状态；调用基类将动画流程交给蓝图 Event ActivateAbility。
void UActDemoAirSlashAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }
    // 【统一攻击朝向】成功提交技能后、蓝图播放动画前对准，发动失败不会改变朝向。
    ActDemoAttackFacing::FaceLockedTarget(GetAvatarActorFromActorInfo());
    bHitProcessed = false;
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

// 【命中检测】一条命中通知对应一次球形扫掠；不生成剑气或远程投射物。
int32 UActDemoAirSlashAbility::ProcessAirSlashHit()
{
    AActDemoGASCharacter* Attacker = Cast<AActDemoGASCharacter>(GetAvatarActorFromActorInfo());
    UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
    if (!IsActive() || bHitProcessed || !IsValid(Attacker) || !Attacker->HasAuthority() ||
        Attacker->GetCurrentHealth() <= 0.f || !SourceASC || !DamageEffectClass) return 0;

    bHitProcessed = true;
    const FVector Start = Attacker->GetActorLocation();
    const FVector End = Start + Attacker->GetActorForwardVector() * FMath::Max(1.f, AttackDistance);
    TArray<TEnumAsByte<EObjectTypeQuery>> Types;
    Types.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));
    TArray<AActor*> Ignore;
    Ignore.Add(Attacker);
    TArray<FHitResult> Hits;
    UKismetSystemLibrary::SphereTraceMultiForObjects(Attacker, Start, End,
        FMath::Max(1.f, AttackRadius), Types, false, Ignore, EDrawDebugTrace::None, Hits, true);

    // 【目标去重】仅对存活敌人结算；网格和胶囊重复命中不重复扣血。
    TSet<AActor*> Processed;
    int32 DamagedCount = 0;
    for (const FHitResult& Hit : Hits)
    {
        AACTDEMOEnemyCharacter* Enemy = Cast<AACTDEMOEnemyCharacter>(Hit.GetActor());
        if (!IsValid(Enemy) || Enemy->GetCurrentHealth() <= 0.f || Processed.Contains(Enemy)) continue;
        Processed.Add(Enemy);
        UAbilitySystemComponent* TargetASC = Enemy->GetAbilitySystemComponent();
        if (!TargetASC) continue;

        // 【GAS 伤害】保留攻击来源、命中信息；无敌或其它 GE 规则导致未扣血时不挑飞。
        FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
        Context.AddSourceObject(Attacker);
        Context.AddHitResult(Hit);
        const FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(DamageEffectClass,
            GetAbilityLevel(), Context);
        if (!Spec.IsValid()) continue;
        const float Before = Enemy->GetCurrentHealth();
        SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
        const float After = Enemy->GetCurrentHealth();
        if (After >= Before) continue;
        ++DamagedCount;

        // 【挑飞与 R 接力】复用 LaunchFromAttack，令同一敌人进入 IsLaunched/Airborne。
        // R 的既有目标判断无需新增分支；追击蓝图另添加取消本技能的标签。
        FGameplayTag Feedback = After <= 0.f ? DeathEventTag : HitReactEventTag;
        if (After > 0.f && LaunchReactEventTag.IsValid() &&
            Enemy->LaunchFromAttack(Attacker, LaunchUpwardSpeed, LaunchForwardSpeed))
            Feedback = LaunchReactEventTag;

        // 【蓝图表现】致死优先，不对尸体挑飞，不在 C++ 控制动画播放或播放速率。
        if (Feedback.IsValid())
        {
            FGameplayEventData Event;
            Event.EventTag = Feedback;
            Event.Instigator = Attacker;
            Event.Target = Enemy;
            Event.EventMagnitude = Before - After;
            Event.ContextHandle = Context;
            UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Enemy, Feedback, Event);
        }
    }
    return DamagedCount;
}
