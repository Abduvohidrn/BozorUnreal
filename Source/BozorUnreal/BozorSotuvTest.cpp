// Tushun() sinovi: bola erkin gapirganda (klaviatura yoki mikrofon/STT) niyat to'g'ri aniqlanadimi.
// STT (FastConformer) chiqishi: sonlar so'z bilan ("o'n ikki ming"), tutuq belgisi ʻ, tinish belgilari yo'q.
// Ishga tushirish: UnrealEditor-Cmd.exe <uproject> -ExecCmds="Automation RunTests Bozor.Tushun; Quit" -unattended -nullrhi -nosplash

#include "BozorSotuv.h"
#include "BozorQuloq.h"
#include "BozorAiMiya.h"
#include "BozorRubai.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "HttpModule.h"
#include "HttpManager.h"
#include "Containers/Ticker.h"
#include "Engine/GameInstance.h"
#include "Async/TaskGraphInterfaces.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FBozorMahsulot Mol(const TCHAR* Nomi, const TCHAR* Birlik, std::initializer_list<const TCHAR*> Sozlar)
	{
		FBozorMahsulot M;
		M.Id = M.Nomi = Nomi;
		M.Birlik = Birlik;
		for (const TCHAR* S : Sozlar) M.Sozlar.Add(S);
		return M;
	}

	struct FHol
	{
		const TCHAR* Gap;
		EBozorBosqich Bosqich;
		EBozorHarakat Kutil;
		float Qiymat;      // < 0 - tekshirilmaydi
		int32 MolI;        // -2 - tekshirilmaydi
		int32 Odob;        // -2 - tekshirilmaydi
	};

	constexpr int32 Erk = -2;   // tekshirilmaydi
	using Bq = EBozorBosqich;
	using Hr = EBozorHarakat;

	const FHol Hollar[] = {
		// salom va odob
		{TEXT("Assalomu alaykum, amaki!"), Bq::Boshlanish, Hr::Salom, -1, Erk, 1},
		{TEXT("assalomu alaykum"), Bq::Boshlanish, Hr::Salom, -1, Erk, Erk},
		{TEXT("salom aka"), Bq::Boshlanish, Hr::Salom, -1, Erk, 1},
		// narx
		{TEXT("Pomidor necha pul?"), Bq::Boshlanish, Hr::NarxAytish, -1, 0, Erk},
		{TEXT("pomidorning kilosi qancha turadi"), Bq::Boshlanish, Hr::NarxAytish, -1, 0, Erk},
		{TEXT("salom pomidor qancha"), Bq::Boshlanish, Hr::NarxAytish, -1, 0, Erk},
		{TEXT("bu necha pul"), Bq::NarxAytildi, Hr::NarxAytish, -1, Erk, Erk},
		{TEXT("pamidor qancha"), Bq::Boshlanish, Hr::NarxAytish, -1, 0, Erk},
		{TEXT("kartoshkani narxi qancha"), Bq::Boshlanish, Hr::NarxAytish, -1, 2, Erk},
		// savdolashish
		{TEXT("Qimmat ekan, ozgina arzonroq bering"), Bq::NarxAytildi, Hr::Savdolashish, -1, Erk, Erk},
		{TEXT("kechirasiz arzonroq boʻlmaydimi"), Bq::NarxAytildi, Hr::Savdolashish, -1, Erk, 1},
		{TEXT("Iltimos, o'n ming qilib bering"), Bq::NarxAytildi, Hr::Savdolashish, 10000, Erk, 1},
		{TEXT("oʻn mingdan bering"), Bq::NarxAytildi, Hr::Savdolashish, 10000, Erk, Erk},
		{TEXT("oʻn bir mingga bermaysizmi"), Bq::NarxAytildi, Hr::Savdolashish, 11000, Erk, Erk},
		{TEXT("Narxing juda qimmat-ku, insof qil"), Bq::NarxAytildi, Hr::Savdolashish, -1, Erk, Erk},
		{TEXT("ozroq tushiring"), Bq::NarxAytildi, Hr::Savdolashish, -1, Erk, Erk},
		// tortish (miqdor)
		{TEXT("Ikki kilo torting"), Bq::NarxAytildi, Hr::Tortish, 2, Erk, Erk},
		{TEXT("ikki kilo bering"), Bq::NarxAytildi, Hr::Tortish, 2, Erk, Erk},
		{TEXT("Menga uch kilo kartoshka kerak"), Bq::Boshlanish, Hr::Tortish, 3, 2, Erk},
		{TEXT("Bir yarim kilo bodring bering"), Bq::Boshlanish, Hr::Tortish, 1.5f, 1, Erk},
		{TEXT("bir yarim kilo bodring bering"), Bq::Boshlanish, Hr::Tortish, 1.5f, 1, Erk},
		{TEXT("ikki kilo pamidor"), Bq::Boshlanish, Hr::Tortish, 2, 0, Erk},
		{TEXT("yarim kilo pomidor bering"), Bq::Boshlanish, Hr::Tortish, 0.5f, 0, Erk},
		{TEXT("2 kg pomidor"), Bq::Boshlanish, Hr::Tortish, 2, 0, Erk},
		{TEXT("menga ikkita bering"), Bq::NarxAytildi, Hr::Tortish, 2, Erk, Erk},
		{TEXT("uch kilogramm torting iltimos"), Bq::NarxAytildi, Hr::Tortish, 3, Erk, 1},
		{TEXT("Hoy, tez bo'l, ikki kilo pomidor ber"), Bq::Boshlanish, Hr::Tortish, 2, 0, -1},
		{TEXT("mayli olaman"), Bq::NarxAytildi, Hr::Tortish, -1, Erk, Erk},
		// pul
		{TEXT("Mana, ellik ming"), Bq::PulKutilmoqda, Hr::PulQabul, 50000, Erk, Erk},
		{TEXT("mana yigirma ming soʻm"), Bq::PulKutilmoqda, Hr::PulQabul, 20000, Erk, Erk},
		{TEXT("mana yigirma besh ming"), Bq::PulKutilmoqda, Hr::PulQabul, 25000, Erk, Erk},
		{TEXT("oʻn ikki ming besh yuz soʻm"), Bq::PulKutilmoqda, Hr::PulQabul, 12500, Erk, Erk},
		{TEXT("yuz ming soʻm beraman"), Bq::PulKutilmoqda, Hr::PulQabul, 100000, Erk, Erk},
		{TEXT("Mana 50 000"), Bq::PulKutilmoqda, Hr::PulQabul, 50000, Erk, Erk},
		{TEXT("ikki yuz ming"), Bq::PulKutilmoqda, Hr::PulQabul, 200000, Erk, Erk},
		// tarozi va qaytim
		{TEXT("men ikki kilo soʻragandim"), Bq::TaroziTekshir, Hr::TaroziAyt, -1, Erk, Erk},
		{TEXT("koʻp tortibsiz"), Bq::TaroziTekshir, Hr::TaroziAyt, -1, Erk, Erk},
		{TEXT("mayli rahmat"), Bq::TaroziTekshir, Hr::TaroziRozi, -1, Erk, Erk},
		{TEXT("qaytim kam berdingiz"), Bq::QaytimTekshir, Hr::QaytimKam, -1, Erk, Erk},
		{TEXT("rahmat amaki"), Bq::QaytimTekshir, Hr::QaytimRozi, -1, Erk, 1},
		// mol tanlash, xayr, tushunmadi
		{TEXT("Bodring bormi?"), Bq::Boshlanish, Hr::MolTanlash, -1, 1, Erk},
		{TEXT("Rahmat, xayr!"), Bq::NarxAytildi, Hr::Xayr, -1, Erk, Erk},
		{TEXT("kerak emas rahmat"), Bq::NarxAytildi, Hr::Xayr, -1, Erk, Erk},
		{TEXT("Ob-havo qanaqa bugun?"), Bq::NarxAytildi, Hr::Tushunmadi, -1, Erk, Erk},
		// mikrofon xatolari (bir harf)
		{TEXT("assalom aleykum"), Bq::Boshlanish, Hr::Salom, -1, Erk, Erk},
		{TEXT("pamidor qanch"), Bq::Boshlanish, Hr::NarxAytish, -1, 0, Erk},
		{TEXT("arzanroq bering"), Bq::NarxAytildi, Hr::Savdolashish, -1, Erk, Erk},
		{TEXT("raxmat xayr"), Bq::NarxAytildi, Hr::Xayr, -1, Erk, Erk},
		{TEXT("necha puldan ekan"), Bq::NarxAytildi, Hr::NarxAytish, -1, Erk, Erk},
	};

	FString Nom(EBozorHarakat V) { return StaticEnum<EBozorHarakat>()->GetNameStringByValue((int64)V); }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBozorTushunTest, "Bozor.Tushun", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBozorTushunTest::RunTest(const FString& Parameters)
{
	UBozorTayyorJavob* Miya = NewObject<UBozorTayyorJavob>();
	FBozorSuhbatHolati Holat;
	Holat.Mollar = {
		Mol(TEXT("pomidor"), TEXT("kilo"), {TEXT("pomidor")}),
		Mol(TEXT("bodring"), TEXT("kilo"), {TEXT("bodring")}),
		Mol(TEXT("kartoshka"), TEXT("kilo"), {TEXT("kartoshka")}),
	};

	int32 Togri = 0;
	for (const FHol& C : Hollar)
	{
		Holat.Bosqich = C.Bosqich;
		Holat.JoriyNarx = C.Bosqich == Bq::Boshlanish ? 0 : 12000;
		Holat.Tanlangan = C.Bosqich == Bq::Boshlanish ? INDEX_NONE : 0;
		const FBozorTushuncha T = Miya->Tushun(C.Gap, Holat);
		TArray<FString> Xato;
		if (T.Harakat != C.Kutil) Xato.Add(FString::Printf(TEXT("harakat %s (kutilgan %s)"), *Nom(T.Harakat), *Nom(C.Kutil)));
		if (C.Qiymat >= 0.f && !FMath::IsNearlyEqual(T.Qiymat, C.Qiymat, 0.01f)) Xato.Add(FString::Printf(TEXT("qiymat %g (kutilgan %g)"), T.Qiymat, C.Qiymat));
		if (C.MolI != Erk && T.Mol != C.MolI) Xato.Add(FString::Printf(TEXT("mol %d (kutilgan %d)"), T.Mol, C.MolI));
		if (C.Odob != Erk && T.Odob != C.Odob) Xato.Add(FString::Printf(TEXT("odob %d (kutilgan %d)"), T.Odob, C.Odob));
		if (Xato.IsEmpty()) ++Togri;
		else AddError(FString::Printf(TEXT("\"%s\": %s"), C.Gap, *FString::Join(Xato, TEXT(", "))));
	}
	AddInfo(FString::Printf(TEXT("Tushun: %d/%d to'g'ri"), Togri, (int32)UE_ARRAY_COUNT(Hollar)));
	return true;
}

