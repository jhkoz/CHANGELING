// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CHANGELING : ModuleRules
{
	public CHANGELING(ReadOnlyTargetRules Target) : base(Target)
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
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"CHANGELING",
			"CHANGELING/Variant_Platforming",
			"CHANGELING/Variant_Platforming/Animation",
			"CHANGELING/Variant_Combat",
			"CHANGELING/Variant_Combat/AI",
			"CHANGELING/Variant_Combat/Animation",
			"CHANGELING/Variant_Combat/Gameplay",
			"CHANGELING/Variant_Combat/Interfaces",
			"CHANGELING/Variant_Combat/UI",
			"CHANGELING/Variant_SideScrolling",
			"CHANGELING/Variant_SideScrolling/AI",
			"CHANGELING/Variant_SideScrolling/Gameplay",
			"CHANGELING/Variant_SideScrolling/Interfaces",
			"CHANGELING/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
