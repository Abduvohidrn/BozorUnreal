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
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundWaveProcedural.h"
#include "Kismet/KismetSystemLibrary.h"
#include "BozorOvoz.h"
#include "BozorNpc.h"
#include "BozorSotuv.h"

#include "Async/Async.h"
#include "Misc/ScopeLock.h"

DEFINE_LOG_CATEGORY_STATIC(LogBozorNavoiy, Log, All);

using FOqim = FBozorNavoiyOqim;

static TAutoConsoleVariable<int32> CVarNavoiy(TEXT("bozor.Navoiy"), 1,
	TEXT("1 - sotuvchi gaplarini Navoiy TTS jonli o'qiydi, 0 - faqat yozilgan bo'lakli ovozlar"));
static TAutoConsoleVariable<float> CVarNavoiyKutish(TEXT("bozor.NavoiyKutish"), 12.f,
	TEXT("Navoiy ovozini eng ko'pi bilan necha soniya kutish (oshsa - yozilgan ovoz)"));
static TAutoConsoleVariable<float> CVarNavoiyFps(TEXT("bozor.NavoiyFps"), 30.f,
	TEXT("Navoiy gap yozayotganda o'yin kadrini shunga cheklaydi (videokarta Navoiy'ga bo'shasin), 0 - cheklamaslik"));
static TAutoConsoleVariable<int32> CVarMalika(TEXT("bozor.Malika"), 1,
	TEXT("1 - Malika ovozli sotuvchilar (xulq.json ovoz.server malika_kore*) Malika API'da gapiradi, 0 - Navoiy'da"));
static TAutoConsoleVariable<float> CVarMalikaKutish(TEXT("bozor.MalikaKutish"), 5.f,
	TEXT("Malika birinchi bo'lagini necha soniya kutish (oshsa - shu gap Navoiy'da yoki yozilgan ovozda)"));

namespace
{
	FString NavoiyPapka() { return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("AI/navoiy")); }
	FString PythonExe() { return NavoiyPapka() / TEXT("muhit/.venv/Scripts/python.exe"); }
	FString ServerSkript() { return NavoiyPapka() / TEXT("server.py"); }
	FString BazaModel() { return NavoiyPapka() / TEXT("muhit/CosyVoice/pretrained_models/CosyVoice2-0.5B/llm.pt"); }
	FString NavoiyModel() { return NavoiyPapka() / TEXT("muhit/navoiy-tts/emotion_600h_joint.pt"); }
	// WSL rejimi: AI/navoiy/wsl.txt - 1-qator distro (Ubuntu), 2-qator muhit papkasi WSL ichida (/home/<user>/navoiy)
	FString WslFayl() { return NavoiyPapka() / TEXT("wsl.txt"); }
	FString WslExe() { return FPaths::Combine(FPlatformMisc::GetEnvironmentVariable(TEXT("SystemRoot")), TEXT("System32/wsl.exe")); }
	bool WslSozlama(FString& Distro, FString& Muhit)
	{
		TArray<FString> Q;
		if (!FFileHelper::LoadFileToStringArray(Q, *WslFayl())) return false;
		Q.RemoveAll([](const FString& S) { return S.TrimStartAndEnd().IsEmpty() || S.StartsWith(TEXT("#")); });
		if (Q.Num() < 2) return false;
		Distro = Q[0].TrimStartAndEnd();
		Muhit = Q[1].TrimStartAndEnd();
		return FPaths::FileExists(WslExe());
	}
	// C:/Users/x -> /mnt/c/Users/x
	FString WslYol(const FString& Win)
	{
		FString P = FPaths::ConvertRelativePathToFull(Win).Replace(TEXT("\\"), TEXT("/"));
		if (P.Len() >= 2 && P[1] == TEXT(':')) P = TEXT("/mnt/") + P.Left(1).ToLower() + P.Mid(2);
		return P;
	}

	uint32 O32(const TArray<uint8>& B, int32 I) { return B[I] | (B[I + 1] << 8) | (B[I + 2] << 16) | (uint32(B[I + 3]) << 24); }
	uint16 O16(const TArray<uint8>& B, int32 I) { return uint16(B[I] | (B[I + 1] << 8)); }
}

