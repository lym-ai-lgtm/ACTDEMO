// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ACTDEMO : ModuleRules
{
	public ACTDEMO(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
            "SlateCore",
            "GameplayAbilities",
            "GameplayTags",
            "GameplayTasks"
        });

		PrivateDependencyModuleNames.Add("Niagara");

		PublicIncludePaths.AddRange(new string[] {
			"ACTDEMO",
			"ACTDEMO/Variant_Platforming",
			"ACTDEMO/Variant_Platforming/Animation",
			"ACTDEMO/Variant_Combat",
			"ACTDEMO/Variant_Combat/AI",
			"ACTDEMO/Variant_Combat/Animation",
			"ACTDEMO/Variant_Combat/Gameplay",
			"ACTDEMO/Variant_Combat/Interfaces",
			"ACTDEMO/Variant_Combat/UI",
			"ACTDEMO/Variant_SideScrolling",
			"ACTDEMO/Variant_SideScrolling/AI",
			"ACTDEMO/Variant_SideScrolling/Gameplay",
			"ACTDEMO/Variant_SideScrolling/Interfaces",
			"ACTDEMO/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
