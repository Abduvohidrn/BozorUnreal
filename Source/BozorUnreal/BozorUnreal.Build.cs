using UnrealBuildTool;

public class BozorUnreal : ModuleRules
{
	public BozorUnreal(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		CppStandard = CppStandardVersion.Cpp20;
		PublicDependencyModuleNames.AddRange(new string[] {
			"Core", "CoreUObject", "Engine", "InputCore", "AIModule", "NavigationSystem",
			"AnimationCore", "GameplayTasks", "PhysicsCore", "UMG", "Slate", "SlateCore", "EnhancedInput", "Json", "CinematicCamera", "MediaAssets", "RHI", "RenderCore"
		});
		// mikrofon (UBozorQuloq): AudioCapture plagini Windows uchun WASAPI qismini ulaydi
		PrivateDependencyModuleNames.AddRange(new string[] { "AudioCaptureCore", "AudioCapture" });

		// lokal STT: o'yin bilan birga tarqatiladi (AI/stt/yukla.sh tayyorlaydi; git'ga kirmaydi)
		foreach (string Fayl in new string[] { "crispasr.exe", "openblas.dll", "stt-uz.gguf", "LICENSE", "THIRD_PARTY_NOTICES.txt" })
		{
			RuntimeDependencies.Add("$(ProjectDir)/AI/stt/" + Fayl, StagedFileType.NonUFS);
		}
		// ikkinchi STT: rubaiSTT (transcribe.cpp, DLL o'yin ichida yuklanadi; bozor.Stt 1)
		RuntimeDependencies.Add("$(ProjectDir)/AI/stt/rubai/...", StagedFileType.NonUFS);

		// sotuvchi "tili" (UBozorAiMiya): lokal LLM server - llama.cpp (Vulkan) + Qwen3-4B (AI/miya/yukla.sh; git'ga kirmaydi)
		PrivateDependencyModuleNames.Add("HTTP");
		RuntimeDependencies.Add("$(ProjectDir)/AI/miya/llama/...", StagedFileType.NonUFS);
		RuntimeDependencies.Add("$(ProjectDir)/AI/miya/model.gguf", StagedFileType.NonUFS);
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}
	}
}
