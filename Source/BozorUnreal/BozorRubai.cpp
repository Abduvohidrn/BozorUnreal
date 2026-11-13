#include "BozorRubai.h"

#include "HAL/IConsoleManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"

// transcribe.cpp C API (v0.4.1, MIT). Faqat turlar kerak: funksiyalar DLL'dan GetDllExport bilan olinadi
#define TRANSCRIBE_STATIC
THIRD_PARTY_INCLUDES_START
#include "transcribe.h"
THIRD_PARTY_INCLUDES_END

DEFINE_LOG_CATEGORY_STATIC(LogBozorRubai, Log, All);

static TAutoConsoleVariable<int32> CVarStt(TEXT("bozor.Stt"), 1,
	TEXT("Mikrofon STT modeli: 1 - rubaiSTT (Whisper Medium, aniqroq, og'irroq; fayllari bo'lmasa o'zi 0 ga tushadi), 0 - NVIDIA FastConformer (CrispASR, tez)"));
static TAutoConsoleVariable<int32> CVarSttMaslahat(TEXT("bozor.SttMaslahat"), 1,
	TEXT("rubaiSTT ga bozor so'zlari bilan kontekst (prompt) berish: 1 - ha, 0 - yo'q"));

namespace
{
	FString Papka() { return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("AI/stt/rubai")); }
	FString Dll() { return Papka() / TEXT("transcribe.dll"); }
	FString Model() { return Papka() / TEXT("rubaistt.gguf"); }

	// modelni bozor so'zlariga moyil qiladi (Whisper initial prompt): uchraydigan so'zlar to'g'ri yozilishi uchun
	const char* Maslahat =
		"assalomu alaykum. pomidor necha pul? bodring, kartoshka, piyoz, sabzi, karam, non, patir qancha turadi? "
		"bir kilo, ikki kilo, yarim kilo bering. arzonroq qilib bering, iltimos. o'n ming so'm. qaytim kam. rahmat, xayr.";

	struct FDll
	{
		void* Handle = nullptr;
		decltype(&transcribe_init_backends) InitBackends = nullptr;
		decltype(&transcribe_model_load_params_init) LoadParamsInit = nullptr;
		decltype(&transcribe_model_load_file) LoadFile = nullptr;
		decltype(&transcribe_model_free) ModelFree = nullptr;
		decltype(&transcribe_session_params_init) SessionParamsInit = nullptr;
		decltype(&transcribe_session_init) SessionInit = nullptr;
		decltype(&transcribe_session_free) SessionFree = nullptr;
		decltype(&transcribe_run_params_init) RunParamsInit = nullptr;
		decltype(&transcribe_run) Run = nullptr;
		decltype(&transcribe_full_text) FullText = nullptr;
		decltype(&transcribe_status_string) StatusString = nullptr;
		decltype(&transcribe_model_backend) ModelBackend = nullptr;
	};

	FCriticalSection Qulf;
	FDll D;
	transcribe_model* Mdl = nullptr;
	transcribe_session* Ses = nullptr;
	bool bUrinildi = false;   // yuklash bir marta sinaladi (xato bo'lsa - qayta urinmaydi)

	template <typename T>
	bool Ol(T& Out, const TCHAR* Nomi)
	{
		Out = reinterpret_cast<T>(FPlatformProcess::GetDllExport(D.Handle, Nomi));
		if (!Out) UE_LOG(LogBozorRubai, Warning, TEXT("transcribe.dll: %s topilmadi"), Nomi);
		return Out != nullptr;
	}

	bool YuklaQulfda()
	{
		if (Ses) return true;
		if (bUrinildi) return false;
		bUrinildi = true;
		const double T0 = FPlatformTime::Seconds();
		FPlatformProcess::PushDllDirectory(*Papka());
		D.Handle = FPlatformProcess::GetDllHandle(*Dll());
		FPlatformProcess::PopDllDirectory(*Papka());
		if (!D.Handle) { UE_LOG(LogBozorRubai, Warning, TEXT("transcribe.dll yuklanmadi: %s"), *Dll()); return false; }
		const bool bHammasi = Ol(D.InitBackends, TEXT("transcribe_init_backends")) & Ol(D.LoadParamsInit, TEXT("transcribe_model_load_params_init"))
			& Ol(D.LoadFile, TEXT("transcribe_model_load_file")) & Ol(D.ModelFree, TEXT("transcribe_model_free"))
			& Ol(D.SessionParamsInit, TEXT("transcribe_session_params_init")) & Ol(D.SessionInit, TEXT("transcribe_session_init"))
			& Ol(D.SessionFree, TEXT("transcribe_session_free")) & Ol(D.RunParamsInit, TEXT("transcribe_run_params_init"))
			& Ol(D.Run, TEXT("transcribe_run")) & Ol(D.FullText, TEXT("transcribe_full_text"))
			& Ol(D.StatusString, TEXT("transcribe_status_string")) & Ol(D.ModelBackend, TEXT("transcribe_model_backend"));
		if (!bHammasi) return false;

		// hisoblash modullari (Vulkan, CPU) shu papkadan; Vulkan bo'lmasa - o'zi CPU'ga tushadi
		transcribe_status S = D.InitBackends(TCHAR_TO_UTF8(*Papka()));
		if (S != TRANSCRIBE_OK) { UE_LOG(LogBozorRubai, Warning, TEXT("init_backends: %hs"), D.StatusString(S)); return false; }
		transcribe_model_load_params Lp;
		D.LoadParamsInit(&Lp);
		S = D.LoadFile(TCHAR_TO_UTF8(*Model()), &Lp, &Mdl);
		if (S != TRANSCRIBE_OK || !Mdl) { UE_LOG(LogBozorRubai, Warning, TEXT("model yuklanmadi: %hs"), D.StatusString(S)); Mdl = nullptr; return false; }
		transcribe_session_params Sp;
		D.SessionParamsInit(&Sp);
		Sp.n_threads = FMath::Clamp(FPlatformMisc::NumberOfCores() / 2, 2, 8);
		S = D.SessionInit(Mdl, &Sp, &Ses);
		if (S != TRANSCRIBE_OK || !Ses)
		{
			UE_LOG(LogBozorRubai, Warning, TEXT("sessiya ochilmadi: %hs"), D.StatusString(S));
			D.ModelFree(Mdl);
			Mdl = nullptr;
			Ses = nullptr;
			return false;
		}
		// isitish: Vulkan birinchi ishga tushishda shaderlarni kompilyatsiya qiladi (~10 s) - o'yinchining birinchi gapi kutmasin
		{
			TArray<float> Jim;
			Jim.SetNumZeroed(16000);
			transcribe_run_params Rp;
			D.RunParamsInit(&Rp);
			Rp.language = "uz";
			Rp.timestamps = TRANSCRIBE_TIMESTAMPS_NONE;
			D.Run(Ses, Jim.GetData(), Jim.Num(), &Rp);
		}
		UE_LOG(LogBozorRubai, Log, TEXT("rubaiSTT tayyor (%hs, %.1f s)"), D.ModelBackend(Mdl), FPlatformTime::Seconds() - T0);
		return true;
	}
}

