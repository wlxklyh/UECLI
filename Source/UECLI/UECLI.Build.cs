// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UECLI : ModuleRules
{
	public UECLI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		IWYUSupport = IWYUSupport.Full;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
				"Json",
				"JsonUtilities",
				"DeveloperSettings",
				"EngineSettings"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"UnrealEd",
				"EditorScriptingUtilities",
				"EditorSubsystem",
				"Slate",
				"SlateCore",
				"Projects",
				"AssetRegistry",
				"LevelEditor",
				"Sockets",
				"Networking"
			}
		);

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"PropertyEditor",
					"ToolMenus",
					"MaterialEditor",
					"TextureGraph",
					"TextureGraphEngine",
					"TextureGraphEditor",
					"RenderCore",
					"RHI"
				}
			);
		}
	}
}
