// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class PathfinderOtherEarth : ModuleRules
{
	public PathfinderOtherEarth(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "Niagara", "DeveloperSettings", "UMG" });
		PublicIncludePaths.Add(ModuleDirectory);

		// AIModule = EQS + AI controllers, NavigationSystem = navmesh queries (spawn points snap onto the navmesh).
		PrivateDependencyModuleNames.AddRange(new string[] { "GameplayAbilities", "GameplayTasks", "GameplayTags", "AIModule", "NavigationSystem" });

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
