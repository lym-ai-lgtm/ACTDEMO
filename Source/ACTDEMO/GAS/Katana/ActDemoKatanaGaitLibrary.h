#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "ActDemoKatanaGaitLibrary.generated.h"

class UAnimSequence;
class UBlendSpace;

UCLASS()
class ACTDEMO_API UActDemoKatanaGaitLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// 编辑器内按各方向样本的根位移校准播放速度。速度轴必须为 Y，单位 cm/s。
	// 仅编辑与混合空间同目录的太刀动画副本。若重定向后根轨迹缺失，可传入原始循环。
	UFUNCTION(BlueprintCallable, Category = "Katana|Animation", meta = (DevelopmentOnly))
	static bool CalibrateKatanaGait(
		UBlendSpace* BlendSpace,
		const TMap<UAnimSequence*, UAnimSequence*>& SourceOverrides,
		TArray<FString>& Errors);
};
