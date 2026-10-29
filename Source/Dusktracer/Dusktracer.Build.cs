using System;
using System.Diagnostics;
using System.IO;
using UnrealBuildTool;

public class Dusktracer : ModuleRules
{
	public Dusktracer(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Subfolders (Core/, Combat/, ...) are include roots: #include "Combat/DTBulletSubsystem.h".
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"GameplayTags"
		});

		// Build stamp for telemetry (telemetry.md): short git hash of the project, "+dirty" with local changes,
		// "unknown" when git is missing or fails. Never fails the build. It changes every commit, which rebuilds
		// this module's PCH and every file in it.
		PublicDefinitions.Add("DT_BUILD_HASH=\"" + GetBuildHash() + "\"");
	}

	private string GetBuildHash()
	{
		try
		{
			string ProjectDir = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", ".."));
			string Hash = RunGit(ProjectDir, "rev-parse --short HEAD");
			if (string.IsNullOrEmpty(Hash))
			{
				return "unknown";
			}
			foreach (char Ch in Hash)
			{
				if (!Uri.IsHexDigit(Ch))
				{
					return "unknown";
				}
			}
			string Status = RunGit(ProjectDir, "status --porcelain");
			return !string.IsNullOrEmpty(Status) ? Hash + "+dirty" : Hash;
		}
		catch (Exception)
		{
			return "unknown";
		}
	}

	// Trimmed stdout of git, or null when git cannot start, times out or exits non-zero.
	private static string RunGit(string WorkingDir, string Arguments)
	{
		ProcessStartInfo Info = new ProcessStartInfo("git", Arguments)
		{
			WorkingDirectory = WorkingDir,
			RedirectStandardOutput = true,
			RedirectStandardError = true,
			UseShellExecute = false,
			CreateNoWindow = true
		};
		using (Process Git = Process.Start(Info))
		{
			if (Git == null)
			{
				return null;
			}
			System.Threading.Tasks.Task<string> Err = Git.StandardError.ReadToEndAsync();
			string Out = Git.StandardOutput.ReadToEnd();
			if (!Git.WaitForExit(10000))
			{
				try { Git.Kill(); } catch (Exception) { }
				return null;
			}
			Err.Wait();
			return Git.ExitCode == 0 ? Out.Trim() : null;
		}
	}
}
