#include "BozorQuloq.h"
#include "BozorRubai.h"

#include "Async/Async.h"
#include "AudioCaptureCore.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogBozorQuloq, Log, All);

namespace
{
	FString SttPapka() { return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("AI/stt")); }
	FString SttExe() { return SttPapka() / TEXT("crispasr.exe"); }
	FString SttModel() { return SttPapka() / TEXT("stt-uz.gguf"); }

	// Masofaviy Rubai (ofis DGX Spark, rubaistt_v2_medium, RTF ~0.05): AI/stt/masofaviy.json {"url": "...", "kalit": "..."}
	// yoki RUBAI_URL / RUBAI_KALIT. Fayl git'ga kirmaydi. -SttLokal - chetlab o'tish. Bir marta o'qiladi
	struct FMasofaviyRubai { FString Url, Kalit; bool bBor = false; };
	const FMasofaviyRubai& Masofaviy()
	{
		static FMasofaviyRubai M = []()
		{
			FMasofaviyRubai R;
			if (FParse::Param(FCommandLine::Get(), TEXT("SttLokal"))) return R;
			// lokal rubaiSTT bor bo'lsa - u asosiy: unga bozor so'zlari maslahati (prompt) beriladi, server esa promptni
			// qabul qilmaydi va bozor gaplarini yomonroq taniydi ("qimmat" -> "qibbati", 2026-11-19). Server faqat
			// lokal model yo'q bo'lsa yoki -SttServer bilan
			if (BozorRubai::Tanlangan() && !FParse::Param(FCommandLine::Get(), TEXT("SttServer")))
			{
				UE_LOG(LogBozorQuloq, Log, TEXT("STT: lokal rubaiSTT (bozor so'zlari maslahati bilan); ofis serveri - faqat -SttServer"));
				return R;
			}
			FString Matn;
			TSharedPtr<FJsonObject> J;
			if (FFileHelper::LoadFileToString(Matn, *(SttPapka() / TEXT("masofaviy.json")))
				&& FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Matn), J) && J.IsValid())
			{
				J->TryGetStringField(TEXT("url"), R.Url);
				J->TryGetStringField(TEXT("kalit"), R.Kalit);
			}
			const FString EU = FPlatformMisc::GetEnvironmentVariable(TEXT("RUBAI_URL")), EK = FPlatformMisc::GetEnvironmentVariable(TEXT("RUBAI_KALIT"));
			if (!EU.IsEmpty()) R.Url = EU;
			if (!EK.IsEmpty()) R.Kalit = EK;
			R.Url.RemoveFromEnd(TEXT("/"));
			R.bBor = !R.Url.IsEmpty() && !R.Kalit.IsEmpty();
			if (R.bBor) UE_LOG(LogBozorQuloq, Log, TEXT("STT: masofaviy Rubai %s (javob bermasa - lokal)"), *R.Url);
			return R;
		}();
		return M;
	}

	// 16 kHz, 16 bit, mono WAV
	bool WavYoz(const FString& Fayl, const TArray<float>& Ovoz)
	{
		const int32 Baytlar = Ovoz.Num() * 2;
		TArray<uint8> B;
		B.Reserve(44 + Baytlar);
		auto U32 = [&B](uint32 V) { B.Append(reinterpret_cast<const uint8*>(&V), 4); };
		auto U16 = [&B](uint16 V) { B.Append(reinterpret_cast<const uint8*>(&V), 2); };
		B.Append(reinterpret_cast<const uint8*>("RIFF"), 4); U32(36 + Baytlar);
		B.Append(reinterpret_cast<const uint8*>("WAVEfmt "), 8); U32(16); U16(1); U16(1); U32(16000); U32(32000); U16(2); U16(16);
		B.Append(reinterpret_cast<const uint8*>("data"), 4); U32(Baytlar);
		for (float S : Ovoz) U16(uint16(int16(FMath::Clamp(S, -1.f, 1.f) * 32767.f)));
		return FFileHelper::SaveArrayToFile(B, *Fayl);
	}
}

UBozorQuloq::UBozorQuloq()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