bool BozorRubai::Bormi()
{
	return FPaths::FileExists(Dll()) && FPaths::FileExists(Model());
}

bool BozorRubai::Tanlangan()
{
	static const bool bBuyruq = FParse::Param(FCommandLine::Get(), TEXT("SttRubai"));
	return (bBuyruq || CVarStt.GetValueOnAnyThread() == 1) && Bormi();
}

bool BozorRubai::Yukla()
{
	FScopeLock L(&Qulf);
	return YuklaQulfda();
}

FString BozorRubai::Tanit(const TArray<float>& Pcm, bool& bOk)
{
	bOk = false;
	if (Pcm.Num() <= 0) return FString();
	FScopeLock L(&Qulf);
	if (!YuklaQulfda()) return FString();
	transcribe_run_params Rp;
	D.RunParamsInit(&Rp);
	Rp.language = "uz";
	Rp.timestamps = TRANSCRIBE_TIMESTAMPS_NONE;   // model kartasi: vaqt belgilari rejimida boshiga ortiqcha bo'g'in qo'shadi
	if (CVarSttMaslahat.GetValueOnAnyThread() != 0) Rp.prompt = Maslahat;
	const transcribe_status S = D.Run(Ses, Pcm.GetData(), Pcm.Num(), &Rp);
	if (S != TRANSCRIBE_OK)
	{
		UE_LOG(LogBozorRubai, Warning, TEXT("transcribe_run: %hs"), D.StatusString(S));
		return FString();
	}
	bOk = true;
	const char* Matn = D.FullText(Ses);
	return Matn ? FString(UTF8_TO_TCHAR(Matn)).TrimStartAndEnd() : FString();
}

bool BozorRubai::WavOqi(const FString& Fayl, TArray<float>& Pcm)
{
	TArray<uint8> B;
	if (!FFileHelper::LoadFileToArray(B, *Fayl) || B.Num() < 44 || FMemory::Memcmp(B.GetData(), "RIFF", 4) != 0) return false;
	int32 O = 12, Chastota = 0, Kanal = 0, Bit = 0;
	while (O + 8 <= B.Num())
	{
		const uint32 Hajm = *reinterpret_cast<const uint32*>(&B[O + 4]);
		if (FMemory::Memcmp(&B[O], "fmt ", 4) == 0 && O + 24 <= B.Num())
		{
			Kanal = *reinterpret_cast<const uint16*>(&B[O + 10]);
			Chastota = *reinterpret_cast<const uint32*>(&B[O + 12]);
			Bit = *reinterpret_cast<const uint16*>(&B[O + 22]);
		}
		else if (FMemory::Memcmp(&B[O], "data", 4) == 0)
		{
			if (Chastota != 16000 || Kanal != 1 || Bit != 16) return false;
			const int32 N = FMath::Min<int32>(Hajm, B.Num() - O - 8) / 2;
			Pcm.SetNumUninitialized(N);
			const int16* S = reinterpret_cast<const int16*>(&B[O + 8]);
			for (int32 i = 0; i < N; ++i) Pcm[i] = S[i] / 32768.f;
			return N > 0;
		}
		O += 8 + Hajm + (Hajm & 1);
	}
	return false;
}

void BozorRubai::Bosh()
{
	FScopeLock L(&Qulf);
	if (Ses && D.SessionFree) D.SessionFree(Ses);
	if (Mdl && D.ModelFree) D.ModelFree(Mdl);
	Ses = nullptr;
	Mdl = nullptr;
	// DLL ochiq qoladi: ggml backend'lari jarayon oxirigacha ro'yxatda turadi
}
