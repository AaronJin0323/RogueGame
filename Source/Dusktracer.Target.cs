using UnrealBuildTool;

public class DusktracerTarget : TargetRules
{
	public DusktracerTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("Dusktracer");
	}
}