void UBozorNavoiy::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	const TCHAR* Cmd = FCommandLine::Get();
	if (FParse::Param(Cmd, TEXT("NavoiyYoq")) || FParse::Param(Cmd, TEXT("AiYoq")) || IsRunningCommandlet() || IsRunningDedicatedServer()) return;
	// Malika - Navoiy'dan mustaqil: ikkalasi birga ishlaydi (Malika ayol ovozlari, Navoiy erkaklar va zaxira)
	if (!FParse::Param(Cmd, TEXT("MalikaYoq")) && MalikaSozlama()) MalikaniTekshir();
	// ofis serveri bor bo'lsa - avval u (javob bermasa MasofaviyniTekshir lokalni ishga tushiradi)
	if (!FParse::Param(Cmd, TEXT("NavoiyLokal")) && MasofaviySozlama())
	{
		MasofaviyniTekshir();
		return;
	}
	if (!FayllarBormi())
	{
		UE_LOG(LogBozorNavoiy, Log, TEXT("Navoiy o'rnatilmagan (%s) - yozilgan ovozlar ishlatiladi"), *NavoiyPapka());
		return;
	}
	Ishga();
}

void UBozorNavoiy::Deinitialize()
{
	// WSL: stdin quvuri yopilsa server o'zi chiqadi (wsl.exe ni o'ldirish Linux jarayonini to'xtatmasligi mumkin)
	if (StdinOqi || StdinYoz)
	{
		FPlatformProcess::ClosePipe(StdinOqi, StdinYoz);
		StdinOqi = StdinYoz = nullptr;
	}
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
	return (bTayyor || (bMalika && CVarMalika.GetValueOnGameThread() != 0)) && CVarNavoiy.GetValueOnGameThread() != 0;
}

bool UBozorNavoiy::FayllarBormi()
{
	if (!FPaths::FileExists(ServerSkript())) return false;
	if (FPaths::FileExists(PythonExe()) && FPaths::FileExists(BazaModel()) && FPaths::FileExists(NavoiyModel())) return true;
	FString Distro, Muhit;
	return WslSozlama(Distro, Muhit);
}

void UBozorNavoiy::Ishga()
{
	Port = 18600 + FMath::RandRange(0, 99);
	const FString Log = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Navoiy/server.log"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Log), true);
	// --ota: o'yin qulab tushsa ham server GPU xotirasini band qilib qolmasin
	const uint32 Pid = FPlatformProcess::GetCurrentProcessId();
	FString Distro, Muhit;
	if (FPaths::FileExists(PythonExe()))
	{
		const FString Arg = FString::Printf(TEXT("-I \"%s\" --port %d --ota %u --log \"%s\""), *ServerSkript(), Port, Pid, *Log);
		Jarayon = FPlatformProcess::CreateProc(*PythonExe(), *Arg, false, true, true, nullptr, -1, *NavoiyPapka(), nullptr);
	}
	else if (WslSozlama(Distro, Muhit))
	{
		// WSL2 ichidagi Linux muhiti (CUDA WSL orqali); 127.0.0.1 portini WSL Windows'ga o'zi uzatadi.
		// To'xtatish: stdin quvuri yopiladi (--stdin), o'yin qulab tushsa - --ota (tasklist.exe orqali)
		const FString Arg = FString::Printf(TEXT("-d %s --cd \"%s\" -e %s/.venv/bin/python -I server.py --port %d --ota %u --stdin --muhit \"%s\" --log \"%s\""),
			*Distro, *WslYol(NavoiyPapka()), *Muhit, Port, Pid, *Muhit, *WslYol(Log));
		FPlatformProcess::CreatePipe(StdinOqi, StdinYoz, true);
		Jarayon = FPlatformProcess::CreateProc(*WslExe(), *Arg, false, true, true, nullptr, -1, nullptr, nullptr, StdinOqi);
	}
	if (!Jarayon.IsValid())
	{
		UE_LOG(LogBozorNavoiy, Warning, TEXT("Navoiy server ishga tushmadi (%s)"), Distro.IsEmpty() ? *PythonExe() : *WslExe());
		return;
	}
	BoshVaqt = FPlatformTime::Seconds();
	UE_LOG(LogBozorNavoiy, Log, TEXT("Navoiy ishga tushirilmoqda (%s, port %d)..."), Distro.IsEmpty() ? TEXT("Windows") : *(TEXT("WSL ") + Distro), Port);
	Sogmi(0);
}