// Quloq (STT) sinovi: o'yin ichidan crispasr.exe ishga tushadimi va o'zbekcha matn (ʻ) buzilmay keladimi.
// Audio: AI/poc/audio (Common Voice, git'ga kirmaydi) - bo'lmasa sinov o'tkazib yuboriladi.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBozorQuloqTest, "Bozor.Quloq", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBozorQuloqTest::RunTest(const FString& Parameters)
{
	const FString Audio = FPaths::ProjectDir() / TEXT("AI/poc/audio/cv_153573.mp3");
	if (!UBozorQuloq::Tayyormi() || !FPaths::FileExists(Audio))
	{
		AddWarning(TEXT("AI/stt yoki sinov audiosi yo'q - o'tkazib yuborildi"));
		return true;
	}
	UBozorQuloq* Q = NewObject<UBozorQuloq>();
	Q->AddToRoot();
	TSharedRef<TOptional<FString>> Natija = MakeShared<TOptional<FString>>();
	Q->NatijaKeldi = [Natija](const FString& M) { *Natija = M; };
	Q->Fayldan(Audio);
	const double T0 = FPlatformTime::Seconds();
	while (!Natija->IsSet() && FPlatformTime::Seconds() - T0 < 20.0)
	{
		FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
		FPlatformProcess::Sleep(0.01f);
	}
	Q->RemoveFromRoot();
	if (!Natija->IsSet()) { AddError(TEXT("STT 20 s ichida javob bermadi")); return false; }
	const FString Matn = Natija->GetValue();
	AddInfo(FString::Printf(TEXT("Quloq: \"%s\" (%.0f ms)"), *Matn, (FPlatformTime::Seconds() - T0) * 1000.0));
	TestEqual(TEXT("STT matni"), Matn, FString(TEXT("Bugun tur doirasida beshta uchrashuv boʻlib oʻtadi")));
	return true;
}

