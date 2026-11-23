#include "UI/ActDemoHealthDisplay.h"
#include "GAS/ACTDEMOGASCharacter.h"
#include "GAS/ACTDEMOEnemyCharacter.h"
#include "GAS/ActDemoAttributeSet.h"
#include "Combat/ActDemoLockOnComponent.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"


#include "Components/WidgetComponent.h"
#include "GameFramework/PlayerController.h"

void UActDemoHealthWidget::SetHealthSource(AActDemoGASCharacter* Source)
{
    HealthSource = Source;
    RefreshHealth();
}
void UActDemoHealthWidget::RefreshHealth()
{
    const auto* Source = HealthSource.Get();
    const float Maximum = Source ? FMath::Max(0.f, Source->GetCurrentMaxHealth()) : 0.f;
    const float Current = Source ? FMath::Clamp(Source->GetCurrentHealth(),0.f,Maximum) : 0.f;
    if (HealthBar) HealthBar->SetPercent(Maximum > 0.f ? Current / Maximum : 0.f);
    if (HealthText) HealthText->SetText(FText::FromString(FString::Printf(TEXT("%.0f / %.0f"),Current,Maximum)));

    if (PostureBar)
    {
        const UAbilitySystemComponent* ASC = Source ? Source->GetAbilitySystemComponent() : nullptr;
        const float MaxPosture = ASC
            ? FMath::Max(0.f, ASC->GetNumericAttribute(UActDemoAttributeSet::GetMaxPostureAttribute()))
            : 0.f;
        const float Posture = ASC
            ? FMath::Clamp(ASC->GetNumericAttribute(UActDemoAttributeSet::GetPostureAttribute()), 0.f, MaxPosture)
            : 0.f;
        PostureBar->SetPercent(MaxPosture > 0.f ? Posture / MaxPosture : 0.f);
        PostureBar->SetVisibility(MaxPosture > 0.f ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }
}
UActDemoHealthDisplayComponent::UActDemoHealthDisplayComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = .1f;
    // 视觉资源由角色蓝图的 HealthDisplay 配置。
}
void UActDemoHealthDisplayComponent::BeginPlay()
{
    Super::BeginPlay();
    if (GetNetMode() == NM_DedicatedServer || !bShowHealthUI) { SetComponentTickEnabled(false); return; }
    auto* Character = Cast<AActDemoGASCharacter>(GetOwner());
    if (!Character) { SetComponentTickEnabled(false); return; }
    // 未显式配置类时加载项目的 Widget 蓝图，不生成任何原生布局。
    // 避免旧蓝图组件默认值在重新编译/重开后为空而静默不显示。
    if (!HealthWidgetClass)
    {
        const TCHAR* DefaultWidgetPath = Character->IsA<AACTDEMOEnemyCharacter>()
            ? TEXT("/Game/Demo/GAS/UI/WBP_EnemyHealth.WBP_EnemyHealth_C")
            : TEXT("/Game/Demo/GAS/UI/WBP_PlayerHealth.WBP_PlayerHealth_C");
        HealthWidgetClass = LoadClass<UActDemoHealthWidget>(nullptr, DefaultWidgetPath);
    }
    if (!HealthWidgetClass)
    {
        UE_LOG(LogTemp, Error, TEXT("Health UI: cannot load Widget Blueprint for %s. Configure HealthWidgetClass on HealthDisplay."), *GetNameSafe(Character));
        SetComponentTickEnabled(false);
        return;
    }
    if (Character->IsA<AACTDEMOEnemyCharacter>())
    {
        EnemyBar = NewObject<UWidgetComponent>(Character);
        Character->AddInstanceComponent(EnemyBar);
        EnemyBar->SetupAttachment(Character->GetRootComponent());
        EnemyBar->SetRelativeLocation(FVector(0,0,EnemyBarHeight));
        EnemyBar->SetWidgetSpace(EWidgetSpace::Screen);
        EnemyBar->SetDrawSize(EnemyBarSize);
        EnemyBar->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        EnemyBar->SetWidgetClass(HealthWidgetClass);
        EnemyBar->SetVisibility(false);
        EnemyBar->RegisterComponent();
        EnemyBar->InitWidget();
        if (auto* Widget = Cast<UActDemoHealthWidget>(EnemyBar->GetUserWidgetObject())) Widget->SetHealthSource(Character);
        ObservedASC = Character->GetAbilitySystemComponent();
        if (UAbilitySystemComponent* ASC = ObservedASC.Get())
        {
            HealthChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(
                UActDemoAttributeSet::GetHealthAttribute()).AddUObject(
                    this, &UActDemoHealthDisplayComponent::OnEnemyHealthChanged);
            PostureChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(
                UActDemoAttributeSet::GetPostureAttribute()).AddUObject(
                    this, &UActDemoHealthDisplayComponent::OnEnemyPostureChanged);
        }
    }
}
void UActDemoHealthDisplayComponent::OnEnemyHealthChanged(const FOnAttributeChangeData& Data)
{
    // 只有实际掉血才刷新计时；治疗和被无敌拦截的攻击不触发显示。
    if (Data.NewValue < Data.OldValue && GetWorld())
    {
        LastDamageTime = GetWorld()->GetTimeSeconds();
    }
}
void UActDemoHealthDisplayComponent::OnEnemyPostureChanged(const FOnAttributeChangeData& Data)
{
    // 招架攻击虽然没有扣血，也应显示敌人血条组，让玩家看到招架值下降。
    if (Data.NewValue < Data.OldValue && GetWorld())
    {
        LastDamageTime = GetWorld()->GetTimeSeconds();
    }
}
void UActDemoHealthDisplayComponent::TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* TickFunction)
{
    Super::TickComponent(DeltaTime,TickType,TickFunction);
    auto* Character = Cast<AActDemoGASCharacter>(GetOwner());
    if (!Character) return;
    if (EnemyBar)
    {
        const double Now = GetWorld()->GetTimeSeconds();
        const bool bRecentlyDamaged = Now - LastDamageTime < FMath::Max(0.f, EnemyHealthDisplayDuration);
        const APlayerController* LocalPC = GetWorld()->GetFirstPlayerController();
        const APawn* LocalPawn = LocalPC && LocalPC->IsLocalController() ? LocalPC->GetPawn() : nullptr;
        const UActDemoLockOnComponent* LockOn = LocalPawn
            ? LocalPawn->FindComponentByClass<UActDemoLockOnComponent>() : nullptr;
        const bool bLockedByPlayer = LockOn && LockOn->GetLockedTarget() == Character;
        const auto* Enemy = Cast<AACTDEMOEnemyCharacter>(Character);
        const bool bShouldShow = !bShowEnemyOnlyWhenDamagedOrLocked || bRecentlyDamaged ||
            bLockedByPlayer || (Enemy && Enemy->IsPostureBroken());
        EnemyBar->SetVisibility(bShowHealthUI && Character->GetCurrentHealth() > 0.f && bShouldShow);
        if (auto* Widget = Cast<UActDemoHealthWidget>(EnemyBar->GetUserWidgetObject())) Widget->RefreshHealth();
        return;
    }
    auto* PC = Cast<APlayerController>(Character->GetController());
    const bool bLocalPlayer = PC && PC->IsLocalController() && Character->IsLocallyControlled();
    if (!bLocalPlayer || !bShowHealthUI)
    {
        if (PlayerHUD) { PlayerHUD->RemoveFromParent(); PlayerHUD = nullptr; }
        return;
    }
    if (!PlayerHUD && HealthWidgetClass)
    {
        PlayerHUD = CreateWidget<UActDemoHealthWidget>(PC,HealthWidgetClass);
        if (PlayerHUD)
        {
            PlayerHUD->SetHealthSource(Character);
            PlayerHUD->AddToPlayerScreen();


        }
    }
    if (PlayerHUD) PlayerHUD->RefreshHealth();
}
void UActDemoHealthDisplayComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    if (UAbilitySystemComponent* ASC = ObservedASC.Get())
    {
        ASC->GetGameplayAttributeValueChangeDelegate(
            UActDemoAttributeSet::GetHealthAttribute()).Remove(HealthChangedHandle);
        ASC->GetGameplayAttributeValueChangeDelegate(
            UActDemoAttributeSet::GetPostureAttribute()).Remove(PostureChangedHandle);
    }
    HealthChangedHandle.Reset();
    PostureChangedHandle.Reset();
    ObservedASC.Reset();
    if (PlayerHUD) { PlayerHUD->RemoveFromParent(); PlayerHUD = nullptr; }
    if (EnemyBar) { EnemyBar->DestroyComponent(); EnemyBar = nullptr; }
    Super::EndPlay(Reason);
}
