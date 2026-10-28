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
	}
}
