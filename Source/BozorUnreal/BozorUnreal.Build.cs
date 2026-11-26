using UnrealBuildTool;

public class BozorUnreal : ModuleRules
{
	public BozorUnreal(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		CppStandard = CppStandardVersion.Cpp20;
		PublicDependencyModuleNames.AddRange(new string[] {
			"Core", "CoreUObject", "Engine", "InputCore", "AIModule", "NavigationSystem",
			"AnimationCore", "GameplayTasks", "PhysicsCore", "UMG", "Slate", "SlateCore", "EnhancedInput", "Json", "CinematicCamera", "MediaAssets", "RHI", "RenderCore",
			// haydaladigan mashina (ABozorMashina) - Chaos Vehicle
			"ChaosVehicles"
		});
		// mikrofon (UBozorQuloq): AudioCapture plagini Windows uchun WASAPI qismini ulaydi
		PrivateDependencyModuleNames.AddRange(new string[] { "AudioCaptureCore", "AudioCapture", "AudioMixer" });

		// lokal STT: o'yin bilan birga tarqatiladi (AI/stt/yukla.sh tayyorlaydi; git'ga kirmaydi)
		PrivateDependencyModuleNames.Add("HTTP");
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			foreach (string Fayl in new string[] { "crispasr.exe", "openblas.dll", "stt-uz.gguf", "LICENSE", "THIRD_PARTY_NOTICES.txt" })
			{
				RuntimeDependencies.Add("$(ProjectDir)/AI/stt/" + Fayl, StagedFileType.NonUFS);
			}
			RuntimeDependencies.Add("$(ProjectDir)/AI/stt/rubai/...", StagedFileType.NonUFS);

			RuntimeDependencies.Add("$(ProjectDir)/AI/miya/llama/...", StagedFileType.NonUFS);
			RuntimeDependencies.Add("$(ProjectDir)/AI/miya/model.gguf", StagedFileType.NonUFS);
			RuntimeDependencies.Add("$(ProjectDir)/AI/miya/kalit.txt", StagedFileType.NonUFS);
		}
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
			// UBozorOgizMorph: bosh mesh'lariga og'iz morph target'ini yozish (MeshDescription)
			PrivateDependencyModuleNames.AddRange(new string[] { "MeshDescription", "SkeletalMeshDescription" });
		}
	}
}
