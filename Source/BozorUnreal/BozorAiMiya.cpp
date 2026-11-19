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
static TAutoConsoleVariable<int32> CVarMiyaManba(TEXT("bozor.MiyaManba"), 1,
	TEXT("1 - bulut (rag-api.edcom.uz, kalit kerak), 0 - lokal llama-server. O'yin boshida o'qiladi"));
static TAutoConsoleVariable<float> CVarMiyaBulutKutish(TEXT("bozor.MiyaBulutKutish"), 10.f,
	TEXT("Bulut AI javobini eng ko'pi bilan necha soniya kutish (oshsa - tayyor gap)"));

namespace
{
	const TCHAR* BulutUrl = TEXT("https://rag-api.edcom.uz/chat");

	// API kaliti: BOZOR_AI_KALIT muhit o'zgaruvchisi, bo'lmasa AI/miya/kalit.txt ning birinchi qatori (git'ga kirmaydi)
	FString BulutKalit()
	{
		FString K = FPlatformMisc::GetEnvironmentVariable(TEXT("BOZOR_AI_KALIT")).TrimStartAndEnd();
		if (!K.IsEmpty()) return K;
		TArray<FString> Q;
		if (FFileHelper::LoadFileToStringArray(Q, *FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("AI/miya/kalit.txt"))))
			for (const FString& Y : Q)
			{
				const FString T = Y.TrimStartAndEnd();
				if (!T.IsEmpty() && !T.StartsWith(TEXT("#"))) return T;
			}
		return FString();
	}
	bool BulutTanlanganmi() { return CVarMiyaManba.GetValueOnAnyThread() != 0 && !FParse::Param(FCommandLine::Get(), TEXT("AiLokal")); }

	FString MiyaPapka() { return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("AI/miya")); }
	FString ServerExe() { return MiyaPapka() / TEXT("llama/llama-server.exe"); }
	// Model: AI/miya/model.txt bo'lsa - uning birinchi qatoridagi GGUF yo'li (masalan kattaroq Gemma 4 12B: o'zbekchasi ancha yaxshi,
	// lekin ~7 GB videoxotira va javob 1.5-2 s), bo'lmasa AI/miya/model.gguf (Qwen3-4B, yuklab olingan)
	FString ModelFayl()
	{
		TArray<FString> Q;
		if (FFileHelper::LoadFileToStringArray(Q, *(MiyaPapka() / TEXT("model.txt"))))
			for (const FString& Y : Q)
			{
				const FString T = Y.TrimStartAndEnd();
				if (!T.IsEmpty() && !T.StartsWith(TEXT("#")) && FPaths::FileExists(T)) return T;
			}
		return MiyaPapka() / TEXT("model.gguf");
	}
	FString JurnalFayl() { return FPaths::ProjectSavedDir() / TEXT("Miya/jurnal.tsv"); }

	// fe'l: tavsif + namunalar (ma'no -> shu fe'ldagi gap)
	struct FFelUslub { const TCHAR* Tavsif; TArray<TPair<FString, FString>> Namuna; };
	const FFelUslub& Uslub(EBozorFel F)
	{
		static const TMap<EBozorFel, FFelUslub> U = {
			{EBozorFel::Mehribon, {TEXT("mehribon, xushmuomala, bolalarni yaxshi ko'radi, 'bolam', 'o'g'lim', 'aylanay' deydi"), {
				{TEXT("Bir kilo bodring 9 000 so'm."), TEXT("Bodringning kilosi 9 000 so'm, bolam. Hozirgina uzilgan, yangi-yangi.")},
				{TEXT("Narxi shu."), TEXT("Aylanay, bundan tushirolmayman, o'zimga ham qimmatga tushgan.")}}}},
			{EBozorFel::Qattiq, {TEXT("qattiqqo'l, kam gapiradi, qisqa va jiddiy javob beradi, ortiqcha gapni yoqtirmaydi, lekin bolaga baribir hurmat bilan 'siz' deb gapiradi"), {
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
	if (Jarayon.IsValid() || bBulut) return true;
	// bulut: server ishga tushirish shart emas - kalit bo'lsa darhol tayyor (xato bo'lsa har so'rovda tayyor gap ishlatiladi)
	if (BulutTanlanganmi())
	{
		Kalit = BulutKalit();
		if (!Kalit.IsEmpty())
		{
			bBulut = true;
			bTayyor = true;
			UE_LOG(LogBozorMiya, Log, TEXT("AI miya: bulut (%s)"), BulutUrl);
			return true;
		}
		UE_LOG(LogBozorMiya, Log, TEXT("Bulut AI kaliti yo'q (AI/miya/kalit.txt yoki BOZOR_AI_KALIT) - lokal miya sinab ko'riladi"));
	}
	if (!FPaths::FileExists(ServerExe()) || !FPaths::FileExists(ModelFayl()))
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
	bBulut = false;
	Sessiyalar.Reset();
	if (Sorovlar) UE_LOG(LogBozorMiya, Log, TEXT("AI miya: %d so'rov, %d tasi AI javobi"), Sorovlar, AiJavoblar);
}

UBozorAiMiya* UBozorAiMiya::Get(const UObject* WorldContext)
{
	const UGameInstance* Gi = UGameplayStatics::GetGameInstance(WorldContext);
	return Gi ? Gi->GetSubsystem<UBozorAiMiya>() : nullptr;
}

bool UBozorAiMiya::FayllarBormi()
{
	if (BulutTanlanganmi() && !BulutKalit().IsEmpty()) return true;
	return FPaths::FileExists(ServerExe()) && FPaths::FileExists(ModelFayl());
}

// ================= so'rov yuborish (bulut yoki lokal)

void UBozorAiMiya::Yubor(const FString& TizimMatn, const TArray<TPair<FString, FString>>& Tarix, const FString& Savol, double Harorat, int32 MaxToken,
	TFunction<void(const FString& Xom, bool bOk)> Natija, const FString& Sessiya)
{
	TSharedRef<FJsonObject> Tana = MakeShared<FJsonObject>();
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> R = FHttpModule::Get().CreateRequest();
	R->SetVerb(TEXT("POST"));
	R->SetHeader(TEXT("Content-Type"), TEXT("application/json; charset=utf-8"));
	if (bBulut)
	{
		// rag-api: system roli yo'q - shu sotuvchining roli additional_instructions da. Har sotuvchining o'z conversation_id si:
		// birinchi so'rovda null, server qaytargan id saqlanadi va keyingi so'rovlarda yuboriladi - kontekstni server saqlaydi.
		// Sessiya ochilgan bo'lsa tarix xabarga qo'shilmaydi (aks holda u ikki marta bo'ladi)
		const FString* SessiyaId = Sessiya.IsEmpty() ? nullptr : Sessiyalar.Find(Sessiya);
		FString Xabar;
		if (Tarix.Num() && !SessiyaId)
		{
			Xabar = TEXT("Oldingi suhbat:\n");
			for (const TPair<FString, FString>& T : Tarix) Xabar += FString::Printf(TEXT("Bola: %s\nSen: %s\n"), *T.Key, *T.Value);
			Xabar += FString::Printf(TEXT("\nBola hozir aytdi: %s"), *Savol);
		}
		else Xabar = Savol;
		if (SessiyaId) Tana->SetStringField(TEXT("conversation_id"), *SessiyaId);
		else Tana->SetField(TEXT("conversation_id"), MakeShared<FJsonValueNull>());
		Tana->SetStringField(TEXT("message"), Xabar);
		Tana->SetStringField(TEXT("additional_instructions"), TizimMatn +
			TEXT("\nMuhim: sen o'yindagi sotuvchi personajsan. Faqat sotuvchining gapini yoz - hujjat, qonun, manba yoki izoh keltirma, markdown ishlatma."));
		R->SetURL(BulutUrl);
		R->SetHeader(TEXT("accept"), TEXT("application/json"));
		R->SetHeader(TEXT("Accept-Language"), TEXT("UZ"));
		R->SetHeader(TEXT("x-api-key"), Kalit);
		R->SetTimeout(FMath::Max(1.f, CVarMiyaBulutKutish.GetValueOnGameThread()));
	}
	else
	{
		TArray<TSharedPtr<FJsonValue>> Xabarlar;
		auto Qosh = [&Xabarlar](const TCHAR* Rol, const FString& Matn)
		{
			TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
			O->SetStringField(TEXT("role"), Rol);
			O->SetStringField(TEXT("content"), Matn);
			Xabarlar.Add(MakeShared<FJsonValueObject>(O));
		};
		Qosh(TEXT("system"), TizimMatn);
		for (const TPair<FString, FString>& T : Tarix) { Qosh(TEXT("user"), T.Key); Qosh(TEXT("assistant"), T.Value); }
		Qosh(TEXT("user"), Savol);
		Tana->SetArrayField(TEXT("messages"), Xabarlar);
		Tana->SetNumberField(TEXT("temperature"), Harorat);
		Tana->SetNumberField(TEXT("max_tokens"), MaxToken);
		Tana->SetBoolField(TEXT("cache_prompt"), true);
		// "fikrlaydigan" modellar (Qwen3, Gemma 4) fikrlamasin: javob tezroq, tokenlar javobga sarflansin
		TSharedRef<FJsonObject> Kw = MakeShared<FJsonObject>();
		Kw->SetBoolField(TEXT("enable_thinking"), false);
		Tana->SetObjectField(TEXT("chat_template_kwargs"), Kw);
		R->SetURL(FString::Printf(TEXT("http://127.0.0.1:%d/v1/chat/completions"), Port));
		R->SetTimeout(FMath::Max(0.5f, CVarMiyaKutish.GetValueOnGameThread()));
	}
	FString Body;
	FJsonSerializer::Serialize(Tana, TJsonWriterFactory<>::Create(&Body));
	R->SetContentAsString(Body);
	const bool bB = bBulut;
	TWeakObjectPtr<UBozorAiMiya> Men(this);
	R->OnProcessRequestComplete().BindLambda([Natija, bB, Men, Sessiya](FHttpRequestPtr, FHttpResponsePtr Javob, bool bOk)
	{
		FString Xom;
		const bool bJavob = bOk && Javob.IsValid() && Javob->GetResponseCode() == 200;
		if (bOk && Javob.IsValid() && !bJavob)
			UE_LOG(LogBozorMiya, Warning, TEXT("AI miya HTTP %d: %s"), Javob->GetResponseCode(), *Javob->GetContentAsString().Left(300));
		TSharedPtr<FJsonObject> J;
		if (bJavob && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Javob->GetContentAsString()), J) && J.IsValid())
		{
			if (bB)
			{
				J->TryGetStringField(TEXT("answer"), Xom);
				// shu sotuvchining sessiyasi: server bergan conversation_id keyingi so'rovlarda qaytariladi
				FString Cid;
				if (!Sessiya.IsEmpty() && Men.IsValid() && Men->bBulut && J->TryGetStringField(TEXT("conversation_id"), Cid) && !Cid.IsEmpty())
					Men->Sessiyalar.Add(Sessiya, Cid);
			}
			else
			{
				const TArray<TSharedPtr<FJsonValue>>* Ch = nullptr;
				const TSharedPtr<FJsonObject>* Msg = nullptr;
				if (J->TryGetArrayField(TEXT("choices"), Ch) && Ch->Num() && (*Ch)[0]->AsObject()->TryGetObjectField(TEXT("message"), Msg))
					(*Msg)->TryGetStringField(TEXT("content"), Xom);
			}
		}
		if (bB) Xom = Xom.Replace(TEXT("**"), TEXT("")).Replace(TEXT("__"), TEXT(""));
		Natija(Xom, bJavob);
	});
	R->ProcessRequest();
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
	if (Npc) K.Id = Npc->GetName();
	const uint32 H = Npc ? Xesh(Npc->GetName()) : 0u;
	K.bAyol = Npc && (Npc->bFemale || Npc->WalkStyle == 1 || Npc->WalkStyle == 3);
	const bool bKeksa = Npc && (Npc->WalkStyle == 2 || Npc->WalkStyle == 3);
	K.Yosh = bKeksa ? 60 + int32(H % 10) : 30 + int32(H % 20);
	K.Ism = K.bAyol ? Ayollar[H % UE_ARRAY_COUNT(Ayollar)] : Erkaklar[H % UE_ARRAY_COUNT(Erkaklar)];
	if (bKeksa) K.Ism = K.Ism.Replace(TEXT(" opa"), TEXT(" xola")).Replace(TEXT(" aka"), TEXT(" ota"));
	// shaxsiy xulq-atvor (xulq.json)
	if (const UBozorSotuvTizimi* Tz = Npc ? UBozorSotuvTizimi::Get(Npc) : nullptr)
		if (const FBozorRasta* R = Tz->Top(Npc))
		{
			K.Xulq = R->Xulq;
			if (!R->Xulq.Ism.IsEmpty()) K.Ism = R->Xulq.Ism;
			if (R->Xulq.Yosh > 0) K.Yosh = R->Xulq.Yosh;
		}
	return K;
}

FString UBozorAiMiya::Tizim(const FBozorShaxs& Kim) const
{
	const FFelUslub& U = Uslub(Kim.Fel);
	const FBozorXulq& X = Kim.Xulq;
	// namunalar: sotuvchining o'zinikilari (xulq.json), bo'lmasa fe'lniki
	FString Namuna;
	for (const TPair<FString, FString>& N : X.Namunalar.Num() ? X.Namunalar : U.Namuna)
		Namuna += FString::Printf(TEXT("Ma'no: %s\nSen: %s\n"), *N.Key, *N.Value);
	// shaxsiy xulq-atvor: kimligi, gapirish odati, yoqtirgan/yoqtirmagan narsalari
	FString Kimligi;
	if (X.Bormi())
	{
		Kimligi = FString::Printf(TEXT("Sen haqingda: %s\n"), *X.Tavsif);
		if (!X.Odatlar.IsEmpty()) Kimligi += FString::Printf(TEXT("Gapirish odating: %s\n"), *X.Odatlar);
		if (!X.Yoqtiradi.IsEmpty()) Kimligi += FString::Printf(TEXT("Yoqtirasan: %s\n"), *X.Yoqtiradi);
		if (!X.Yoqtirmaydi.IsEmpty()) Kimligi += FString::Printf(TEXT("Yoqtirmaysan: %s\n"), *X.Yoqtirmaydi);
		if (!X.SavdoUslubi.IsEmpty()) Kimligi += FString::Printf(TEXT("Savdolashish uslubing: %s\n"), *X.SavdoUslubi);
		if (!X.Kayfiyat.IsEmpty()) Kimligi += FString::Printf(TEXT("Bugungi holating: %s (gapingda ba'zan sezilsin, har gal aytma)\n"), *X.Kayfiyat);
		if (!X.QopolgaMunosabat.IsEmpty()) Kimligi += FString::Printf(TEXT("Bola qo'pol gapirsa: %s\n"), *X.QopolgaMunosabat);
	}
	// o'yin odobnoma emas: sotuvchi o'qituvchi emas, salom yoki "rahmat" uchun nasihat qilmaydi (faqat xulqida shunday bo'lsa)
	Kimligi += X.SalomTalab >= 0.5f
		? TEXT("Sen odob-axloqqa e'tiborlisan, lekin nasihatni qisqa qil.\n")
		: TEXT("Sen nasihatgo'y emassan: bolaga odob o'rgatma, salom bermasa ham urishma - oddiy sotuvchidek savdo qil.\n");
	return FString::Printf(TEXT(
		"Sen o'zbek bozoridagi sotuvchisan%s, %d yoshdasan, %s.\n"
		"Fe'ling: %s.\n%s"
		"Xaridor - 10-13 yoshli bola. Senga \"ma'no\" beriladi: shu gapning BUTUN ma'nosini o'z fe'lingga mos, jonli o'zbek tilida (lotin yozuvida) qayta ayt. So'zma-so'z takrorlama.\n"
		"Qoidalar:\n"
		"- Faqat 1-2 qisqa gap. Faqat sotuvchining gapi, izohsiz, qo'shtirnoqsiz. Hech qanday ism aytma.\n"
		"- Ma'nodagi barcha sonlarni AYNAN shunday yoz (masalan \"12 000 so'm\"), yangi son, narx yoki chegirma qo'shma.\n"
		"- Ma'nodagi hech bir qismni tashlab ketma, ma'noda yo'q va'da berma.\n"
		"- Namunalardagi gaplarni ko'chirma: ulardan faqat uslubingni (murojaat, so'z odatlari) ol. Ma'noga va shu molga mos kelmaydigan gap qo'shma.\n"
		"- Odat so'zlaringni har gapda takrorlama - ba'zan ishlat. Ma'nodagi savolni albatta savol qilib ayt.\n"
		"- Xaridor bola: fe'ling qanday bo'lmasin, uni hech qachon haqorat qilma, haydama, qo'rqitma ('yo'qol', 'ket', 'bor endi' kabi so'zlar taqiqlangan). Qattiq bo'lsang ham odobli bo'l.\n"
		"Namunalar (sening uslubing):\n%s"),
		// ism promptga berilmaydi: model uni gap oxiriga qo'shib qo'yadi ("... Rustam.")
		TEXT(""), Kim.Yosh, Kim.bAyol ? TEXT("ayol") : TEXT("erkak"), U.Tavsif, *Kimligi, *Namuna);
}

// ================= qayta yozish

void UBozorAiMiya::QaytaYoz(const FBozorShaxs& Kim, int32 Munosabat, const FString& BolaGapi, const FString& ToliqGap, const TArray<FString>& Saqla,
	TFunction<void(const FString& Matn, bool bAi)> Tayyor, int32 BolaOdob, const TArray<TPair<FString, FString>>* Tarix, int32 Savdolashuv)
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
	FString Kayfiyat = Munosabat >= 70 ? TEXT("\nBu bola senga doim odobli gapiryapti - unga ayniqsa iliq gapir.")
		: Munosabat < 30 ? TEXT("\nBu bola senga qayta-qayta qo'pol gapirdi - unga sovuqroq, qisqa gapir.") : TEXT("");
	// shu gapdagi qo'pollik: tanbeh ma'noning o'zida keladi (UBozorSuhbat "QopolIzoh"); bu yerda faqat ohang - qisqa va jiddiy.
	// "munosabat bildir" kabi ko'rsatma berilmaydi: 4B model uni gapga qo'shib yuboradi ("Ranjiganingni bildir, ...")
	// munosabat har sotuvchida o'zicha (xulq.json "qopolgaMunosabat"): biri tanbeh beradi, biri hazilga buradi, biri e'tibor bermaydi
	if (BolaOdob < 0)
		Kayfiyat += Kim.Xulq.QopolgaMunosabat.IsEmpty() ? FString(TEXT("\nBu gapni jiddiy, xafa ohangda ayt."))
			: FString::Printf(TEXT("\nBola hozir qo'pol gapirdi. Sen bunga shunday munosabatdasan: %s"), *Kim.Xulq.QopolgaMunosabat);
	// lokal 4B modelga bolaning gapi berilmaydi: u uni o'ziniki qilib takrorlaydi ("Tez bo'l. Pomidor ber."). Bulut modeli kuchli:
	// bolaning gapi va oldingi suhbatni ko'rib, tirik odamdek munosabat bildiradi (ma'no va sonlar baribir tekshiriladi)
	FString Savol;
	if (bBulut && !BolaGapi.TrimStartAndEnd().IsEmpty())
	{
		// shu sotuvchining sessiyasi bo'lsa oldingi suhbatni server biladi - qayta yuborilmaydi
		if (Tarix && Tarix->Num() && !SessiyaBor(Kim.Id))
		{
			Savol = TEXT("Oldingi suhbat:\n");
			for (int32 i = FMath::Max(0, Tarix->Num() - 4); i < Tarix->Num(); ++i)
				Savol += FString::Printf(TEXT("Bola: %s\nSen: %s\n"), *(*Tarix)[i].Key, *(*Tarix)[i].Value);
			Savol += TEXT("\n");
		}
		Savol += FString::Printf(TEXT("Bola hozir aytdi: \"%s\"\nMa'no: %s"), *BolaGapi, *Mano);
		Kayfiyat += TEXT("\nBolaning hozirgi gapini ko'rib, tirik odamdek tabiiy munosabat bildirib ma'noni ayt: u maqtasa xursand bo'l, hazillashsa kul, "
			"yana arzonlashtirishni so'rasa o'z savdolashish uslubingda javob qaytar. Ma'noni o'zgartirma. Bolaning gapidagi sonlarni takrorlama. "
			"Oldingi gaplaringni so'zma-so'z qaytarma.");
		if (Savdolashuv >= 2) Kayfiyat += FString::Printf(TEXT("\nBu bola narxni allaqachon %d marta tushirishni so'radi - buni his qil."), Savdolashuv);
	}
	else Savol = FString::Printf(TEXT("Ma'no: %s"), *Mano);

	const double T0 = FPlatformTime::Seconds();
	TWeakObjectPtr<UBozorAiMiya> Men(this);
	// temperature 0.4: 0.7 da ma'no ko'proq buziladi (2026-11-12 sinovi)
	Yubor(Tizim(Kim) + Kayfiyat, {}, Savol, 0.4, 100, [Men, Kim, BolaGapi, Mano, Ilova, ToliqGap, Saqla, Tayyor, T0](const FString& Xom, bool bOk)
	{
		const FString Matn = bOk ? Tozala(Xom, Kim) : FString();
		// mol nomlari ("bodring, kartoshka ... bor") tashlab ketilmasin: "Keling. Barcha bor." - yaroqsiz
		const FString Kichik = Matn.ToLower();
		const bool bSozlar = !Saqla.ContainsByPredicate([&Kichik](const FString& S) { return !Kichik.Contains(S.ToLower()); });
		// ma'noda savol bo'lsa ("Nechta?") javobda ham bo'lsin: aks holda bola nima so'ralayotganini bilmay qoladi
		const bool bSavol = !Mano.Contains(TEXT("?")) || Matn.Contains(TEXT("?"));
		const bool bAi = !Matn.IsEmpty() && bSozlar && bSavol && SonlarMosmi(Mano, Matn);
		const double Ms = (FPlatformTime::Seconds() - T0) * 1000.0;
		if (Men.IsValid() && bAi) ++Men->AiJavoblar;
		UE_LOG(LogBozorMiya, Log, TEXT("%s %.0f ms [%s, %s] %s -> %s"), bAi ? TEXT("AI") : TEXT("tayyor"), Ms, *Kim.Ism, BozorSotuv::FelNomi(Kim.Fel), *Mano,
			Xom.IsEmpty() ? (bOk ? TEXT("(bo'sh)") : TEXT("(javob yo'q)")) : *Xom.Replace(TEXT("\n"), TEXT(" ")));

		// jurnal: keyin miyani o'qitish to'plami uchun (fe'l, bola, ma'no, LLM xom javobi, ishlatildimi, ms)
		auto T = [](FString S) { return S.Replace(TEXT("\t"), TEXT(" ")).Replace(TEXT("\n"), TEXT(" ")).Replace(TEXT("\r"), TEXT("")); };
		FFileHelper::SaveStringToFile(FString::Printf(TEXT("%s\t%s\t%s\t%s\t%s\t%d\t%.0f\n"), *FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M:%S")),
			BozorSotuv::FelNomi(Kim.Fel), *T(BolaGapi), *T(Mano), *T(Xom), bAi ? 1 : 0, Ms),
			*JurnalFayl(), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);

		Tayyor(bAi ? (Ilova.IsEmpty() ? Matn : Matn + TEXT(" ") + Ilova) : ToliqGap, bAi);
	}, Kim.Id);
}

// ================= erkin suhbat

void UBozorAiMiya::Suhbatlash(const FBozorShaxs& Kim, int32 Munosabat, const TArray<TPair<FString, FString>>& Tarix, const FString& BolaGapi,
	const FString& Mollar, const FString& Zaxira, TFunction<void(const FString& Matn, bool bAi)> Tayyor)
{
	if (!bTayyor || CVarMiya.GetValueOnGameThread() == 0 || BolaGapi.TrimStartAndEnd().IsEmpty())
	{
		Tayyor(Zaxira, false);
		return;
	}
	++Sorovlar;
	const FFelUslub& U = Uslub(Kim.Fel);
	const FBozorXulq& X = Kim.Xulq;
	FString Tizim = FString::Printf(TEXT("Sen o'zbek bozoridagi sotuvchisan, %d yoshdasan, %s. Fe'ling: %s.\n"), Kim.Yosh, Kim.bAyol ? TEXT("ayol") : TEXT("erkak"), U.Tavsif);
	if (X.Bormi())
	{
		Tizim += FString::Printf(TEXT("Sen haqingda: %s\n"), *X.Tavsif);
		if (!X.Odatlar.IsEmpty()) Tizim += FString::Printf(TEXT("Gapirish odating: %s\n"), *X.Odatlar);
		if (!X.Yoqtiradi.IsEmpty()) Tizim += FString::Printf(TEXT("Yoqtirasan: %s\n"), *X.Yoqtiradi);
		if (!X.Yoqtirmaydi.IsEmpty()) Tizim += FString::Printf(TEXT("Yoqtirmaysan: %s\n"), *X.Yoqtirmaydi);
		if (!X.Kayfiyat.IsEmpty()) Tizim += FString::Printf(TEXT("Bugungi holating: %s\n"), *X.Kayfiyat);
		if (X.Hayot.Num()) Tizim += FString::Printf(TEXT("Hayotingdan (so'rashsa yoki o'rni kelsa, o'z so'zing bilan ayt): %s\n"), *FString::Join(X.Hayot, TEXT("; ")));
		if (!X.QopolgaMunosabat.IsEmpty()) Tizim += FString::Printf(TEXT("Bola qo'pol gapirsa: %s\n"), *X.QopolgaMunosabat);
	}
	if (X.SalomTalab < 0.5f) Tizim += TEXT("Sen nasihatgo'y emassan: bolaga odob o'rgatma, salom bermasa ham urishma.\n");
	if (!Mollar.IsEmpty()) Tizim += FString::Printf(TEXT("Rastangda: %s bor.\n"), *Mollar);
	Tizim += TEXT(
		"Xaridor - 10-13 yoshli bola. U bilan jonli suhbatdasan: bola senga savdodan tashqari gap aytdi yoki savol berdi.\n"
		"Unga o'z fe'lingcha, samimiy va tabiiy o'zbek tilida (lotin yozuvida) javob ber, xuddi tirik odamdek.\n"
		"Qoidalar:\n"
		"- Faqat 1-2 qisqa gap. Faqat sotuvchining gapi, izohsiz, qo'shtirnoqsiz. O'z isming aytma.\n"
		"- Hech qanday son, narx, og'irlik yoki chegirma aytma. Narx so'ralsa, molning nomini aytib so'rashini ayt.\n"
		"- Bilmagan narsangni to'qima. Gap savdodan uzoqlashsa, qisqa javob berib, muloyimlik bilan moliga qaytar.\n"
		"- Bolaga yarashmaydigan, qo'rqinchli yoki kattalarga oid mavzuga kirma: muloyimlik bilan gapni boshqa yoqqa bur.\n"
		"- Bolani hech qachon haqorat qilma, haydama, qo'rqitma ('yo'qol', 'ket', 'bor endi' kabi so'zlar taqiqlangan), u qo'pol gapirsa ham.\n"
		"- \"Sen haqingda\" - seni tushunish uchun; sirlaringni oshkor qilma. Emoji ishlatma.\n"
		"- Odat so'zlaringni har gapda takrorlama. Savolga aniq, mazmunli javob ber.\n"
		"Namunalar (o'z uslubingda ayt):\n"
		"Bola: Charchadingizmi?\nSen: Ha, ertalabdan beri oyoqdaman, lekin xaridor kelsa charchoq ham ketadi.\n"
		"Bola: Qayerda yashaysiz?\nSen: Shu yaqin mahallada, bolam. Har kuni bozorga piyoda kelaman.\n"
		"Bola: Siz futbol yaxshi ko'rasizmi?\nSen: Yoshligimda ko'p o'ynardim! Endi esa bozor - mening maydonim.");
	if (Munosabat >= 70) Tizim += TEXT("\nBu bola senga doim odobli gapiryapti - unga ayniqsa iliq gapir.");
	else if (Munosabat < 30) Tizim += TEXT("\nBu bola senga qo'pol gapirgan - sovuqroq, qisqa gapir.");

	// oxirgi 6 navbat: sotuvchi oldin nima deganini eslasin ("Bayaga aytdim-ku...")
	TArray<TPair<FString, FString>> Oxirgi;
	for (int32 i = FMath::Max(0, Tarix.Num() - 6); i < Tarix.Num(); ++i) Oxirgi.Add(Tarix[i]);
	const double T0 = FPlatformTime::Seconds();
	TWeakObjectPtr<UBozorAiMiya> Men(this);
	Yubor(Tizim, Oxirgi, BolaGapi, 0.6, 140, [Men, Kim, BolaGapi, Zaxira, Tayyor, T0](const FString& Xom, bool bOk)
	{
		const FString Matn = bOk ? Tozala(Xom, Kim) : FString();
		// pul hisobi faqat o'yinda: erkin javobda narx/pul bilan birga son bo'lmasin ("o'ttiz yildan beri" - mayli,
		// "o'n ming so'm" - yo'q: o'yindagi narxga zid bo'lishi mumkin)
		const FString K = Matn.ToLower();
		const bool bPul = K.Contains(TEXT("so'm")) || K.Contains(TEXT("som ")) || K.Contains(TEXT("pul")) || K.Contains(TEXT("narx"))
			|| K.Contains(TEXT("arzon")) || K.Contains(TEXT("chegirma")) || K.Contains(TEXT("kilo")) || K.Contains(TEXT("dona"));
		const bool bAi = !Matn.IsEmpty() && !(bPul && !SonlarMosmi(FString(), Matn));
		const double Ms = (FPlatformTime::Seconds() - T0) * 1000.0;
		if (Men.IsValid() && bAi) ++Men->AiJavoblar;
		UE_LOG(LogBozorMiya, Log, TEXT("%s %.0f ms [%s, erkin] bola: %s -> %s"), bAi ? TEXT("AI") : TEXT("tayyor"), Ms, *Kim.Ism, *BolaGapi,
			Xom.IsEmpty() ? (bOk ? TEXT("(bo'sh)") : TEXT("(javob yo'q)")) : *Xom.Replace(TEXT("\n"), TEXT(" ")));
		auto T = [](FString S) { return S.Replace(TEXT("\t"), TEXT(" ")).Replace(TEXT("\n"), TEXT(" ")).Replace(TEXT("\r"), TEXT("")); };
		FFileHelper::SaveStringToFile(FString::Printf(TEXT("%s\t%s\t%s\t%s\t%s\t%d\t%.0f\n"), *FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M:%S")),
			TEXT("erkin"), *T(BolaGapi), TEXT(""), *T(Xom), bAi ? 1 : 0, Ms),
			*JurnalFayl(), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
		Tayyor(bAi ? Matn : Zaxira, bAi);
	}, Kim.Id);
}

FString UBozorAiMiya::Tozala(const FString& Xom, const FBozorShaxs& Kim)
{
	// Gemma 4: "<|channel>thought ... <channel|>javob" - fikrlash bloki tashlanadi
	FString Javob = Xom;
	{
		const int32 Oxiri = Javob.Find(TEXT("<channel|>"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		if (Oxiri != INDEX_NONE) Javob = Javob.RightChop(Oxiri + 10);
		Javob = Javob.Replace(TEXT("<|channel>"), TEXT(""));
	}
	TArray<FString> Qatorlar;
	Javob.ParseIntoArrayLines(Qatorlar, true);
	FString S;
	for (const FString& Q : Qatorlar) if (!Q.TrimStartAndEnd().IsEmpty()) { S = Q.TrimStartAndEnd(); break; }
	for (const TCHAR* Old : {TEXT("Sen:"), TEXT("Sotuvchi:"), TEXT("Javob:")})
		if (S.StartsWith(Old)) S = S.RightChop(FCString::Strlen(Old)).TrimStart();
	// emoji va belgilar (🍃 😊 ★): ekranda va Navoiy ovozida keraksiz. O'zbekcha ʻ ’ ‘ (U+02BB, U+2018/9) qoladi
	{
		FString Toza;
		for (TCHAR C : S)
			if (!(C >= 0xD800 && C <= 0xDFFF) && !(C >= 0x2190 && C <= 0x2BFF) && C != 0xFE0F && C != 0x200D) Toza.AppendChar(C);
		S = Toza.Replace(TEXT("  "), TEXT(" ")).TrimStartAndEnd();
	}
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
	// "kayfiyat:" - ikki nuqta bilan: endi sotuvchining bugungi holati bor, "kayfiyatim zo'r" - tabiiy gap
	for (const TCHAR* Siz : {TEXT("kayfiyat:"), TEXT("ma'no"), TEXT("sovuq."), TEXT("iliq."), TEXT("oddiy."), TEXT("namuna"), TEXT("ohangda"), TEXT("ranjiganing"), TEXT("bildir,")})
		if (S.ToLower().Contains(Siz)) return FString();
	// xaridor - bola: haqorat yoki haydash ("Bor, yo'qol endi", "Olasanmi yo ket!") bo'lsa - yaroqsiz, tayyor gap aytiladi
	{
		FString K = TEXT(" ") + S.ToLower().Replace(TEXT("ʻ"), TEXT("'")).Replace(TEXT("‘"), TEXT("'")).Replace(TEXT("’"), TEXT("'")).Replace(TEXT("`"), TEXT("'")) + TEXT(" ");
		for (TCHAR& C : K) if (!FChar::IsAlpha(C) && C != TEXT('\'')) C = TEXT(' ');
		while (K.Contains(TEXT("  "))) K = K.Replace(TEXT("  "), TEXT(" "));
		static const TCHAR* Qopol[] = {
			TEXT(" yo'qol"), TEXT(" yo'kol"), TEXT(" yoqol "), TEXT(" yo ket "), TEXT(" ket endi "), TEXT(" bor endi "), TEXT(" ket bu yerdan"),
			TEXT(" jo'nab qol"), TEXT(" daf bo'l"), TEXT(" jim bo'l"), TEXT(" tuyog'ingni"), TEXT(" ahmoq"), TEXT(" axmoq"), TEXT(" tentak"),
			TEXT(" jinni"), TEXT(" eshak"), TEXT(" hayvon"), TEXT(" nodon"), TEXT(" besharm"), TEXT(" la'nat"), TEXT(" itvachcha"), TEXT(" galvars")};
		for (const TCHAR* Q : Qopol) if (K.Contains(Q)) return FString();
	}
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
