using UnrealBuildTool;

public class BozorUnreal : ModuleRules
{
	public BozorUnreal(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		CppStandard = CppStandardVersion.Cpp20;
		PublicDependencyModuleNames.AddRange(new string[] {
			"Core", "CoreUObject", "Engine", "InputCore", "AIModule", "NavigationSystem",
			"AnimationCore", "GameplayTasks", "PhysicsCore", "UMG", "EnhancedInput", "Json"
		});
		// mikrofon (UBozorQuloq): AudioCapture plagini Windows uchun WASAPI qismini ulaydi
		PrivateDependencyModuleNames.AddRange(new string[] { "AudioCaptureCore", "AudioCapture" });

		// lokal STT: o'yin bilan birga tarqatiladi (AI/stt/yukla.sh tayyorlaydi; git'ga kirmaydi)
		foreach (string Fayl in new string[] { "crispasr.exe", "openblas.dll", "stt-uz.gguf", "LICENSE", "THIRD_PARTY_NOTICES.txt" })
		{
			RuntimeDependencies.Add("$(ProjectDir)/AI/stt/" + Fayl, StagedFileType.NonUFS);
		}
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}
	}
}
