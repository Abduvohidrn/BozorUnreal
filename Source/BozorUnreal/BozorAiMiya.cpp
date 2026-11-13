#include "BozorAiMiya.h"
#include "BozorNpc.h"
#include "BozorRubai.h"
#include "Async/Async.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Containers/Ticker.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogBozorMiya, Log, All);

static TAutoConsoleVariable<int32> CVarMiya(TEXT("bozor.Miya"), 1,
	TEXT("1 - sotuvchi javoblarini lokal LLM o'z fe'lida qayta yozadi, 0 - faqat tayyor gaplar"));
static TAutoConsoleVariable<float> CVarMiyaKutish(TEXT("bozor.MiyaKutish"), 4.f,
	TEXT("LLM javobini eng ko'pi bilan necha soniya kutish (oshsa - tayyor gap)"));
static TAutoConsoleVariable<int32> CVarMiyaYadro(TEXT("bozor.MiyaYadro"), 4,
	TEXT("LLM ishlatadigan CPU yadrolari (videokarta bo'lmasa)"));

namespace
{
	FString MiyaPapka() { return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("AI/miya")); }
	FString ServerExe() { return MiyaPapka() / TEXT("llama/llama-server.exe"); }
	FString ModelFayl() { return MiyaPapka() / TEXT("model.gguf"); }
	FString JurnalFayl() { return FPaths::ProjectSavedDir() / TEXT("Miya/jurnal.tsv"); }

	// fe'l: tavsif + namunalar (ma'no -> shu fe'ldagi gap)
	struct FFelUslub { const TCHAR* Tavsif; TArray<TPair<FString, FString>> Namuna; };
	const FFelUslub& Uslub(EBozorFel F)
	{
		static const TMap<EBozorFel, FFelUslub> U = {
			{EBozorFel::Mehribon, {TEXT("mehribon, xushmuomala, bolalarni yaxshi ko'radi, 'bolam', 'o'g'lim', 'aylanay' deydi"), {
				{TEXT("Bir kilo bodring 9 000 so'm."), TEXT("Bodringning kilosi 9 000 so'm, bolam. Hozirgina uzilgan, yangi-yangi.")},
				{TEXT("Narxi shu."), TEXT("Aylanay, bundan tushirolmayman, o'zimga ham qimmatga tushgan.")}}}},
			{EBozorFel::Qattiq, {TEXT("qattiqqo'l, kam gapiradi, qisqa va keskin javob beradi, ortiqcha gapni yoqtirmaydi"), {
				{TEXT("Bir kilo bodring 9 000 so'm."), TEXT("Bodring 9 000 so'm. Olasizmi?")},
				{TEXT("Narxi shu."), TEXT("Narxi shu, tushmaydi. Olsangiz oling.")}}}},
			{EBozorFel::Shoshqaloq, {TEXT("shoshqaloq, doim ishi ko'p, tez gapiradi, 'qani', 'tezroq', 'xo'p-xo'p' deydi"), {
				{TEXT("Bir kilo bodring 9 000 so'm."), TEXT("Bodring 9 000 so'm, qani, necha kilo? Tezroq ayting!")},
				{TEXT("Narxi shu."), TEXT("Xo'p-xo'p, narxi shu, vaqtim yo'q, olasizmi?")}}}},
			{EBozorFel::Ayyor, {TEXT("ayyor, molini haddan ortiq maqtaydi, shirin so'z, 'asal mol', 'butun bozorda yo'q' deydi"), {
				{TEXT("Bir kilo bodring 9 000 so'm."), TEXT("Bunaqa bodring butun bozorda yo'q, asal! Kilosi atigi 9 000 so'm.")},
				{TEXT("Narxi shu."), TEXT("Shu sifatga shu narx - tekin degan gap, bolam!")}}}},
		};
		return U[F];
	}

	bool Raqammi(TCHAR C) { return C >= TEXT('0') && C <= TEXT('9'); }
	bool Bosliqmi(TCHAR C) { return C == TEXT(' ') || C == 0x00A0 || C == 0x202F; }

	// matndagi sonlar: "12 000", "12000", "1 500" -> "12000", "1500" (minglik bo'shliqlari hisobga olinmaydi)
	TArray<FString> Sonlar(const FString& S)
	{
		TArray<FString> R;
		int32 i = 0;
		while (i < S.Len())
		{
			if (!Raqammi(S[i])) { ++i; continue; }
			FString N;
			while (i < S.Len())
			{
				if (Raqammi(S[i])) { N.AppendChar(S[i++]); continue; }
				// minglik ajratgich: bo'shliqdan keyin aynan 3 ta raqam
				if (Bosliqmi(S[i]) && S.IsValidIndex(i + 3) && Raqammi(S[i + 1]) && Raqammi(S[i + 2]) && Raqammi(S[i + 3])
					&& !(S.IsValidIndex(i + 4) && Raqammi(S[i + 4])))
				{
					++i;
					continue;
				}
				break;
			}
			R.AddUnique(N);
		}
		return R;
	}

	// "12000" -> "12 000" (o'yindagi BozorSotuv::Som bilan bir xil ko'rinish)
	FString SonlarniBezat(const FString& S)
	{
		FString R;
		int32 i = 0;
		while (i < S.Len())
		{
			if (!Raqammi(S[i])) { R.AppendChar(S[i++]); continue; }
			int32 j = i;
			while (j < S.Len() && Raqammi(S[j])) ++j;
			FString N = S.Mid(i, j - i);
			const bool bKasr = (i > 0 && (S[i - 1] == TEXT(',') || S[i - 1] == TEXT('.'))) || (j < S.Len() && (S[j] == TEXT(',') || S[j] == TEXT('.')) && j + 1 < S.Len() && Raqammi(S[j + 1]));
			if (N.Len() >= 4 && !bKasr)
				for (int32 k = N.Len() - 3; k > 0; k -= 3) N.InsertAt(k, TEXT(' '));
			R += N;
			i = j;
		}
		return R;
	}

	uint32 Xesh(const FString& S) { return GetTypeHash(S); }
}