bool UBozorQuloq::Tayyormi()
{
	return (FPaths::FileExists(SttExe()) && FPaths::FileExists(SttModel())) || BozorRubai::Tanlangan() || Masofaviy().bBor;
}

bool UBozorQuloq::Boshla()
{
	Xato.Reset();
	if (bTinglayapti && !bOxiri) return true;
	if (bOxiri) { Xato = TEXT("Oldingi gap hali tushunilmoqda..."); return false; }
	if (bTushunyapti) { Xato = TEXT("Oldingi gap hali tushunilmoqda..."); return false; }
	if (!Tayyormi()) { Xato = TEXT("Mikrofon bilan gapirish o'rnatilmagan (AI/stt topilmadi)"); return false; }
	if (!Och()) return false;
	bAvto = false;   // tugma bilan: gap oxiri - tugma qo'yilganda
	// fonda yozilayotgan bo'lsa - oxirgi ~0.5 s (tugma bosilishidan oldingi bo'g'in) yozuvda qoladi
	if (!bYozmoqda)
	{
		Xom.Reset();
		Synth->StartCapturing();
		bYozmoqda = true;
	}
	bTinglayapti = true;
	return true;
}

bool UBozorQuloq::Och()
{
	if (Synth) return true;
	Synth = MakeShared<Audio::FAudioCaptureSynth>();
	Audio::FCaptureDeviceInfo Info;
	if (!Synth->GetDefaultCaptureDeviceInfo(Info) || !Synth->OpenDefaultStream())
	{
		Synth.Reset();
		Xato = TEXT("Mikrofon topilmadi");
		return false;
	}
	Kanallar = FMath::Max(1, Info.InputChannels);
	Chastota = Info.PreferredSampleRate > 0 ? Info.PreferredSampleRate : 48000;
	UE_LOG(LogBozorQuloq, Log, TEXT("Mikrofon: %s, %d Hz, %d kanal"), *Info.DeviceName, Chastota, Kanallar);
	return true;
}

void UBozorQuloq::Isit(bool bHa)
{
	if (bHa == bFon || !Tayyormi()) return;
	bFon = bHa;
	if (bHa)
	{
		// rubaiSTT ~0.8 GB: suhbat boshlanganda fonda yuklanadi - birinchi gap kutib qolmasin
		// (masofaviy Rubai bo'lsa - yuklanmaydi: videokarta o'yin va LLM'ga qolsin; server ishlamasa birinchi gapda yuklanadi)
		if (BozorRubai::Tanlangan() && !Masofaviy().bBor) Async(EAsyncExecution::Thread, []() { BozorRubai::Yukla(); });
		if (!Och()) { bFon = false; return; }
		if (!bYozmoqda)
		{
			Xom.Reset();
			Synth->StartCapturing();
			bYozmoqda = true;
		}
	}
	else if (bYozmoqda && !bTinglayapti)
	{
		Synth->StopCapturing();
		bYozmoqda = false;
		Xom.Reset();
	}
}

void UBozorQuloq::Yigish()
{
	if (!Synth) return;
	Bufer.Reset();
	if (!Synth->GetAudioData(Bufer)) return;
	Xom.Append(Bufer);
	float Eng = 0.f;
	double E = 0.0;
	for (float S : Bufer) { Eng = FMath::Max(Eng, FMath::Abs(S)); E += double(S) * S; }
	Rms = Bufer.Num() ? float(FMath::Sqrt(E / Bufer.Num())) : -1.f;
	// mikrofon odatda juda past keladi - ko'rsatkich uchun kuchaytiriladi
	Balandlik = FMath::Max(FMath::Min(1.f, Eng * 8.f), Balandlik * 0.85f);
}

void UBozorQuloq::SetQolsiz(bool bHa)
{
	bQolsiz = bHa && Tayyormi();
	NutqDavomi = Jimlik = 0.f;
	if (bQolsiz) Isit(true);
}

float UBozorQuloq::Esik() const
{
	return FMath::Max(MinEsik, Shovqin * NisbatEsik) * AksSado;
}

