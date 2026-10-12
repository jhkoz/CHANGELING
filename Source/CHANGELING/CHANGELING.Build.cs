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
			"NavigationSystem",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
			"SlateCore",
			// Runtime module (checked: Type "Runtime" in MetaHumanCharacter.uplugin, so it
			// packages). Needed so AChangelingCharacterBase can implement
			// IMetaHumanCharacterActorInterface -- the MetaHuman assembly pipeline will only
			// generate a Blueprint on top of a class that implements it.
			"MetaHumanCharacterPalette",
			// UGroomComponent, so AChangelingNPC can list a MetaHuman's grooms with its
			// LOD sync. The HairStrands plugin is already enabled -- MetaHumanCharacter
			// depends on it -- so this adds a module reference, not a plugin.
			"HairStrandsCore",
			"SunPosition",
			"Niagara",
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",
			"PhysicsCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		// Head/body composition needs MetaHumanCharacterEditor, whose module Type is
		// "Editor" -- linking it unconditionally would break a packaged build. Everything
		// that uses it is inside #if WITH_EDITOR, and a shipped game has no business
		// conforming a face to a body anyway.
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] {
				"MetaHumanCharacter",
				"MetaHumanCharacterEditor",
				"UnrealEd",
				"AssetTools",
			});
		}

		PublicIncludePaths.AddRange(new string[] {
			"CHANGELING",
			"CHANGELING/AI",
			"CHANGELING/Audio",
			"CHANGELING/Water",
			"CHANGELING/Environment",
			"CHANGELING/GAS",
			"CHANGELING/Items",
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