// ================= ishga tushirish

void UBozorAiMiya::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (FParse::Param(FCommandLine::Get(), TEXT("AiYoq")) || IsRunningCommandlet() || IsRunningDedicatedServer()) return;
	// mikrofon STT (rubaiSTT, ~0.8 GB + GPU isitish): o'yin ochilishi bilan fonda - menyu/avtobus paytida tayyor bo'ladi,
	// o'yinchining birinchi gapi kutmaydi (suhbat boshida yuklansa ulgurmasdi: birinchi gap ~9 s)
	if (BozorRubai::Tanlangan() && !GIsAutomationTesting)
		Async(EAsyncExecution::Thread, []() { BozorRubai::Yukla(); });
	Boshla();
}

void UBozorAiMiya::Deinitialize()
{
	Toxtat();
	Super::Deinitialize();
}

bool UBozorAiMiya::Boshla()
{
	if (Jarayon.IsValid()) return true;
	if (!FayllarBormi())
	{
		UE_LOG(LogBozorMiya, Log, TEXT("AI miya o'rnatilmagan (%s yo'q) - tayyor gaplar ishlatiladi"), *ModelFayl());
		return false;
	}
	Ishga();
	return Jarayon.IsValid();
}

void UBozorAiMiya::Toxtat()
{
	if (Jarayon.IsValid())
	{
		FPlatformProcess::TerminateProc(Jarayon, true);
		FPlatformProcess::CloseProc(Jarayon);
		Jarayon.Reset();
	}
	bTayyor = false;
	if (Sorovlar) UE_LOG(LogBozorMiya, Log, TEXT("AI miya: %d so'rov, %d tasi AI javobi"), Sorovlar, AiJavoblar);
}

UBozorAiMiya* UBozorAiMiya::Get(const UObject* WorldContext)
{
	const UGameInstance* Gi = UGameplayStatics::GetGameInstance(WorldContext);
	return Gi ? Gi->GetSubsystem<UBozorAiMiya>() : nullptr;
}

bool UBozorAiMiya::FayllarBormi()
{
	return FPaths::FileExists(ServerExe()) && FPaths::FileExists(ModelFayl());
}

