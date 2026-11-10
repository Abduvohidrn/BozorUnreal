using UnrealBuildTool;

public class BozorUnrealEditorTarget : TargetRules
{
	public BozorUnrealEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("BozorUnreal");
	}
}
