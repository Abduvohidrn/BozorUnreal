using UnrealBuildTool;

public class BozorUnrealTarget : TargetRules
{
	public BozorUnrealTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("BozorUnreal");
	}
}