void UBozorAiMiya::Ishga()
{
	Port = 18700 + FMath::RandRange(0, 999);
	const FString Log = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Miya/server.log"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Log), true);
	// -ngl 99: videokarta (Vulkan) bo'lsa hammasi unda, bo'lmasa CPU. -np 1: bitta suhbat. Kontekst kichik - xotira tejaladi
	const FString Arg = FString::Printf(TEXT("-m \"%s\" --host 127.0.0.1 --port %d -ngl 99 -c 2048 -np 1 -t %d --log-file \"%s\""),
		*ModelFayl(), Port, FMath::Clamp(CVarMiyaYadro.GetValueOnGameThread(), 1, 32), *Log);
	Jarayon = FPlatformProcess::CreateProc(*ServerExe(), *Arg, false, true, true, nullptr, -1, *FPaths::GetPath(ServerExe()), nullptr);
	if (!Jarayon.IsValid())
	{
		UE_LOG(LogBozorMiya, Warning, TEXT("llama-server ishga tushmadi: %s"), *ServerExe());
		return;
	}
	BoshVaqt = FPlatformTime::Seconds();
	UE_LOG(LogBozorMiya, Log, TEXT("AI miya ishga tushirilmoqda (port %d)..."), Port);
	Sogmi(0);
}

// /health javob berguncha har 0.5 s so'raydi (model yuklanishi GPU'da ~2 s, CPU'da ~5-10 s)
void UBozorAiMiya::Sogmi(int32 Urinish)
{
	if (!Jarayon.IsValid() || !FPlatformProcess::IsProcRunning(Jarayon))
	{
		UE_LOG(LogBozorMiya, Warning, TEXT("llama-server to'xtadi - Saved/Miya/server.log ni qarang. Tayyor gaplar ishlatiladi"));
		return;
	}
	if (Urinish > 240) { UE_LOG(LogBozorMiya, Warning, TEXT("llama-server 2 daqiqada tayyor bo'lmadi")); return; }

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> R = FHttpModule::Get().CreateRequest();
	R->SetURL(FString::Printf(TEXT("http://127.0.0.1:%d/health"), Port));
	R->SetVerb(TEXT("GET"));
	R->SetTimeout(2.f);
	TWeakObjectPtr<UBozorAiMiya> Men(this);
	R->OnProcessRequestComplete().BindLambda([Men, Urinish](FHttpRequestPtr, FHttpResponsePtr Javob, bool bOk)
	{
		if (!Men.IsValid()) return;
		if (bOk && Javob.IsValid() && Javob->GetResponseCode() == 200)
		{
			Men->bTayyor = true;
			UE_LOG(LogBozorMiya, Log, TEXT("AI miya tayyor (%.1f s)"), FPlatformTime::Seconds() - Men->BoshVaqt);
			return;
		}
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Men, Urinish](float)
		{
			if (Men.IsValid()) Men->Sogmi(Urinish + 1);
			return false;
		}), 0.5f);
	});
	R->ProcessRequest();
}

// ================= shaxs va prompt

FBozorShaxs UBozorAiMiya::Shaxs(const ABozorNpc* Npc, EBozorFel Fel)
{
	static const TCHAR* Ayollar[] = {TEXT("Malika opa"), TEXT("Gulnora opa"), TEXT("Dilfuza opa"), TEXT("Nodira opa"), TEXT("Muqaddas opa"), TEXT("Zulfiya opa"), TEXT("Feruza opa"), TEXT("Barno opa")};
	static const TCHAR* Erkaklar[] = {TEXT("Rustam aka"), TEXT("Dilshod aka"), TEXT("Botir aka"), TEXT("Shavkat aka"), TEXT("Akmal aka"), TEXT("Jasur aka"), TEXT("Olim aka"), TEXT("Sanjar aka")};
	FBozorShaxs K;
	K.Fel = Fel;
	const uint32 H = Npc ? Xesh(Npc->GetName()) : 0u;
	K.bAyol = Npc && (Npc->bFemale || Npc->WalkStyle == 1 || Npc->WalkStyle == 3);
	const bool bKeksa = Npc && (Npc->WalkStyle == 2 || Npc->WalkStyle == 3);
	K.Yosh = bKeksa ? 60 + int32(H % 10) : 30 + int32(H % 20);
	K.Ism = K.bAyol ? Ayollar[H % UE_ARRAY_COUNT(Ayollar)] : Erkaklar[H % UE_ARRAY_COUNT(Erkaklar)];
	if (bKeksa) K.Ism = K.Ism.Replace(TEXT(" opa"), TEXT(" xola")).Replace(TEXT(" aka"), TEXT(" ota"));
	return K;
}