bool UBozorNavoiy::MasofaviySozlama()
{
	FString Matn;
	TSharedPtr<FJsonObject> J;
	if (FFileHelper::LoadFileToString(Matn, *(NavoiyPapka() / TEXT("masofaviy.json"))))
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Matn), J);
	if (J.IsValid())
	{
		J->TryGetStringField(TEXT("url"), MUrl);
		J->TryGetStringField(TEXT("kalit"), MKalit);
		const TSharedPtr<FJsonObject>* Ov = nullptr;
		if (J->TryGetObjectField(TEXT("ovozlar"), Ov))
			for (const auto& P : (*Ov)->Values) MOvozlar.Add(FString(P.Key), P.Value->AsString());
		const TSharedPtr<FJsonObject>* Tz = nullptr;
		if (J->TryGetObjectField(TEXT("tezlik"), Tz))
			for (const auto& P : (*Tz)->Values) MTezlik.Add(FString(P.Key), float(P.Value->AsNumber()));
	}
	const FString EnvUrl = FPlatformMisc::GetEnvironmentVariable(TEXT("NAVOIY_URL"));
	const FString EnvKalit = FPlatformMisc::GetEnvironmentVariable(TEXT("NAVOIY_KALIT"));
	if (!EnvUrl.IsEmpty()) MUrl = EnvUrl;
	if (!EnvKalit.IsEmpty()) MKalit = EnvKalit;
	MUrl.RemoveFromEnd(TEXT("/"));
	if (MUrl.IsEmpty()) return false;
	if (MKalit.IsEmpty()) UE_LOG(LogBozorNavoiy, Warning, TEXT("Masofaviy Navoiy (%s): kalit yo'q - AI/navoiy/masofaviy.json \"kalit\" yoki NAVOIY_KALIT"), *MUrl);
	return !MKalit.IsEmpty();
}

// ofis serveri /health: ready bo'lsa masofaviy rejim, bo'lmasa lokal Navoiy
void UBozorNavoiy::MasofaviyniTekshir()
{
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> R = FHttpModule::Get().CreateRequest();
	R->SetURL(MUrl + TEXT("/health"));
	R->SetVerb(TEXT("GET"));
	R->SetTimeout(3.f);
	BoshVaqt = FPlatformTime::Seconds();
	TWeakObjectPtr<UBozorNavoiy> Men(this);
	R->OnProcessRequestComplete().BindLambda([Men](FHttpRequestPtr, FHttpResponsePtr Javob, bool bOk)
	{
		if (!Men.IsValid()) return;
		TSharedPtr<FJsonObject> J;
		bool bReady = false;
		if (bOk && Javob.IsValid() && Javob->GetResponseCode() == 200
			&& FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Javob->GetContentAsString()), J) && J.IsValid())
			J->TryGetBoolField(TEXT("ready"), bReady);
		if (bReady)
		{
			Men->bMasofaviy = true;
			Men->bTayyor = true;
			UE_LOG(LogBozorNavoiy, Log, TEXT("Navoiy tayyor: masofaviy %s (%.1f s) - lokal server ishga tushirilmadi"), *Men->MUrl, FPlatformTime::Seconds() - Men->BoshVaqt);
			if (FParse::Param(FCommandLine::Get(), TEXT("NavoiySinov"))) Men->Sinov(0, 0, 0);
			return;
		}
		UE_LOG(LogBozorNavoiy, Warning, TEXT("Masofaviy Navoiy (%s) javob bermadi - lokal Navoiy"), *Men->MUrl);
		if (Men->FayllarBormi()) Men->Ishga();
	});
	R->ProcessRequest();
}

