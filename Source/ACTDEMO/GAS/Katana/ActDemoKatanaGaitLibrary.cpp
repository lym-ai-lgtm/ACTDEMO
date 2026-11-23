#include "GAS/Katana/ActDemoKatanaGaitLibrary.h"

#if WITH_EDITOR
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Misc/PackageName.h"
#endif

bool UActDemoKatanaGaitLibrary::CalibrateKatanaGait(
	UBlendSpace* BlendSpace,
	const TMap<UAnimSequence*, UAnimSequence*>& SourceOverrides,
	TArray<FString>& Errors)
{
	Errors.Reset();
#if WITH_EDITOR
	if (!IsValid(BlendSpace))
	{
		Errors.Add(TEXT("Select a Katana locomotion Blend Space."));
		return false;
	}

	const FString Folder = FPackageName::GetLongPackagePath(BlendSpace->GetOutermost()->GetName());
	TMap<UAnimSequence*, float> NewRates;
	for (const FBlendSample& Sample : BlendSpace->GetBlendSamples())
	{
		const double DesiredSpeed = Sample.SampleValue.Y;
		if (DesiredSpeed <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		UAnimSequence* Animation = Sample.Animation.Get();
		if (!IsValid(Animation) || !Animation->bForceRootLock || Animation->HasRootMotion() ||
			FPackageName::GetLongPackagePath(Animation->GetOutermost()->GetName()) != Folder)
		{
			Errors.Add(FString::Printf(TEXT("%s must be an in-place animation in the Blend Space folder."), *GetNameSafe(Animation)));
			continue;
		}

		UAnimSequence* const* Override = SourceOverrides.Find(Animation);
		const UAnimSequence* Source = Override ? *Override : Animation;
		const double Length = IsValid(Source) ? Source->GetPlayLength() : 0.0;
		if (Length <= KINDA_SMALL_NUMBER || Sample.RateScale <= KINDA_SMALL_NUMBER)
		{
			Errors.Add(FString::Printf(TEXT("%s has an invalid source length or sample rate."), *Animation->GetName()));
			continue;
		}

		const FVector First = Source->ExtractRootTrackTransform(FAnimExtractContext(0.0), nullptr).GetTranslation();
		const FVector Last = Source->ExtractRootTrackTransform(FAnimExtractContext(Length), nullptr).GetTranslation();
		const double Travel = (Last - First).Size2D();
		if (!FMath::IsFinite(Travel) || Travel < 1.0)
		{
			Errors.Add(FString::Printf(TEXT("%s has no usable root displacement. Assign the original moving sequence as its source."), *Animation->GetName()));
			continue;
		}

		// 混合空间样本倍率和动画资产倍率相乘。用目标动画的时长匹配保留的步幅。
		const float Rate = static_cast<float>(DesiredSpeed * Animation->GetPlayLength() / (Travel * Sample.RateScale));
		if (!FMath::IsFinite(Rate) || Rate <= KINDA_SMALL_NUMBER)
		{
			Errors.Add(FString::Printf(TEXT("%s produces an invalid gait rate."), *Animation->GetName()));
			continue;
		}
		if (const float* Previous = NewRates.Find(Animation); Previous && !FMath::IsNearlyEqual(*Previous, Rate))
		{
			Errors.Add(FString::Printf(TEXT("%s is used at conflicting sample speeds."), *Animation->GetName()));
			continue;
		}
		NewRates.Add(Animation, Rate);
	}

	if (NewRates.IsEmpty() && Errors.IsEmpty())
	{
		Errors.Add(TEXT("The Blend Space has no moving samples."));
	}
	if (!Errors.IsEmpty())
	{
		return false;
	}

	for (const TPair<UAnimSequence*, float>& Entry : NewRates)
	{
		if (!FMath::IsNearlyEqual(Entry.Key->RateScale, Entry.Value))
		{
			Entry.Key->Modify();
			Entry.Key->RateScale = Entry.Value;
			Entry.Key->MarkPackageDirty();
		}
	}
	return true;
#else
	Errors.Add(TEXT("Katana gait calibration is available in the editor."));
	return false;
#endif
}
