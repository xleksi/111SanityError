// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class DialoguePluginEditor : ModuleRules
	{
        public DialoguePluginEditor(ReadOnlyTargetRules Target) : base(Target)
        {
            PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

            PublicIncludePaths.AddRange(new string[] 
			{ 
				System.IO.Path.Combine(ModuleDirectory, "Public")
			});

            //var EngineDir = Path.GetFullPath(Target.RelativeEnginePath);
            PrivateIncludePaths.AddRange(new string[] 
			{ 
				System.IO.Path.Combine(ModuleDirectory, "Private"),
                //Path.Combine(EngineDir, @"Source/Editor/DetailCustomizations/Private"),
            });

			/*
			 * Explanation of Public/Private dependency module names:
			 * If you're using the same source code layout as we use internally (Source/Public and Source/Private), 
			 * then PublicDependencyModuleNames will be available in the Public and Private folders, 
			 * but PrivateDependencyModuleNames will only be available in the Private folder.
			 * TODO: clean up
			 */
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
                    "EditorStyle",
					"Engine",
                    "InputCore",
					"LevelEditor",
					"Slate",
                    "AssetTools",
                    "KismetWidgets",
                    "WorkspaceMenuStructure",
                    "Projects",
                    "GraphEditor",
                    "AnimGraph"
                }
			);

            PrivateDependencyModuleNames.AddRange(
				new string[]
				{					
					"PropertyEditor",
					"SlateCore",
                    "ApplicationCore",
                    "UnrealEd",
                    "Json",
                    "JsonUtilities",
					"ToolWidgets",
					"EditorWidgets",
					"DialoguePlugin"
				}
			);

			// For ChatGPT integration
            PrivateDependencyModuleNames.AddRange(
				new string[] 
				{
                    "HTTP",
                    "Json", 
					"JsonUtilities",
                    "Blutility",
                    "EditorSubsystem"
                }
			);

            PrivateIncludePathModuleNames.AddRange(
				new string[] 
				{
					"MainFrame",
					"Settings"
				}
			);

			
		}
	}
}