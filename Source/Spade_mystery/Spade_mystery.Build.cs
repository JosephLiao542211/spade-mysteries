// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Spade_mystery : ModuleRules
{
	public Spade_mystery(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "DeveloperSettings", "AnimationCore", "ChaosClothAssetEngine" });
	}
}