void UBozorQuloq::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!bTinglayapti)
	{
		Balandlik *= 0.85f;
		// fon: faqat oxirgi 0.6 s saqlanadi (keyingi gapning boshi uchun)
		if (bYozmoqda)
		{
			Rms = -1.f;
			Yigish();
			if (!bQolsiz) Balandlik = 0.f;
			const int32 Saqla = int32(0.6f * Chastota) * Kanallar;
			if (Xom.Num() > Saqla) Xom.RemoveAt(0, Xom.Num() - Saqla, EAllowShrinking::No);
			// Qo'lsiz rejim (ovozli chat kabi): gap boshlanishini o'zi sezadi. Esik - fon shovqinidan NisbatEsik baravar baland
			// (shovqin tez pasayadi, sekin ko'tariladi: gap shovqinga aylanib qolmasin); sotuvchi gapirayotganda karnaydan
			// mikrofonga qaytgan ovoz gap deb olinmasin - AksSado ko'paytiradi va gap uzoqroq davom etishi kerak
			if (bQolsiz && !bTushunyapti && Rms >= 0.f)
			{
				if (Rms > Esik())
				{
					NutqDavomi += DeltaTime;
					if (NutqDavomi >= (AksSado > 1.f ? 0.25f : 0.12f))
					{
						bTinglayapti = bAvto = true;
						Jimlik = 0.f;
						NutqDavomi = 0.f;
						UE_LOG(LogBozorQuloq, Log, TEXT("Gap boshlandi (o'zi): rms %.4f, shovqin %.4f, esik %.4f"), Rms, Shovqin, Esik());
						if (NutqBoshlandi) NutqBoshlandi();
					}
				}
				else
				{
					NutqDavomi = 0.f;
					Shovqin = Shovqin <= 0.f ? Rms : FMath::Lerp(Shovqin, Rms, Rms < Shovqin ? 0.3f : 0.02f);
				}
			}
		}
		return;
	}
	Rms = -1.f;
	Yigish();
	// qo'lsiz rejimda gap oxiri: JimlikTugash s jimlik (o'yinchi o'ylab qolsa ham gapi bo'linib ketmasin - juda qisqa emas)
	if (bAvto && !bOxiri && Rms >= 0.f)
	{
		if (Rms < Esik() * 0.8f) Jimlik += DeltaTime;
		else Jimlik = 0.f;
		if (Jimlik >= JimlikTugash) Tugat();
	}
	if (bOxiri && FPlatformTime::Seconds() >= OxiriVaqti) Yakunla();
	else if (!bOxiri && Xom.Num() > EngUzun * Chastota * Kanallar) Tugat();
}

void UBozorQuloq::Tugat()
{
	// tugma qo'yilganda qurilma buferida qolgan oxirgi ~0.1 s olinadi, keyin mikrofon darhol yopiladi
	// (0.35 s edi - tugma qo'yilgandan keyingi ovoz ham yozilib qolardi)
	if (!bTinglayapti || bOxiri) return;
	bOxiri = true;
	OxiriVaqti = FPlatformTime::Seconds() + 0.1;
}