FString UBozorAiMiya::Tizim(const FBozorShaxs& Kim) const
{
	const FFelUslub& U = Uslub(Kim.Fel);
	FString Namuna;
	for (const TPair<FString, FString>& N : U.Namuna)
		Namuna += FString::Printf(TEXT("Ma'no: %s\nSen: %s\n"), *N.Key, *N.Value);
	return FString::Printf(TEXT(
		"Sen o'zbek bozoridagi sotuvchisan%s, %d yoshdasan, %s.\n"
		"Fe'ling: %s.\n"
		"Xaridor - 10-13 yoshli bola. Senga \"ma'no\" beriladi: shu gapning BUTUN ma'nosini o'z fe'lingga mos, jonli o'zbek tilida (lotin yozuvida) qayta ayt. So'zma-so'z takrorlama.\n"
		"Qoidalar:\n"
		"- Faqat 1-2 qisqa gap. Faqat sotuvchining gapi, izohsiz, qo'shtirnoqsiz. Hech qanday ism aytma.\n"
		"- Ma'nodagi barcha sonlarni AYNAN shunday yoz (masalan \"12 000 so'm\"), yangi son, narx yoki chegirma qo'shma.\n"
		"- Ma'nodagi hech bir qismni tashlab ketma, ma'noda yo'q va'da berma.\n"
		"Namunalar (sening uslubing):\n%s"),
		// ism promptga berilmaydi: model uni gap oxiriga qo'shib qo'yadi ("... Rustam.")
		TEXT(""), Kim.Yosh, Kim.bAyol ? TEXT("ayol") : TEXT("erkak"), U.Tavsif, *Namuna);
}

// ================= qayta yozish