// AI miya sinovi: (1) son tekshiruvi va javobni tozalash - har doim; (2) jonli: llama-server ishga tushadi,
// 4 xil fe'ldagi sotuvchi bir xil holatlarga javob beradi (AI/miya bo'lmasa - o'tkazib yuboriladi).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBozorMiyaTest, "Bozor.Miya", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBozorMiyaTest::RunTest(const FString& Parameters)
{
	// --- sonlar
	TestTrue(TEXT("bir xil son"), UBozorAiMiya::SonlarMosmi(TEXT("Bir kilo 12 000 so'm."), TEXT("Kilosi 12 000, asal!")));
	TestTrue(TEXT("bo'shliqsiz son"), UBozorAiMiya::SonlarMosmi(TEXT("7 500 so'm."), TEXT("7500. Olsangiz oling.")));
	TestTrue(TEXT("qaytim kupyuralari"), UBozorAiMiya::SonlarMosmi(TEXT("Mana: 20 000, 5 000 va 1 000 so'm."), TEXT("Oling: 20 000, 5 000 va 1 000 so'm, bolam.")));
	TestFalse(TEXT("son yo'qoldi"), UBozorAiMiya::SonlarMosmi(TEXT("24 000 so'm, yana 4 000 bering."), TEXT("24 000 so'm.")));
	TestFalse(TEXT("yangi narx to'qildi"), UBozorAiMiya::SonlarMosmi(TEXT("Narxi shu."), TEXT("Pomidor 8 000 so'mdan.")));
	TestFalse(TEXT("narx o'zgardi"), UBozorAiMiya::SonlarMosmi(TEXT("11 000 so'm qilaman."), TEXT("10 000 so'm qilaman.")));
	TestFalse(TEXT("so'z bilan son to'qildi"), UBozorAiMiya::SonlarMosmi(TEXT("Bundan arzoni yo'q. 11 500 so'm."), TEXT("11 500 so'm - asal! O'n mingdan bering!")));
	TestTrue(TEXT("so'z bilan son yo'q"), UBozorAiMiya::SonlarMosmi(TEXT("Narxi shu."), TEXT("Narxi shu, bolam, yaxshi mol.")));
	// --- tozalash
	FBozorShaxs K;
	K.Ism = TEXT("Gulnora opa");
	TestEqual(TEXT("qo'shtirnoq va prefiks"), UBozorAiMiya::Tozala(TEXT("Sen: \"qani, 7500 so'm!\""), K), FString(TEXT("Qani, 7 500 so'm!")));
	TestEqual(TEXT("o'z ismi - yaroqsiz"), UBozorAiMiya::Tozala(TEXT("Gulnora opa sizga aytadi: 5 000."), K), FString());
	TestEqual(TEXT("ikkinchi qator tashlanadi"), UBozorAiMiya::Tozala(TEXT("Mana, 2 kilo.\nIzoh: bu gap..."), K), FString(TEXT("Mana, 2 kilo.")));
	TestEqual(TEXT("kasr son buzilmaydi"), UBozorAiMiya::Tozala(TEXT("1,5 kilo - 18000 so'm."), K), FString(TEXT("1,5 kilo - 18 000 so'm.")));

	// --- jonli
	if (!UBozorAiMiya::FayllarBormi())
	{
		AddWarning(TEXT("AI/miya yo'q - jonli qism o'tkazib yuborildi"));
		return true;
	}
	UGameInstance* Gi = NewObject<UGameInstance>(GetTransientPackage());
	Gi->AddToRoot();
	UBozorAiMiya* M = NewObject<UBozorAiMiya>(Gi);
	auto Kut = [](TFunctionRef<bool()> Shart, double Sekund)
	{
		const double T0 = FPlatformTime::Seconds();
		double Oldin = T0;
		while (!Shart() && FPlatformTime::Seconds() - T0 < Sekund)
		{
			const double H = FPlatformTime::Seconds();
			FHttpModule::Get().GetHttpManager().Tick(float(H - Oldin));
			FTSTicker::GetCoreTicker().Tick(float(H - Oldin));
			FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
			Oldin = H;
			FPlatformProcess::Sleep(0.02f);
		}
		return Shart();
	};
	M->Boshla();
	const double T0 = FPlatformTime::Seconds();
	if (!Kut([M] { return M->Tayyormi(); }, 120.0))
	{
		M->Toxtat();
		Gi->RemoveFromRoot();
		AddError(TEXT("llama-server 120 s da tayyor bo'lmadi (Saved/Miya/server.log)"));
		return false;
	}
	AddInfo(FString::Printf(TEXT("Server tayyor: %.1f s"), FPlatformTime::Seconds() - T0));

	struct FHolat { EBozorFel Fel; int32 Munosabat; const TCHAR* Bola; const TCHAR* Mano; };
	const FHolat Holatlar[] = {
		{EBozorFel::Mehribon, 60, TEXT("Assalomu alaykum!"), TEXT("Va alaykum assalom, bolam! Keling, nima kerak? Bizda pomidor, bodring bor.")},
		{EBozorFel::Qattiq, 50, TEXT("pomidor necha pul"), TEXT("Bir kilo pomidor 12 000 so'm.")},
		{EBozorFel::Shoshqaloq, 50, TEXT("ikki kilo torting"), TEXT("Mana, 2 kilo pomidor. Jami 24 000 so'm bo'ldi.")},
		{EBozorFel::Ayyor, 50, TEXT("qimmat ekan, o'n mingdan bering"), TEXT("Bundan arzoni yo'q. 11 500 so'm.")},
		{EBozorFel::Mehribon, 60, TEXT("mana ellik ming"), TEXT("Rahmat! Mana qaytimingiz: 20 000, 5 000 va 1 000 so'm.")},
		{EBozorFel::Qattiq, 20, TEXT("hoy tez bo'l pomidor ber"), TEXT("Kattalarga odob bilan gapirish kerak. Narxi shu.")},
	};
	int32 Ai = 0;
	for (const FHolat& H : Holatlar)
	{
		FBozorShaxs Kim = UBozorAiMiya::Shaxs(nullptr, H.Fel);
		TSharedRef<TOptional<TPair<FString, bool>>> J = MakeShared<TOptional<TPair<FString, bool>>>();
		const double T1 = FPlatformTime::Seconds();
		M->QaytaYoz(Kim, H.Munosabat, H.Bola, H.Mano, {}, [J](const FString& Matn, bool bAi) { *J = TPair<FString, bool>(Matn, bAi); });
		if (!Kut([J] { return J->IsSet(); }, 30.0)) { AddError(FString::Printf(TEXT("javob kelmadi: %s"), H.Mano)); continue; }
		const TPair<FString, bool>& R = J->GetValue();
		Ai += R.Value ? 1 : 0;
		TestTrue(TEXT("sonlar mos"), UBozorAiMiya::SonlarMosmi(H.Mano, R.Key));
		AddInfo(FString::Printf(TEXT("%s %.0f ms [%s] %s -> %s"), R.Value ? TEXT("AI") : TEXT("tayyor"), (FPlatformTime::Seconds() - T1) * 1000.0,
			BozorSotuv::FelNomi(H.Fel), H.Mano, *R.Key));
	}
	AddInfo(FString::Printf(TEXT("AI javoblari: %d/%d"), Ai, UE_ARRAY_COUNT(Holatlar)));
	M->Toxtat();
	Gi->RemoveFromRoot();
	return true;
}