void UBozorQuloq::Yakunla()
{
	Yigish();
	if (!bFon)
	{
		Synth->StopCapturing();
		bYozmoqda = false;
	}
	bOxirgiAvto = bAvto;
	bTinglayapti = bOxiri = bAvto = false;

	// eng baland kanal (stereo mikrofonning bir kanali ko'pincha bo'sh - o'rtachasi ovozni yarmiga tushiradi)
	const int32 Kadrlar = Xom.Num() / Kanallar;
	int32 Kanal = 0;
	double EngKuch = -1.0;
	for (int32 k = 0; k < Kanallar; ++k)
	{
		double E = 0.0;
		for (int32 i = 0; i < Kadrlar; ++i) E += double(Xom[i * Kanallar + k]) * Xom[i * Kanallar + k];
		if (E > EngKuch) { EngKuch = E; Kanal = k; }
	}
	TArray<float> Mono;
	Mono.SetNumUninitialized(Kadrlar);
	float Eng = 0.f;
	for (int32 i = 0; i < Kadrlar; ++i) { Mono[i] = Xom[i * Kanallar + Kanal]; Eng = FMath::Max(Eng, FMath::Abs(Mono[i])); }
	Xom.Reset();
	// oqim endi ochilganda qurilma bir muddat aniq nol beradi - olib tashlanadi
	int32 Boshi = 0;
	while (Boshi < Mono.Num() && FMath::Abs(Mono[Boshi]) < 1e-7f) ++Boshi;
	if (Boshi > 0) Mono.RemoveAt(0, Boshi);

	const float Sekund = float(Mono.Num()) / Chastota;
	if (Sekund < 0.4f || Eng < (bOxirgiAvto ? MinChoqqi : 0.002f))
	{
		UE_LOG(LogBozorQuloq, Log, TEXT("Gap juda qisqa yoki jim (%.2f s, cho'qqi %.4f%s)"), Sekund, Eng, bOxirgiAvto ? TEXT(", o'zi boshlangan - fon ovozi") : TEXT(""));
		if (NatijaKeldi) NatijaKeldi(FString());
		return;
	}

	// 16 kHz ga: past chastota filtrli (Hann oynali sinc) resemplash - filtrsiz yuqori chastotalar buziladi (aliasing)
	const double Nisbat = double(Chastota) / 16000.0;
	const double Fc = 0.45 / FMath::Max(1.0, Nisbat);   // kirish chastotasiga nisbatan, ~7.2 kHz
	const int32 K = 16 * FMath::Max(1, FMath::CeilToInt(Nisbat));
	const int32 N = int32(Mono.Num() / Nisbat);
	const float Kuch = FMath::Clamp(0.9f / Eng, 1.f, 30.f);
	TArray<float> Ovoz;
	Ovoz.SetNumUninitialized(N);
	for (int32 i = 0; i < N; ++i)
	{
		const double X = i * Nisbat;
		const int32 Markaz = int32(X);
		double Yig = 0.0, Vazn = 0.0;
		for (int32 j = Markaz - K + 1; j <= Markaz + K; ++j)
		{
			if (j < 0 || j >= Mono.Num()) continue;
			const double D = X - j;
			const double Arg = 2.0 * Fc * D;
			const double Sinc = FMath::Abs(Arg) < 1e-9 ? 1.0 : FMath::Sin(UE_DOUBLE_PI * Arg) / (UE_DOUBLE_PI * Arg);
			const double H = Sinc * (0.5 + 0.5 * FMath::Cos(UE_DOUBLE_PI * D / K));
			Yig += Mono[j] * H;
			Vazn += H;
		}
		Ovoz[i] = float(Vazn > 1e-9 ? Yig / Vazn : 0.0) * Kuch;
	}

	// kuchaytirilgandan keyin ham o'rtacha energiya past - ichida gap yo'q (bitta "chiq" ovozi). Whisper (rubaiSTT)
	// jim yozuvga so'z to'qiydi ("rahmat.", "tushunarli.") - bunday yozuv STT'ga berilmaydi
	{
		double E = 0.0;
		for (float S : Ovoz) E += double(S) * S;
		const float OrtaRms = Ovoz.Num() ? float(FMath::Sqrt(E / Ovoz.Num())) : 0.f;
		if (OrtaRms < 0.012f)
		{
			UE_LOG(LogBozorQuloq, Log, TEXT("Yozuvda gap yo'q (%.2f s, o'rtacha %.4f)"), Sekund, OrtaRms);
			if (NatijaKeldi) NatijaKeldi(FString());
			return;
		}
	}

	const FString Papka = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Quloq"));
	IFileManager::Get().MakeDirectory(*Papka, true);
	const FString Asos = Papka / FString::Printf(TEXT("gap_%02d"), Raqam++ % 20);
	if (!WavYoz(Asos + TEXT(".wav"), Ovoz))
	{
		if (NatijaKeldi) NatijaKeldi(FString());
		return;
	}
	UE_LOG(LogBozorQuloq, Log, TEXT("Yozuv: %.1f s, %d Hz, %d kanal (olingan %d), cho'qqi %.4f, kuch x%.1f"), Sekund, Chastota, Kanallar, Kanal, Eng, Kuch);
	IFileManager::Get().Delete(*(Asos + TEXT(".txt")), false, true, true);

	Tanit(Asos + TEXT(".wav"), Asos, Sekund);
}

