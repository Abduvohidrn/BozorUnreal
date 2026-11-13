#include "BozorNavoiy.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Containers/Ticker.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundWaveProcedural.h"
#include "Kismet/KismetSystemLibrary.h"
#include "BozorOvoz.h"
#include "BozorNpc.h"
#include "BozorSotuv.h"

DEFINE_LOG_CATEGORY_STATIC(LogBozorNavoiy, Log, All);

static TAutoConsoleVariable<int32> CVarNavoiy(TEXT("bozor.Navoiy"), 1,
	TEXT("1 - sotuvchi gaplarini Navoiy TTS jonli o'qiydi, 0 - faqat yozilgan bo'lakli ovozlar"));
static TAutoConsoleVariable<float> CVarNavoiyKutish(TEXT("bozor.NavoiyKutish"), 12.f,
	TEXT("Navoiy ovozini eng ko'pi bilan necha soniya kutish (oshsa - yozilgan ovoz)"));

namespace
{
	FString NavoiyPapka() { return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("AI/navoiy")); }
	FString PythonExe() { return NavoiyPapka() / TEXT("muhit/.venv/Scripts/python.exe"); }
	FString ServerSkript() { return NavoiyPapka() / TEXT("server.py"); }
	FString BazaModel() { return NavoiyPapka() / TEXT("muhit/CosyVoice/pretrained_models/CosyVoice2-0.5B/llm.pt"); }
	FString NavoiyModel() { return NavoiyPapka() / TEXT("muhit/navoiy-tts/emotion_600h_joint.pt"); }

	uint32 O32(const TArray<uint8>& B, int32 I) { return B[I] | (B[I + 1] << 8) | (B[I + 2] << 16) | (uint32(B[I + 3]) << 24); }
	uint16 O16(const TArray<uint8>& B, int32 I) { return uint16(B[I] | (B[I + 1] << 8)); }
}

void UBozorNavoiy::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	const TCHAR* Cmd = FCommandLine::Get();
	if (FParse::Param(Cmd, TEXT("NavoiyYoq")) || FParse::Param(Cmd, TEXT("AiYoq")) || IsRunningCommandlet() || IsRunningDedicatedServer()) return;
	if (!FayllarBormi())
	{
		UE_LOG(LogBozorNavoiy, Log, TEXT("Navoiy o'rnatilmagan (%s) - yozilgan ovozlar ishlatiladi"), *NavoiyPapka());
		return;
	}
	Ishga();
}

void UBozorNavoiy::Deinitialize()
{
	if (Jarayon.IsValid())
	{
		FPlatformProcess::TerminateProc(Jarayon, true);
		FPlatformProcess::CloseProc(Jarayon);
		Jarayon.Reset();
	}
	bTayyor = false;
	if (Sorovlar) UE_LOG(LogBozorNavoiy, Log, TEXT("Navoiy: %d so'rov, %d tasi ovozlandi"), Sorovlar, Ovozlar);
	Super::Deinitialize();
}

UBozorNavoiy* UBozorNavoiy::Get(const UObject* WorldContext)
{
	const UGameInstance* Gi = UGameplayStatics::GetGameInstance(WorldContext);
	return Gi ? Gi->GetSubsystem<UBozorNavoiy>() : nullptr;
}

bool UBozorNavoiy::Tayyormi() const
{
	return bTayyor && CVarNavoiy.GetValueOnGameThread() != 0;
}

bool UBozorNavoiy::FayllarBormi()
{
	return FPaths::FileExists(PythonExe()) && FPaths::FileExists(ServerSkript()) && FPaths::FileExists(BazaModel()) && FPaths::FileExists(NavoiyModel());
}

void UBozorNavoiy::Ishga()
{
	Port = 18600 + FMath::RandRange(0, 99);
	const FString Log = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Navoiy/server.log"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Log), true);
	// --ota: o'yin qulab tushsa ham server GPU xotirasini band qilib qolmasin
	const FString Arg = FString::Printf(TEXT("-I \"%s\" --port %d --ota %u --log \"%s\""), *ServerSkript(), Port, FPlatformProcess::GetCurrentProcessId(), *Log);
	Jarayon = FPlatformProcess::CreateProc(*PythonExe(), *Arg, false, true, true, nullptr, -1, *NavoiyPapka(), nullptr);
	if (!Jarayon.IsValid())
	{
		UE_LOG(LogBozorNavoiy, Warning, TEXT("Navoiy server ishga tushmadi: %s"), *PythonExe());
		return;
	}
	BoshVaqt = FPlatformTime::Seconds();
	UE_LOG(LogBozorNavoiy, Log, TEXT("Navoiy ishga tushirilmoqda (port %d)..."), Port);
	Sogmi(0);
}