void UBozorAiMiya::QaytaYoz(const FBozorShaxs& Kim, int32 Munosabat, const FString& BolaGapi, const FString& ToliqGap, const TArray<FString>& Saqla,
	TFunction<void(const FString& Matn, bool bAi)> Tayyor)
{
	if (!bTayyor || CVarMiya.GetValueOnGameThread() == 0 || ToliqGap.TrimStartAndEnd().IsEmpty())
	{
		Tayyor(ToliqGap, false);
		return;
	}
	// oxiridagi "[Tarozida: 2 kilo]" - o'yin belgisi, sotuvchi gapi emas: LLM'ga berilmaydi, javobga qayta qo'shiladi
	FString Mano = ToliqGap, Ilova;
	{
		const int32 Q = ToliqGap.Find(TEXT("["), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		if (Q > 0 && ToliqGap.TrimEnd().EndsWith(TEXT("]")))
		{
			Ilova = ToliqGap.Mid(Q).TrimEnd();
			Mano = ToliqGap.Left(Q).TrimEnd();
		}
	}
	++Sorovlar;
	// kayfiyat foydalanuvchi xabarida emas, tizim promptida: aks holda 4B model uni gapga qo'shadi ("Sovuq. Narxi shu.")
	const TCHAR* Kayfiyat = Munosabat >= 70 ? TEXT("\nBu bola senga juda odobli gapirdi - unga ayniqsa iliq gapir.")
		: Munosabat < 30 ? TEXT("\nBu bola senga qo'pol gapirdi - unga sovuqroq, qisqa gapir.") : TEXT("");
	// bolaning gapi berilmaydi: 4B model uni o'ziniki qilib takrorlaydi ("Tez bo'l. Pomidor ber."). Jurnalga yoziladi
	const FString Savol = FString::Printf(TEXT("Ma'no: %s"), *Mano);

	TSharedRef<FJsonObject> Tana = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Xabarlar;
	auto Xabar = [&Xabarlar](const TCHAR* Rol, const FString& Matn)
	{
		TSharedRef<FJsonObject> X = MakeShared<FJsonObject>();
		X->SetStringField(TEXT("role"), Rol);
		X->SetStringField(TEXT("content"), Matn);
		Xabarlar.Add(MakeShared<FJsonValueObject>(X));
	};
	Xabar(TEXT("system"), Tizim(Kim) + Kayfiyat);
	Xabar(TEXT("user"), Savol);
	Tana->SetArrayField(TEXT("messages"), Xabarlar);
	Tana->SetNumberField(TEXT("temperature"), 0.4);   // 0.7 da ma'no ko'proq buziladi (2026-11-12 sinovi)
	Tana->SetNumberField(TEXT("max_tokens"), 80);
	Tana->SetBoolField(TEXT("cache_prompt"), true);
	FString Body;
	FJsonSerializer::Serialize(Tana, TJsonWriterFactory<>::Create(&Body));

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> R = FHttpModule::Get().CreateRequest();
	R->SetURL(FString::Printf(TEXT("http://127.0.0.1:%d/v1/chat/completions"), Port));
	R->SetVerb(TEXT("POST"));
	R->SetHeader(TEXT("Content-Type"), TEXT("application/json; charset=utf-8"));
	R->SetContentAsString(Body);
	R->SetTimeout(FMath::Max(0.5f, CVarMiyaKutish.GetValueOnGameThread()));
	const double T0 = FPlatformTime::Seconds();
	TWeakObjectPtr<UBozorAiMiya> Men(this);
	R->OnProcessRequestComplete().BindLambda([Men, Kim, BolaGapi, Mano, Ilova, ToliqGap, Saqla, Tayyor, T0](FHttpRequestPtr, FHttpResponsePtr Javob, bool bOk)
	{
		FString Xom, Matn;
		if (bOk && Javob.IsValid() && Javob->GetResponseCode() == 200)
		{
			TSharedPtr<FJsonObject> J;
			if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Javob->GetContentAsString()), J) && J.IsValid())
			{
				const TArray<TSharedPtr<FJsonValue>>* Ch = nullptr;
				if (J->TryGetArrayField(TEXT("choices"), Ch) && Ch->Num())
				{
					const TSharedPtr<FJsonObject>* Msg = nullptr;
					if ((*Ch)[0]->AsObject()->TryGetObjectField(TEXT("message"), Msg)) Xom = (*Msg)->GetStringField(TEXT("content"));
				}
			}
			Matn = Tozala(Xom, Kim);
		}
		// mol nomlari ("bodring, kartoshka ... bor") tashlab ketilmasin: "Keling. Barcha bor." - yaroqsiz
		const FString Kichik = Matn.ToLower();
		const bool bSozlar = !Saqla.ContainsByPredicate([&Kichik](const FString& S) { return !Kichik.Contains(S.ToLower()); });
		const bool bAi = !Matn.IsEmpty() && bSozlar && SonlarMosmi(Mano, Matn);
		const double Ms = (FPlatformTime::Seconds() - T0) * 1000.0;
		if (Men.IsValid() && bAi) ++Men->AiJavoblar;
		UE_LOG(LogBozorMiya, Log, TEXT("%s %.0f ms [%s] %s -> %s"), bAi ? TEXT("AI") : TEXT("tayyor"), Ms, BozorSotuv::FelNomi(Kim.Fel), *Mano,
			Xom.IsEmpty() ? (bOk ? TEXT("(bo'sh)") : TEXT("(javob yo'q)")) : *Xom.Replace(TEXT("\n"), TEXT(" ")));

		// jurnal: keyin miyani o'qitish to'plami uchun (fe'l, bola, ma'no, LLM xom javobi, ishlatildimi, ms)
		auto T = [](FString S) { return S.Replace(TEXT("\t"), TEXT(" ")).Replace(TEXT("\n"), TEXT(" ")).Replace(TEXT("\r"), TEXT("")); };
		FFileHelper::SaveStringToFile(FString::Printf(TEXT("%s\t%s\t%s\t%s\t%s\t%d\t%.0f\n"), *FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M:%S")),
			BozorSotuv::FelNomi(Kim.Fel), *T(BolaGapi), *T(Mano), *T(Xom), bAi ? 1 : 0, Ms),
			*JurnalFayl(), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);

		Tayyor(bAi ? (Ilova.IsEmpty() ? Matn : Matn + TEXT(" ") + Ilova) : ToliqGap, bAi);
	});
	R->ProcessRequest();
}

