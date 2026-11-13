// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class Spade_mystery : ModuleRules
{
	public Spade_mystery(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "DeveloperSettings" });

		// Local LLM sidecar (Source/Spade_mystery/Llm): HTTP to llama-server, JSON, port probing, debug chat window.
		PrivateDependencyModuleNames.AddRange(new string[] { "HTTP", "Json", "Sockets", "Slate", "SlateCore" });

		// Stage llama-server and the models next to the packaged game (outside the .pak).
		// Fetched by Tools/fetch-models.ps1; skipped if not present so the build never fails because of them.
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			string ProjectDir = Target.ProjectFile.Directory.FullName;
			foreach (string Rel in new string[] { "ThirdParty/llama/b12368-vulkan", "ThirdParty/llama/b12368-cpu", "Models" })
			{
				string Dir = Path.Combine(ProjectDir, Rel);
				if (!Directory.Exists(Dir))
				{
					continue;
				}
				foreach (string FilePath in Directory.GetFiles(Dir, "*", SearchOption.AllDirectories))
				{
					if (FilePath.EndsWith(".part"))
					{
						continue;
					}
					string RelFile = Path.GetRelativePath(ProjectDir, FilePath).Replace('\\', '/');
					RuntimeDependencies.Add("$(ProjectDir)/" + RelFile, StagedFileType.NonUFS);
				}
			}
		}
	}
}