// /health 200 bo'lguncha har 1 s (modellar GPU'ga ~20-40 s yuklanadi)
void UBozorNavoiy::Sogmi(int32 Urinish)
{
	if (!Jarayon.IsValid() || !FPlatformProcess::IsProcRunning(Jarayon))
	{
		UE_LOG(LogBozorNavoiy, Warning, TEXT("Navoiy server to'xtadi - Saved/Navoiy/server.log ni qarang. Yozilgan ovozlar ishlatiladi"));
		return;
	}
	if (Urinish > 300) { UE_LOG(LogBozorNavoiy, Warning, TEXT("Navoiy 5 daqiqada tayyor bo'lmadi")); return; }

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> R = FHttpModule::Get().CreateRequest();
	R->SetURL(FString::Printf(TEXT("http://127.0.0.1:%d/health"), Port));
	R->SetVerb(TEXT("GET"));
	R->SetTimeout(2.f);
	TWeakObjectPtr<UBozorNavoiy> Men(this);
	R->OnProcessRequestComplete().BindLambda([Men, Urinish](FHttpRequestPtr, FHttpResponsePtr Javob, bool bOk)
	{
		if (!Men.IsValid()) return;
		if (bOk && Javob.IsValid() && Javob->GetResponseCode() == 200)
		{
			Men->bTayyor = true;
			UE_LOG(LogBozorNavoiy, Log, TEXT("Navoiy tayyor (%.1f s)"), FPlatformTime::Seconds() - Men->BoshVaqt);
			if (FParse::Param(FCommandLine::Get(), TEXT("NavoiySinov"))) Men->Sinov(0, 0, 0);
			return;
		}
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Men, Urinish](float)
		{
			if (Men.IsValid()) Men->Sogmi(Urinish + 1);
			return false;
		}), 1.f);
	});
	R->ProcessRequest();
}

void UBozorNavoiy::Ayt(const FString& Fel, bool bAyol, const FString& Matn, TFunction<void(USoundWave* Ovoz)> Tayyor)
{
	if (!Tayyormi() || Matn.TrimStartAndEnd().IsEmpty())
	{
		Tayyor(nullptr);
		return;
	}
	++Sorovlar;
	TSharedRef<FJsonObject> Tana = MakeShared<FJsonObject>();
	Tana->SetStringField(TEXT("fel"), Fel);
	Tana->SetBoolField(TEXT("ayol"), bAyol);
	Tana->SetStringField(TEXT("matn"), Matn);
	FString Body;
	FJsonSerializer::Serialize(Tana, TJsonWriterFactory<>::Create(&Body));

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> R = FHttpModule::Get().CreateRequest();
	R->SetURL(FString::Printf(TEXT("http://127.0.0.1:%d/ayt"), Port));
	R->SetVerb(TEXT("POST"));
	R->SetHeader(TEXT("Content-Type"), TEXT("application/json; charset=utf-8"));
	R->SetContentAsString(Body);
	R->SetTimeout(FMath::Max(1.f, CVarNavoiyKutish.GetValueOnGameThread()));
	const double T0 = FPlatformTime::Seconds();
	TWeakObjectPtr<UBozorNavoiy> Men(this);
	R->OnProcessRequestComplete().BindLambda([Men, Fel, bAyol, Matn, Tayyor, T0](FHttpRequestPtr, FHttpResponsePtr Javob, bool bOk)
	{
		USoundWave* W = nullptr;
		if (Men.IsValid() && bOk && Javob.IsValid() && Javob->GetResponseCode() == 200)
			W = WavdanOvoz(Javob->GetContent(), Men.Get());
		if (Men.IsValid() && W) ++Men->Ovozlar;
		UE_LOG(LogBozorNavoiy, Log, TEXT("%s %.0f ms [%s %s] %.1f s: %s"), W ? TEXT("ovoz") : TEXT("yo'q"), (FPlatformTime::Seconds() - T0) * 1000.0,
			*Fel, bAyol ? TEXT("ayol") : TEXT("erkak"), W ? W->Duration : 0.f, *Matn);
		if (!W && Javob.IsValid() && Javob->GetResponseCode() != 200)
			UE_LOG(LogBozorNavoiy, Warning, TEXT("Navoiy javobi %d: %s"), Javob->GetResponseCode(), *Javob->GetContentAsString().Left(200));
		Tayyor(W);
	});
	R->ProcessRequest();
}