bool UBozorNavoiy::MalikaSozlama()
{
	// standart: avvalgi malika_kore* ovozlari (Gemini namunasi) endi haqiqiy Malika ovozi; qo'shimcha -> ohang
	MlOhanglar = {
		{TEXT("malika_kore"), TEXT("oddiy")}, {TEXT("malika_kore_xursand"), TEXT("xursand")},
		{TEXT("malika_kore_afsus"), TEXT("afsus")}, {TEXT("malika_kore_shoshilinch"), TEXT("shoshilinch")} };
	FString Matn;
	TSharedPtr<FJsonObject> J;
	if (FFileHelper::LoadFileToString(Matn, *(NavoiyPapka() / TEXT("malika.json"))))
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Matn), J);
	if (J.IsValid())
	{
		J->TryGetStringField(TEXT("url"), MlUrl);
		J->TryGetStringField(TEXT("kalit"), MlKalit);
		const TSharedPtr<FJsonObject>* Oh = nullptr;
		if (J->TryGetObjectField(TEXT("ohanglar"), Oh))
			for (const auto& P : (*Oh)->Values) MlOhanglar.Add(FString(P.Key), P.Value->AsString());
	}
	const FString EnvUrl = FPlatformMisc::GetEnvironmentVariable(TEXT("MALIKA_URL"));
	const FString EnvKalit = FPlatformMisc::GetEnvironmentVariable(TEXT("MALIKA_KEY"));
	if (!EnvUrl.IsEmpty()) MlUrl = EnvUrl;
	if (!EnvKalit.IsEmpty()) MlKalit = EnvKalit;
	if (MlUrl.IsEmpty() && !MlKalit.IsEmpty()) MlUrl = TEXT("http://172.17.2.36:8014");
	MlUrl.RemoveFromEnd(TEXT("/"));
	if (MlUrl.IsEmpty()) return false;
	if (MlKalit.IsEmpty()) UE_LOG(LogBozorNavoiy, Warning, TEXT("Malika (%s): kalit yo'q - AI/navoiy/malika.json \"kalit\" yoki MALIKA_KEY"), *MlUrl);
	return !MlKalit.IsEmpty();
}

// /v1/voices (kalit bilan): 200 - kalit to'g'ri va server tirik
void UBozorNavoiy::MalikaniTekshir()
{
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> R = FHttpModule::Get().CreateRequest();
	R->SetURL(MlUrl + TEXT("/v1/voices"));
	R->SetVerb(TEXT("GET"));
	R->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + MlKalit);
	R->SetTimeout(3.f);
	TWeakObjectPtr<UBozorNavoiy> Men(this);
	R->OnProcessRequestComplete().BindLambda([Men](FHttpRequestPtr, FHttpResponsePtr Javob, bool bOk)
	{
		if (!Men.IsValid()) return;
		const int32 Kod = bOk && Javob.IsValid() ? Javob->GetResponseCode() : 0;
		if (Kod == 200)
		{
			Men->bMalika = true;
			UE_LOG(LogBozorNavoiy, Log, TEXT("Malika tayyor: %s (%d ovoz -> ohang)"), *Men->MlUrl, Men->MlOhanglar.Num());
			return;
		}
		const FString Sabab = Kod == 401 ? FString(TEXT("kalit noto'g'ri (401)")) : Kod ? FString::Printf(TEXT("javob %d"), Kod) : FString(TEXT("javob yo'q (ofis tarmog'ida emasmi?)"));
		UE_LOG(LogBozorNavoiy, Warning, TEXT("Malika (%s) ishlamaydi: %s - Malika ovozli sotuvchilar Navoiy'da gapiradi"), *Men->MlUrl, *Sabab);
	});
	R->ProcessRequest();
}

FString UBozorNavoiy::MalikaOhangi(const FString& ServerOvoz) const
{
	if (ServerOvoz.StartsWith(TEXT("malika:"))) return ServerOvoz.Mid(7);
	const FString* S = MlOhanglar.Find(ServerOvoz);
	return S ? *S : FString();
}