void UBozorQuloq::Fayldan(const FString& Fayl)
{
	if (bTinglayapti || bTushunyapti || !Tayyormi()) { if (NatijaKeldi) NatijaKeldi(FString()); return; }
	const FString Papka = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Quloq"));
	IFileManager::Get().MakeDirectory(*Papka, true);
	const FString Asos = Papka / TEXT("fayl");
	IFileManager::Get().Delete(*(Asos + TEXT(".txt")), false, true, true);
	Tanit(FPaths::ConvertRelativePathToFull(Fayl), Asos, 0.f);
}

void UBozorQuloq::Tanit(const FString& Audio, const FString& Asos, float Sekund)
{
	bTushunyapti = true;
	// avval ofis serveri; javob bermasa yoki xato - lokal STT (pastdagi yo'l)
	const FMasofaviyRubai& M = Masofaviy();
	TArray<uint8> Wav;
	if (M.bBor && FFileHelper::LoadFileToArray(Wav, *Audio))
	{
		const FString Chegara = FString::Printf(TEXT("----BozorChegara%08x"), FMath::Rand());
		TArray<uint8> Tana;
		auto Qosh = [&Tana](const FString& S) { FTCHARToUTF8 U(*S); Tana.Append(reinterpret_cast<const uint8*>(U.Get()), U.Length()); };
		Qosh(FString::Printf(TEXT("--%s\r\nContent-Disposition: form-data; name=\"language\"\r\n\r\nuz\r\n"), *Chegara));
		Qosh(FString::Printf(TEXT("--%s\r\nContent-Disposition: form-data; name=\"response_format\"\r\n\r\njson\r\n"), *Chegara));
		// server ffmpeg bilan istalgan formatni o'qiydi (Fayldan mp3 ham berishi mumkin)
		const FString Kengaytma = FPaths::GetExtension(Audio).ToLower();
		Qosh(FString::Printf(TEXT("--%s\r\nContent-Disposition: form-data; name=\"file\"; filename=\"gap.%s\"\r\nContent-Type: %s\r\n\r\n"), *Chegara,
			*Kengaytma, Kengaytma == TEXT("mp3") ? TEXT("audio/mpeg") : TEXT("audio/wav")));
		Tana.Append(Wav);
		Qosh(FString::Printf(TEXT("\r\n--%s--\r\n"), *Chegara));
		TSharedRef<IHttpRequest, ESPMode::ThreadSafe> R = FHttpModule::Get().CreateRequest();
		R->SetURL(M.Url + TEXT("/v1/audio/transcriptions"));
		R->SetVerb(TEXT("POST"));
		R->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + M.Kalit);
		R->SetHeader(TEXT("Content-Type"), TEXT("multipart/form-data; boundary=") + Chegara);
		R->SetContent(MoveTemp(Tana));
		R->SetTimeout(8.f);
		const double T0 = FPlatformTime::Seconds();
		TWeakObjectPtr<UBozorQuloq> Men(this);
		R->OnProcessRequestComplete().BindLambda([Men, Audio, Asos, Sekund, T0](FHttpRequestPtr, FHttpResponsePtr Javob, bool bOk)
		{
			UBozorQuloq* Q = Men.Get();
			if (!Q) return;
			FString Matn;
			TSharedPtr<FJsonObject> J;
			const bool bJavob = bOk && Javob.IsValid() && Javob->GetResponseCode() == 200
				&& FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Javob->GetContentAsString()), J) && J.IsValid() && J->TryGetStringField(TEXT("text"), Matn);
			const double Ms = (FPlatformTime::Seconds() - T0) * 1000.0;
			if (!bJavob)
			{
				UE_LOG(LogBozorQuloq, Warning, TEXT("Masofaviy Rubai javob bermadi (%d, %.0f ms) - lokal STT"), Javob.IsValid() ? Javob->GetResponseCode() : 0, Ms);
				Q->MahalliyTanit(Audio, Asos, Sekund);
				return;
			}
			Matn.TrimStartAndEndInline();
			UE_LOG(LogBozorQuloq, Log, TEXT("STT (Rubai server): %.1f s ovoz -> %.0f ms: \"%s\""), Sekund, Ms, *Matn);
			FFileHelper::SaveStringToFile(FString::Printf(TEXT("%s\t%s\t%.0f\t%s\t%s\n"), *FDateTime::Now().ToString(), *FPaths::GetCleanFilename(Audio), Ms, *Matn, TEXT("RubaiServer")),
				*(FPaths::GetPath(Asos) / TEXT("jurnal.tsv")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
			Q->bTushunyapti = false;
			if (Q->NatijaKeldi) Q->NatijaKeldi(Matn);
		});
		R->ProcessRequest();
		return;
	}
	MahalliyTanit(Audio, Asos, Sekund);
}