// STT taqqoslash: Saved/Quloq/*.wav (o'yinda mikrofon bilan yozilganlar) yoki AI/stt/sinov/*.wav ni ikkala model bilan taniydi.
// Natija: Saved/Quloq/taqqos.tsv (fayl, FastConformer, ms, rubaiSTT, ms). Haqiqiy gapni qo'lda yonma-yon yozib, xatolarni solishtiring
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBozorSttTaqqosTest, "Bozor.SttTaqqos", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBozorSttTaqqosTest::RunTest(const FString& Parameters)
{
	const FString Stt = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("AI/stt"));
	TArray<FString> Fayllar;
	for (const FString& Papka : { FPaths::ProjectSavedDir() / TEXT("Quloq"), FPaths::ProjectDir() / TEXT("AI/stt/sinov") })
	{
		TArray<FString> F;
		IFileManager::Get().FindFiles(F, *(Papka / TEXT("*.wav")), true, false);
		for (const FString& X : F) Fayllar.Add(FPaths::ConvertRelativePathToFull(Papka / X));
	}
	if (Fayllar.IsEmpty() || !BozorRubai::Bormi() || !FPaths::FileExists(Stt / TEXT("crispasr.exe")))
	{
		AddWarning(TEXT("yozuvlar, AI/stt/rubai yoki crispasr yo'q - o'tkazib yuborildi"));
		return true;
	}
	Fayllar.Sort();
	double T0 = FPlatformTime::Seconds();
	if (!BozorRubai::Yukla()) { AddError(TEXT("rubaiSTT yuklanmadi (logda LogBozorRubai)")); return false; }
	AddInfo(FString::Printf(TEXT("rubaiSTT yuklandi: %.1f s"), FPlatformTime::Seconds() - T0));
	FString Tsv = TEXT("fayl\tFastConformer\tms\trubaiSTT\tms\n");
	const FString Vaqtinchalik = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Quloq/_taqqos"));
	for (const FString& Fayl : Fayllar)
	{
		T0 = FPlatformTime::Seconds();
		int32 Kod = -1;
		FString Chiqdi, Xato, A;
		IFileManager::Get().Delete(*(Vaqtinchalik + TEXT(".txt")), false, true, true);
		FPlatformProcess::ExecProcess(*(Stt / TEXT("crispasr.exe")), *FString::Printf(TEXT("--backend fastconformer-ctc -m \"%s\" -f \"%s\" -t 4 -nt -ng -np --lid-backend off --no-punctuation -otxt -of \"%s\""),
			*(Stt / TEXT("stt-uz.gguf")), *Fayl, *Vaqtinchalik), &Kod, &Chiqdi, &Xato, *Stt);
		FFileHelper::LoadFileToString(A, *(Vaqtinchalik + TEXT(".txt")));
		A.TrimStartAndEndInline();
		const double MsA = (FPlatformTime::Seconds() - T0) * 1000.0;
		T0 = FPlatformTime::Seconds();
		TArray<float> Pcm;
		bool bOk = false;
		const FString B = BozorRubai::WavOqi(Fayl, Pcm) ? BozorRubai::Tanit(Pcm, bOk) : FString(TEXT("(WAV emas)"));
		const double MsB = (FPlatformTime::Seconds() - T0) * 1000.0;
		AddInfo(FString::Printf(TEXT("%s (%.1f s) | FastConformer %.0f ms: %s | rubaiSTT %.0f ms: %s"), *FPaths::GetCleanFilename(Fayl), Pcm.Num() / 16000.f, MsA, *A, MsB, *B));
		Tsv += FString::Printf(TEXT("%s\t%s\t%.0f\t%s\t%.0f\n"), *FPaths::GetCleanFilename(Fayl), *A, MsA, *B, MsB);
	}
	FFileHelper::SaveStringToFile(Tsv, *(FPaths::ProjectSavedDir() / TEXT("Quloq/taqqos.tsv")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	return true;
}

#endif