// fe'l x jins -> server ovozi (masofaviy.json "ovozlar" ustun; serverda faqat bitta erkak ovozi bor)
FString UBozorNavoiy::MasofaviyOvoz(const FString& Fel, bool bAyol) const
{
	if (const FString* S = MOvozlar.Find(Fel + (bAyol ? TEXT(" ayol") : TEXT(" erkak")))) return *S;
	if (const FString* S = MOvozlar.Find(bAyol ? TEXT("ayol") : TEXT("erkak"))) return *S;
	if (!bAyol) return TEXT("erkak");
	static const TMap<FString, FString> Ayol = {
		{TEXT("mehribon"), TEXT("malika_kore")}, {TEXT("qattiq"), TEXT("jj_4c")},
		{TEXT("shoshqaloq"), TEXT("malika_kore_shoshilinch")}, {TEXT("ayyor"), TEXT("malika_kore_xursand")} };
	const FString* S = Ayol.Find(Fel);
	return S ? *S : FString(TEXT("malika_kore"));
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

void UBozorNavoiy::Ayt(const FString& Fel, bool bAyol, const FString& Matn, TFunction<void(USoundWave* Ovoz)> Tayyor, float Ohang, const FString& Namuna,
	const FString& ServerOvoz, float ServerTezlik)
{
	AytIchki(Fel, bAyol, Matn, MoveTemp(Tayyor), Ohang, Namuna, ServerOvoz, ServerTezlik, true);
}

void UBozorNavoiy::AytIchki(const FString& Fel, bool bAyol, const FString& Matn, TFunction<void(USoundWave* Ovoz)> Tayyor, float Ohang, const FString& Namuna,
	const FString& ServerOvoz, float ServerTezlik, bool bMalikaMumkin)
{
	// Malikaga ketadimi: sotuvchining server ovozi Malika ohangiga mos kelsa va Malika band deb belgilanmagan bo'lsa
	FString MlOhang;
	if (bMalikaMumkin && bMalika && CVarMalika.GetValueOnGameThread() != 0 && FPlatformTime::Seconds() >= MlBandGacha)
		MlOhang = MalikaOhangi(ServerOvoz.IsEmpty() ? MasofaviyOvoz(Fel, bAyol) : ServerOvoz);
	const bool bMl = !MlOhang.IsEmpty();
	if (CVarNavoiy.GetValueOnGameThread() == 0 || (!bMl && !bTayyor) || Matn.TrimStartAndEnd().IsEmpty())
	{
		Tayyor(nullptr);
		return;
	}
	// Malika ovoz bermasa (band, kalit, tarmoq) - shu gap Navoiy bilan (sotuvchi jim qolmasin)
	if (bMl && bTayyor)
	{
		TWeakObjectPtr<UBozorNavoiy> Zaxira(this);
		Tayyor = [Zaxira, Fel, bAyol, Matn, Asl = MoveTemp(Tayyor), Ohang, Namuna, ServerOvoz, ServerTezlik](USoundWave* W) mutable
		{
			if (W || !Zaxira.IsValid()) { Asl(W); return; }
			UE_LOG(LogBozorNavoiy, Log, TEXT("Malika ovoz bermadi - Navoiy: %s"), *Matn);
			Zaxira->AytIchki(Fel, bAyol, Matn, MoveTemp(Asl), Ohang, Namuna, ServerOvoz, ServerTezlik, false);
		};
	}
	++Sorovlar;
	TSharedRef<FJsonObject> Tana = MakeShared<FJsonObject>();
	Tana->SetStringField(TEXT("fel"), Fel);
	Tana->SetBoolField(TEXT("ayol"), bAyol);
	Tana->SetStringField(TEXT("matn"), Matn);
	if (Ohang != 0.f) Tana->SetNumberField(TEXT("ohang"), Ohang);
	if (!Namuna.IsEmpty()) Tana->SetStringField(TEXT("namuna"), Namuna);
	FString Body;
	FJsonSerializer::Serialize(Tana, TJsonWriterFactory<>::Create(&Body));

	// Oqim (/ayt_oqim): server har bo'lakni tayyor bo'lishi bilan yuboradi. Birinchi bo'lak kelganda Tayyor chaqiriladi
	// (sotuvchi gapira boshlaydi), keyingilari shu tovushning navbatiga qo'shiladi. Tayyor faqat birinchi bo'lak
	// NavoiyKutish ichida kelmasa nullptr bilan chaqiriladi (yozilgan ovozga qaytish).
	TSharedRef<FOqim, ESPMode::ThreadSafe> O = MakeShared<FOqim, ESPMode::ThreadSafe>();
	O->Tayyor = MoveTemp(Tayyor);
	O->T0 = FPlatformTime::Seconds();
	O->Nomi = FString::Printf(TEXT("[%s %s] %s"), *Fel, bAyol ? TEXT("ayol") : TEXT("erkak"), *Matn);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> R = FHttpModule::Get().CreateRequest();
	if (bMl)
	{
		// Malika: PCM s16le 24 kHz mono oqimi; bitta so'rovda 400 belgigacha - uzunini oxirgi gap chegarasida qisqartiramiz
		FString Kirish = Matn.TrimStartAndEnd();
		if (Kirish.Len() > 400)
		{
			int32 Chek = 0;
			for (int32 I = 0; I < 400; ++I) if (Kirish[I] == TEXT('.') || Kirish[I] == TEXT('!') || Kirish[I] == TEXT('?')) Chek = I + 1;
			Kirish = Kirish.Left(Chek > 0 ? Chek : 400);
		}
		TSharedRef<FJsonObject> M = MakeShared<FJsonObject>();
		M->SetStringField(TEXT("input"), Kirish);
		M->SetStringField(TEXT("tone"), MlOhang);
		M->SetStringField(TEXT("response_format"), TEXT("pcm"));
		Body.Reset();
		FJsonSerializer::Serialize(M, TJsonWriterFactory<>::Create(&Body));
		R->SetURL(MlUrl + TEXT("/v1/audio/speech"));
		R->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + MlKalit);
		O->bSarlavha = true;
		O->Chastota = 24000;
		O->Nomi = TEXT("malika/") + MlOhang + TEXT(" ") + O->Nomi;
	}
	else if (bMasofaviy)
	{
		// ofis serveri (OpenAI'ga o'xshash): javob - sarlavhasiz PCM s16le 24 kHz mono oqimi
		TSharedRef<FJsonObject> M = MakeShared<FJsonObject>();
		M->SetStringField(TEXT("input"), Matn);
		const FString Ovoz = ServerOvoz.IsEmpty() ? MasofaviyOvoz(Fel, bAyol) : ServerOvoz;
		M->SetStringField(TEXT("voice"), Ovoz);
		M->SetStringField(TEXT("response_format"), TEXT("pcm"));
		const float* Tz = MTezlik.Find(Fel);
		const float Tezlik = ServerTezlik > 0.f ? ServerTezlik : (Tz ? *Tz : (Fel == TEXT("shoshqaloq") ? 1.2f : 1.05f));
		M->SetNumberField(TEXT("speed"), FMath::Clamp(Tezlik, 0.75f, 1.3f));
		Body.Reset();
		FJsonSerializer::Serialize(M, TJsonWriterFactory<>::Create(&Body));
		R->SetURL(MUrl + TEXT("/v1/audio/speech"));
		R->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + MKalit);
		O->bSarlavha = true;
		O->Chastota = 24000;
		O->Nomi = Ovoz + TEXT(" ") + O->Nomi;
	}
	else
	{
		R->SetURL(FString::Printf(TEXT("http://127.0.0.1:%d/ayt_oqim"), Port));
	}
	if (bMl || bMasofaviy)
	{
		// xato javobi (401/404/422/429/503 - JSON matn) ovoz deb chalinmasin
		R->OnHeaderReceived().BindLambda([O](FHttpRequestPtr, const FString& Nom, const FString& Qiymat)
		{
			if (Nom.Equals(TEXT("Content-Type"), ESearchCase::IgnoreCase) && !Qiymat.Contains(TEXT("audio")))
			{
				FScopeLock L(&O->Qulf);
				O->bHttpXato = true;
			}
		});
	}
	R->SetVerb(TEXT("POST"));
	R->SetHeader(TEXT("Content-Type"), TEXT("application/json; charset=utf-8"));
	R->SetContentAsString(Body);
	R->SetTimeout(60.f);   // butun gap; birinchi bo'lak kutishi - quyidagi tekshiruv
	TWeakObjectPtr<UBozorNavoiy> Men(this);
	// HTTP ipida: baytlarni yig'ib, o'yin oqimiga uzatadi
	R->SetResponseBodyReceiveStreamDelegateV2(FHttpRequestStreamDelegateV2::CreateLambda([O, Men](void* Ptr, int64& Uz)
	{
		{
			FScopeLock L(&O->Qulf);
			O->Yangi.Append(static_cast<const uint8*>(Ptr), int32(Uz));
		}
		AsyncTask(ENamedThreads::GameThread, [O, Men]() { if (Men.IsValid()) Men->OqimIshla(O, false); });
	}));
	// kadrni cheklash faqat lokal Navoiy uchun (u o'yin bilan bitta videokartada)
	const bool bChekla = !bMasofaviy && !bMl;
	if (bChekla) KadrChekla(true);
	R->OnProcessRequestComplete().BindLambda([Men, O, bChekla, bMl](FHttpRequestPtr, FHttpResponsePtr Javob, bool bOk)
	{
		if (!Men.IsValid()) return;
		if (bChekla) Men->KadrChekla(false);
		const int32 Kod = Javob.IsValid() ? Javob->GetResponseCode() : 0;
		if (Kod && Kod != 200)
			UE_LOG(LogBozorNavoiy, Warning, TEXT("%s javobi %d: %s"), bMl ? TEXT("Malika") : TEXT("Navoiy"), Kod, *Javob->GetContentAsString().Left(200));
		// Malika band (503 - telefon qo'ng'irog'ida, 429 - chegara): Retry-After tugaguncha Malika gaplari Navoiy'ga
		if (bMl && (Kod == 503 || Kod == 429))
			Men->MlBandGacha = FPlatformTime::Seconds() + FMath::Clamp(FCString::Atof(*Javob->GetHeader(TEXT("Retry-After"))), 2.f, 60.f);
		Men->OqimIshla(O, true);
	});
	// birinchi bo'lak NavoiyKutish ichida kelmasa - so'rov bekor (OnProcessRequestComplete Tayyor(nullptr) beradi)
	TWeakPtr<IHttpRequest, ESPMode::ThreadSafe> RZaif = R;
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([O, RZaif](float)
	{
		if (!O->bJavob)
			if (TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> Rq = RZaif.Pin()) Rq->CancelRequest();
		return false;
	}), FMath::Max(1.f, (bMl ? CVarMalikaKutish : CVarNavoiyKutish).GetValueOnGameThread()));
	R->ProcessRequest();
}

