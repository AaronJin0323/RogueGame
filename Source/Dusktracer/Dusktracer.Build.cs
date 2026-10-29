using System;
using System.Collections.Generic;
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
		string ProjectDir = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", ".."));
		PublicDefinitions.Add("DT_BUILD_HASH=\"" + GetBuildHash(ProjectDir) + "\"");
		AddGitHeadDependencies(ProjectDir);
	}

	// UBT caches this rules file in its makefile; without these the hash would stay stale after a commit or checkout.
	// HEAD (checkout), logs/HEAD (every commit/reset in this worktree) and the branch ref re-run the rules when they move.
	// Only existing files are added: a missing one would invalidate the makefile on every build.
	private void AddGitHeadDependencies(string ProjectDir)
	{
		try
		{
			string GitDir = RunGit(ProjectDir, "rev-parse --absolute-git-dir");
			if (string.IsNullOrEmpty(GitDir))
			{
				return;
			}
			List<string> Candidates = new List<string> { Path.Combine(GitDir, "HEAD"), Path.Combine(GitDir, "logs", "HEAD") };
			string CommonDir = RunGit(ProjectDir, "rev-parse --git-common-dir");
			string Ref = RunGit(ProjectDir, "symbolic-ref -q HEAD");
			if (!string.IsNullOrEmpty(CommonDir) && !string.IsNullOrEmpty(Ref))
			{
				Candidates.Add(Path.GetFullPath(Path.Combine(ProjectDir, CommonDir, Ref)));
			}
			foreach (string Candidate in Candidates)
			{
				if (File.Exists(Candidate))
				{
					ExternalDependencies.Add(Path.GetFullPath(Candidate));
				}
			}
		}
		catch (Exception)
		{
			// Best effort: a stale stamp is better than a failed build.
		}
	}

	private static string GetBuildHash(string ProjectDir)
	{
		try
		{
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
			// No optional locks: a build must not rewrite the index under a parallel git command.
			string Status = RunGit(ProjectDir, "--no-optional-locks status --porcelain");
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