void UBozorQuloq::MahalliyTanit(const FString& Audio, const FString& Asos, float Sekund)
{
	bTushunyapti = true;
	const int32 Iplar = FMath::Clamp(FPlatformMisc::NumberOfCores() / 2, 2, 4);
	const FString Arg = FString::Printf(TEXT("--backend fastconformer-ctc -m \"%s\" -f \"%s\" -t %d -nt -ng -np --lid-backend off --no-punctuation -otxt -of \"%s\""),
		*SttModel(), *Audio, Iplar, *Asos);
	TWeakObjectPtr<UBozorQuloq> Men(this);
	const bool bRubai = BozorRubai::Tanlangan();
	Async(EAsyncExecution::Thread, [Men, Arg, Audio, Asos, Sekund, bRubai]()
	{
		const double T0 = FPlatformTime::Seconds();
		int32 Kod = -1;
		FString Matn;
		const TCHAR* Model = TEXT("FastConformer");
		TArray<float> Pcm;
		if (bRubai && BozorRubai::WavOqi(Audio, Pcm))
		{
			bool bOk = false;
			Matn = BozorRubai::Tanit(Pcm, bOk);
			Kod = bOk ? 0 : 1;
			Model = TEXT("rubaiSTT");
		}
		// rubaiSTT tanlanmagan, ishlamadi yoki fayl WAV emas (mp3) - CrispASR + FastConformer
		if (Kod != 0 && FPaths::FileExists(SttExe()))
		{
			FString Chiqdi, XatoMatn;
			FPlatformProcess::ExecProcess(*SttExe(), *Arg, &Kod, &Chiqdi, &XatoMatn, *SttPapka());
			FFileHelper::LoadFileToString(Matn, *(Asos + TEXT(".txt")));
			Matn.TrimStartAndEndInline();
			Model = TEXT("FastConformer");
			if (Kod != 0) UE_LOG(LogBozorQuloq, Warning, TEXT("STT xatosi: %s"), *XatoMatn.Right(500));
		}
		const double Ms = (FPlatformTime::Seconds() - T0) * 1000.0;
		UE_LOG(LogBozorQuloq, Log, TEXT("STT (%s): %.1f s ovoz -> %.0f ms, kod %d: \"%s\""), Model, Sekund, Ms, Kod, *Matn);
		// jurnal: qaysi yozuvdan nima eshitildi (STT sifatini keyin tahlil qilish uchun)
		FFileHelper::SaveStringToFile(FString::Printf(TEXT("%s\t%s\t%.0f\t%s\t%s\n"), *FDateTime::Now().ToString(), *FPaths::GetCleanFilename(Audio), Ms, *Matn, Model),
			*(FPaths::GetPath(Asos) / TEXT("jurnal.tsv")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
		AsyncTask(ENamedThreads::GameThread, [Men, Matn]()
		{
			if (UBozorQuloq* Q = Men.Get())
			{
				Q->bTushunyapti = false;
				if (Q->NatijaKeldi) Q->NatijaKeldi(Matn);
			}
		});
	});
}

void UBozorQuloq::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Synth)
	{
		if (bYozmoqda) Synth->StopCapturing();
		bOxiri = bYozmoqda = bFon = false;
		Synth->AbortCapturing();
		Synth.Reset();
	}
	bTinglayapti = false;
	Super::EndPlay(EndPlayReason);
}