// o'yin oqimida: kelgan baytlarni tovushga qo'shadi; bOxiri - so'rov tugadi (muvaffaqiyatli yoki yo'q)
void UBozorNavoiy::OqimIshla(const TSharedRef<FOqim, ESPMode::ThreadSafe>& O, bool bOxiri)
{
	if (O->bTugadi) return;
	TArray<uint8> B = MoveTemp(O->Qoldiq);
	O->Qoldiq.Reset();
	{
		FScopeLock L(&O->Qulf);
		B.Append(O->Yangi);
		O->Yangi.Reset();
		if (O->bHttpXato) O->bYaroqsiz = true;
	}
	if (!O->bSarlavha && !O->bYaroqsiz)
	{
		if (B.Num() >= 8)
		{
			if (FMemory::Memcmp(B.GetData(), "PCM1", 4) == 0)
			{
				O->Chastota = int32(O32(B, 4));
				B.RemoveAt(0, 8);
				O->bSarlavha = O->Chastota > 0;
			}
			if (!O->bSarlavha)
			{
				O->bYaroqsiz = true;
				UE_LOG(LogBozorNavoiy, Warning, TEXT("Navoiy oqimi ovoz emas: %s"), *FString(UTF8_TO_TCHAR(reinterpret_cast<const ANSICHAR*>(B.GetData()))).Left(200));
			}
		}
		else if (!bOxiri)
		{
			O->Qoldiq = MoveTemp(B);
			return;
		}
	}
	if (O->bSarlavha && !O->bYaroqsiz)
	{
		const int32 Juft = B.Num() & ~1;
		if (Juft < B.Num()) O->Qoldiq.Add(B.Last());
		if (Juft > 0)
		{
			if (!O->W.IsValid())
			{
				USoundWaveProcedural* W = NewObject<USoundWaveProcedural>(this);
				W->SetSampleRate(O->Chastota);
				W->NumChannels = 1;
				W->SoundGroup = SOUNDGROUP_Voice;
				W->bLooping = false;
				O->W.Reset(W);
			}
			O->W->QueueAudio(B.GetData(), Juft);
			O->Bayt += Juft;
			O->W->Duration = float(O->Bayt / 2) / float(O->Chastota);
			if (!O->bJavob)
			{
				O->bJavob = true;
				O->Birinchi = FPlatformTime::Seconds() - O->T0;
				++Ovozlar;
				O->Tayyor(O->W.Get());
			}
		}
	}
	// chalinayotgan tovush tugash vaqti: navbatdagi o'qilmagan baytlar + (oqim davom etsa) keyingi bo'lakni kutish
	if (O->W.IsValid())
		ABozorSotuvchiOvozi::JonliDavom(O->W.Get(), float(O->W->GetAvailableAudioByteCount() / 2) / float(O->Chastota) + (bOxiri ? 0.f : 3.f));
	if (!bOxiri) return;
	O->bTugadi = true;
	const double Jami = FPlatformTime::Seconds() - O->T0;
	UE_LOG(LogBozorNavoiy, Log, TEXT("%s birinchi bo'lak %.0f ms, jami %.0f ms, %.1f s ovoz %s"), O->bJavob ? TEXT("ovoz") : TEXT("yo'q"),
		O->Birinchi * 1000.0, Jami * 1000.0, O->W.IsValid() ? O->W->Duration : 0.f, *O->Nomi);
	if (!O->bJavob)
	{
		O->bJavob = true;
		O->Tayyor(nullptr);
	}
	O->Tayyor = nullptr;
	O->W.Reset();
}