void UBozorNavoiy::Sinov(int32 I, int32 Ovozli, int32 Chalindi)
{
	const UBozorSotuvTizimi* Tz = UBozorSotuvTizimi::Get(this);
	TArray<const FBozorRasta*> Rastalar;
	if (Tz) for (const FBozorRasta& R : Tz->GetRastalar()) if (R.Sotuvchi) Rastalar.Add(&R);
	const int32 Jami = FMath::Min(Rastalar.Num(), 6);
	auto Keyingi = [this](int32 N, int32 O, int32 C, float Kut)
	{
		TWeakObjectPtr<UBozorNavoiy> Men(this);
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Men, N, O, C](float)
		{
			if (Men.IsValid()) Men->Sinov(N, O, C);
			return false;
		}), Kut);
	};
	if (I >= Jami)
	{
		UE_LOG(LogBozorNavoiy, Display, TEXT("[NavoiySinov] rasta=%d ovozli=%d chalindi=%d xato=%d"), Jami, Ovozli, Chalindi, Jami - Chalindi);
		if (UWorld* W = GetGameInstance()->GetWorld()) UKismetSystemLibrary::QuitGame(W, nullptr, EQuitPreference::Quit, false);
		return;
	}
	ABozorNpc* V = Rastalar[I]->Sotuvchi;
	const FString Fel = BozorSotuv::FelNomi(Rastalar[I]->Fel);
	static const TCHAR* Gaplar[] = {
		TEXT("Keling, bolam! Bodringning kilosi 9 000 so'm."),
		TEXT("Narxi shu, tushmaydi. Olsangiz oling."),
		TEXT("Mana, 2 kilo pomidor. Jami 18 000 so'm bo'ldi."),
		TEXT("Xo'p, 1,5 kilo kartoshka, 7 500 so'm."),
		TEXT("Bunaqa qovun butun bozorda yo'q, asal!"),
		TEXT("Mayli, boshqa safar keling!") };
	const FString Gap = Gaplar[I % UE_ARRAY_COUNT(Gaplar)];
	TWeakObjectPtr<UBozorNavoiy> Men(this);
	TWeakObjectPtr<ABozorNpc> Sotuvchi(V);
	const double T0 = FPlatformTime::Seconds();
	Ayt(Fel, V->bFemale, Gap, [Men, Sotuvchi, I, Ovozli, Chalindi, Fel, Gap, T0, Keyingi](USoundWave* Ovoz) mutable
	{
		if (!Men.IsValid()) return;
		const bool bAytdi = Ovoz && Sotuvchi.IsValid() && ABozorSotuvchiOvozi::SayWave(Sotuvchi.Get(), Ovoz);
		const float Uz = Ovoz ? Ovoz->Duration : 0.f;
		// yarim yo'lda haqiqatan chalinyaptimi
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Men, Sotuvchi, I, Ovozli, Chalindi, Fel, Gap, T0, bAytdi, Uz, Keyingi](float) mutable
		{
			if (!Men.IsValid()) return false;
			const bool bChal = bAytdi && Sotuvchi.IsValid() && ABozorSotuvchiOvozi::Gapiryapti(Sotuvchi.Get());
			UE_LOG(LogBozorNavoiy, Display, TEXT("[NavoiySinov] %d %s %s: ovoz %.1f s, kutish %.1f s, chalinyapti=%d | %s"), I,
				*Fel, Sotuvchi.IsValid() && Sotuvchi->bFemale ? TEXT("ayol") : TEXT("erkak"), Uz, FPlatformTime::Seconds() - T0 - Uz * 0.5, bChal ? 1 : 0, *Gap);
			Keyingi(I + 1, Ovozli + (Uz > 0.f ? 1 : 0), Chalindi + (bChal ? 1 : 0), Uz * 0.5f + 0.7f);
			return false;
		}), FMath::Max(0.2f, Uz * 0.5f));
	});
}

USoundWave* UBozorNavoiy::WavdanOvoz(const TArray<uint8>& B, UObject* Egasi)
{
	if (B.Num() < 44 || FMemory::Memcmp(B.GetData(), "RIFF", 4) != 0 || FMemory::Memcmp(B.GetData() + 8, "WAVE", 4) != 0) return nullptr;
	int32 Kanal = 0, Chastota = 0, Bit = 0, Boshi = -1, Hajm = 0;
	for (int32 I = 12; I + 8 <= B.Num();)
	{
		const uint32 Uz = O32(B, I + 4);
		if (FMemory::Memcmp(B.GetData() + I, "fmt ", 4) == 0 && I + 24 <= B.Num())
		{
			Kanal = O16(B, I + 10);
			Chastota = int32(O32(B, I + 12));
			Bit = O16(B, I + 22);
		}
		else if (FMemory::Memcmp(B.GetData() + I, "data", 4) == 0)
		{
			Boshi = I + 8;
			Hajm = int32(FMath::Min<int64>(Uz, B.Num() - Boshi));
			break;
		}
		I += 8 + int32(Uz) + (Uz & 1);
	}
	if (Boshi < 0 || Bit != 16 || Kanal < 1 || Chastota <= 0 || Hajm <= 0) return nullptr;

	USoundWaveProcedural* W = NewObject<USoundWaveProcedural>(Egasi ? Egasi : GetTransientPackage());
	W->SetSampleRate(Chastota);
	W->NumChannels = Kanal;
	W->Duration = float(Hajm / (2 * Kanal)) / float(Chastota);
	W->SoundGroup = SOUNDGROUP_Voice;
	W->bLooping = false;
	W->QueueAudio(B.GetData() + Boshi, Hajm);
	return W;
}