FString UBozorAiMiya::Tozala(const FString& Xom, const FBozorShaxs& Kim)
{
	TArray<FString> Qatorlar;
	Xom.ParseIntoArrayLines(Qatorlar, true);
	FString S;
	for (const FString& Q : Qatorlar) if (!Q.TrimStartAndEnd().IsEmpty()) { S = Q.TrimStartAndEnd(); break; }
	for (const TCHAR* Old : {TEXT("Sen:"), TEXT("Sotuvchi:"), TEXT("Javob:")})
		if (S.StartsWith(Old)) S = S.RightChop(FCString::Strlen(Old)).TrimStart();
	// qo'shtirnoqlar
	static const FString Qosh = TEXT("\"'«»“”„");
	while (S.Len() >= 2 && Qosh.Contains(FString::Chr(S[0])) && Qosh.Contains(FString::Chr(S[S.Len() - 1]))) S = S.Mid(1, S.Len() - 2).TrimStartAndEnd();
	if (S.StartsWith(TEXT("\"")) || S.StartsWith(TEXT("«"))) S = S.RightChop(1);
	if (S.EndsWith(TEXT("\"")) || S.EndsWith(TEXT("»"))) S.LeftChopInline(1);
	S.TrimStartAndEndInline();
	// o'z ismini aytsa ("Gulnora opa" / "Gulnora") yoki juda uzun bo'lsa - yaroqsiz
	FString IsmBoshi, Qolgani;
	if (!Kim.Ism.Split(TEXT(" "), &IsmBoshi, &Qolgani)) IsmBoshi = Kim.Ism;
	if (!IsmBoshi.IsEmpty() && S.Contains(IsmBoshi)) return FString();
	if (S.Len() > 220) return FString();
	// prompt'dan sizib chiqqan so'zlar ("Sovuq. Narxi shu.", "Ma'no: ...")
	for (const TCHAR* Siz : {TEXT("kayfiyat"), TEXT("ma'no"), TEXT("sovuq."), TEXT("iliq."), TEXT("oddiy."), TEXT("namuna")})
		if (S.ToLower().Contains(Siz)) return FString();
	S = SonlarniBezat(S);
	if (S.Len()) S[0] = FChar::ToUpper(S[0]);
	return S;
}

bool UBozorAiMiya::SonlarMosmi(const FString& Mano, const FString& Javob)
{
	const TArray<FString> A = Sonlar(Mano), B = Sonlar(Javob);
	for (const FString& N : A) if (!B.Contains(N)) return false;
	for (const FString& N : B) if (!A.Contains(N)) return false;
	// so'z bilan aytilgan son ("o'n mingdan bering") - ma'noda bo'lmasa yangi narx to'qilgan deb olinadi
	static const TCHAR* SonSozlari[] = {TEXT("ming"), TEXT("yuz"), TEXT("o'n"), TEXT("yigirma"), TEXT("o'ttiz"), TEXT("qirq"), TEXT("ellik"),
		TEXT("oltmish"), TEXT("yetmish"), TEXT("sakson"), TEXT("to'qson"), TEXT("million")};
	auto Sozlar = [](const FString& S)
	{
		FString K = S.ToLower().Replace(TEXT("ʻ"), TEXT("'")).Replace(TEXT("‘"), TEXT("'")).Replace(TEXT("’"), TEXT("'"));
		for (TCHAR& C : K) if (!FChar::IsAlpha(C) && C != TEXT('\'')) C = TEXT(' ');
		TArray<FString> R;
		K.ParseIntoArray(R, TEXT(" "));
		return R;
	};
	const TArray<FString> MS = Sozlar(Mano), JS = Sozlar(Javob);
	for (const TCHAR* Son : SonSozlari)
	{
		auto Bor = [Son](const TArray<FString>& W) { return W.ContainsByPredicate([Son](const FString& X) { return X.StartsWith(Son) && X.Len() <= FCString::Strlen(Son) + 4; }); };
		if (Bor(JS) && !Bor(MS)) return false;
	}
	return true;
}