// Navoiy o'yin bilan bitta videokartada: cheklovsiz kadrda o'yin GPU'ni ~90% band qiladi va WSL'dagi Navoiy 8-10 baravar
// sekinlashadi (4 s gap 13-18 s). Gap yozilayotganda kadr vaqtincha bozor.NavoiyFps bilan cheklanadi (30 da 2.5-3 s)
void UBozorNavoiy::KadrChekla(bool bHa)
{
	IConsoleVariable* MaxFps = IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS"));
	if (!MaxFps) return;
	if (bHa)
	{
		if (Faol++ > 0) return;
		const float Chek = CVarNavoiyFps.GetValueOnGameThread();
		EskiFps = MaxFps->GetFloat();
		bFpsOzgardi = Chek > 0.f && (EskiFps <= 0.f || EskiFps > Chek);
		if (bFpsOzgardi) MaxFps->Set(Chek, ECVF_SetByCode);
	}
	else
	{
		if (Faol <= 0 || --Faol > 0) return;
		if (bFpsOzgardi) MaxFps->Set(EskiFps, ECVF_SetByCode);
		bFpsOzgardi = false;
	}
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
	Ayt(Fel, V->Ayolmi(), Gap, [Men, Sotuvchi, I, Ovozli, Chalindi, Fel, Gap, T0, Keyingi](USoundWave* Ovoz) mutable
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
				*Fel, Sotuvchi.IsValid() && Sotuvchi->Ayolmi() ? TEXT("ayol") : TEXT("erkak"), Uz, FPlatformTime::Seconds() - T0 - Uz * 0.5, bChal ? 1 : 0, *Gap);
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
