using UnrealBuildTool;

public class DusktracerEditorTarget : TargetRules
{
	public DusktracerEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("Dusktracer");
	}
}
