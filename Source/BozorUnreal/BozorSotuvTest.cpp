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
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "HAL/IConsoleManager.h"
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
		// "bitta" (o'yinda sotuvchi tushunmay "Nechta?" deb qayta-qayta so'ragan gaplar, 2026-11-16)
		{TEXT("ha, unda bitta bering, aka."), Bq::NarxAytildi, Hr::Tortish, 1, Erk, 1},
		{TEXT("aka, bitta non bering. zabil qilmang, bo'ldi, bitta non bering."), Bq::NarxAytildi, Hr::Tortish, 1, Erk, 1},
		{TEXT("xo'p, shuni ham bitta berang."), Bq::NarxAytildi, Hr::Tortish, 1, Erk, Erk},
		{TEXT("bittagina pomidor bering"), Bq::Boshlanish, Hr::Tortish, 1, 0, Erk},
		{TEXT("uchta bodring bering"), Bq::Boshlanish, Hr::Tortish, 3, 1, Erk},
		// qo'pollik: haqorat, "sen", yalang buyruq - odob so'zidan ustun
		{TEXT("ikki kilo pomidor ber"), Bq::Boshlanish, Hr::Tortish, 2, 0, -1},
		{TEXT("sen ahmoqmisan, pomidor necha pul"), Bq::Boshlanish, Hr::NarxAytish, -1, 0, -1},
		{TEXT("aka, siz tentakmisiz, pomidor qancha"), Bq::Boshlanish, Hr::NarxAytish, -1, 0, -1},
		{TEXT("arzon qil, xasis"), Bq::NarxAytildi, Hr::Savdolashish, -1, Erk, -1},
		{TEXT("pomidor qancha, iltimos"), Bq::Boshlanish, Hr::NarxAytish, -1, 0, 1},
		{TEXT("opajon, bodring necha pul?"), Bq::Boshlanish, Hr::NarxAytish, -1, 1, 1},
		{TEXT("bodring necha pul"), Bq::Boshlanish, Hr::NarxAytish, -1, 1, 0},
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

// Odob: sotuvchi bolaning ohangiga munosabat bildiradimi - qo'pol gapga har doim tanbeh (QopolIzoh), munosabat tushadi;
// odobli gapga ba'zan maqtov (OdobIzoh), ketma-ket emas; oddiy gapga hech narsa
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBozorOdobTest, "Bozor.Odob", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBozorOdobTest::RunTest(const FString& Parameters)
{
	TMap<FString, TArray<FString>> Umumiy = {
		{TEXT("Salom"), {TEXT("Vaalaykum assalom.")}}, {TEXT("Narx"), {TEXT("Bir {birlik} {mahsulot} {narx} so'm.")}},
		{TEXT("KutibOlish"), {TEXT("Keling.")}}, {TEXT("SalomBering"), {TEXT("Salom bering.")}},
		{TEXT("QopolIzoh"), {TEXT("UMUMIY-TANBEH.")}}, {TEXT("OdobIzoh"), {TEXT("UMUMIY-MAQTOV.")}}};
	FBozorRasta R;
	R.Mollar = {Mol(TEXT("pomidor"), TEXT("kilo"), {TEXT("pomidor")})};
	R.Mollar[0].Narx = 10000;
	R.Gaplar = {{TEXT("QopolIzoh"), {TEXT("FEL-TANBEH.")}}, {TEXT("OdobIzoh"), {TEXT("FEL-MAQTOV.")}}};
	R.Asosiy = &Umumiy;
	UBozorHamyon* Hamyon = NewObject<UBozorHamyon>();
	UBozorSuhbat* S = NewObject<UBozorSuhbat>();
	S->Boshla(R, Hamyon, NewObject<UBozorTayyorJavob>(), 50);
	S->Yoz(TEXT("assalomu alaykum"));
	const FString Oddiy = S->Yoz(TEXT("pomidor necha pul"));
	TestFalse(TEXT("oddiy gapga munosabat yo'q"), Oddiy.Contains(TEXT("TANBEH")) || Oddiy.Contains(TEXT("MAQTOV")));
	const int32 M0 = S->GetHolat().Munosabat;
	const FString Qopol = S->Yoz(TEXT("pomidor necha pul, ahmoq"));
	TestTrue(FString::Printf(TEXT("qo'pol gapga fe'l tanbehi: \"%s\""), *Qopol), Qopol.StartsWith(TEXT("FEL-TANBEH.")) && S->GetOxirgiIzoh() == TEXT("FEL-TANBEH."));
	TestTrue(TEXT("qo'pol gapdan keyin munosabat tushdi"), S->GetHolat().Munosabat < M0);
	TestEqual(TEXT("oxirgi odob -1"), S->GetOxirgiOdob(), -1);
	// odobli gap 30 marta: maqtov ba'zan (40%), lekin hech qachon ketma-ket emas
	int32 Maqtov = 0;
	bool bOldin = false, bKetma = false;
	for (int32 i = 0; i < 30; ++i)
	{
		const bool bBor = S->Yoz(TEXT("iltimos, pomidor necha pul")).StartsWith(TEXT("FEL-MAQTOV."));
		Maqtov += bBor ? 1 : 0;
		bKetma |= bBor && bOldin;
		bOldin = bBor;
	}
	TestTrue(FString::Printf(TEXT("odobli gapga ba'zan maqtov (%d/30)"), Maqtov), Maqtov >= 3 && Maqtov <= 20);
	TestFalse(TEXT("maqtov ketma-ket takrorlanmaydi"), bKetma);
	AddInfo(FString::Printf(TEXT("qo'pol: \"%s\" | maqtov %d/30 | munosabat %d -> %d"), *Qopol, Maqtov, M0, S->GetHolat().Munosabat));
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
	// bolaga haqorat/haydash - yaroqsiz (bulut sinovida qattiq sotuvchi shunday degan, 2026-11-18)
	TestEqual(TEXT("haydash - yaroqsiz"), UBozorAiMiya::Tozala(TEXT("Ana ol, 2 000 so'ming. Bor yo'kol."), K), FString());
	TestEqual(TEXT("yo ket - yaroqsiz"), UBozorAiMiya::Tozala(TEXT("Narxi shu, tushmaydi. Olasanmi yo ket!"), K), FString());
	TestEqual(TEXT("yo'qol (ʻ) - yaroqsiz"), UBozorAiMiya::Tozala(TEXT("Bor, yoʻqol endi."), K), FString());
	TestEqual(TEXT("haqorat - yaroqsiz"), UBozorAiMiya::Tozala(TEXT("Ahmoqmisan, narxi shu."), K), FString());
	TestEqual(TEXT("oddiy qattiq gap qoladi"), UBozorAiMiya::Tozala(TEXT("Narxi shu, tushmaydi. Olasizmi?"), K), FString(TEXT("Narxi shu, tushmaydi. Olasizmi?")));
	TestEqual(TEXT("'pomidor endi' - haydash emas"), UBozorAiMiya::Tozala(TEXT("Pomidor endi tugadi, bodring bor."), K), FString(TEXT("Pomidor endi tugadi, bodring bor.")));

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


// Xulq-atvor: bir xil ma'noni har xil sotuvchilarga (xulq.json) LLM orqali aytdiradi - javoblar har birining o'z uslubida
// bo'lishi va sonlar o'zgarmasligi kerak. Natija: Saved/Miya/xulq_sinov.tsv (sotuvchi, ma'no, javob, AI/tayyor)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBozorXulqTest, "Bozor.Xulq", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBozorXulqTest::RunTest(const FString& Parameters)
{
	FString Matn;
	TSharedPtr<FJsonObject> Ildiz;
	const FString Yol = FPaths::ProjectContentDir() / TEXT("Bozor/Data/xulq.json");
	if (!FFileHelper::LoadFileToString(Matn, *Yol) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Matn), Ildiz) || !Ildiz)
	{
		AddError(TEXT("xulq.json o'qilmadi"));
		return false;
	}
	const TSharedPtr<FJsonObject> Sotuvchilar = Ildiz->GetObjectField(TEXT("sotuvchilar"));
	TestTrue(TEXT("sotuvchilar bor"), Sotuvchilar.IsValid() && Sotuvchilar->Values.Num() > 0);
	// har yozuv: ism, tavsif, kamida bitta namuna, fe'l to'g'ri
	TArray<FBozorShaxs> Shaxslar;
	for (const auto& P : Sotuvchilar->Values)
	{
		const TSharedPtr<FJsonObject> O = P.Value->AsObject();
		FBozorShaxs K;
		BozorSotuv::XulqniOqi(*O, K.Xulq);
		K.Ism = K.Xulq.Ism;
		K.Yosh = K.Xulq.Yosh;
		K.bAyol = K.Ism.EndsWith(TEXT(" opa")) || K.Ism.EndsWith(TEXT(" xola"));
		FString Fel;
		O->TryGetStringField(TEXT("fel"), Fel);
		const int64 V = StaticEnum<EBozorFel>()->GetValueByNameString(Fel.Left(1).ToUpper() + Fel.Mid(1));
		TestTrue(FString::Printf(TEXT("%s: fe'l '%s' to'g'ri"), *P.Key, *Fel), V != INDEX_NONE);
		if (V != INDEX_NONE) K.Fel = static_cast<EBozorFel>(V);
		TestTrue(FString::Printf(TEXT("%s: ism, tavsif va namuna bor"), *P.Key), !K.Ism.IsEmpty() && K.Xulq.Bormi() && K.Xulq.Namunalar.Num() > 0);
		for (const TPair<FString, FString>& N : K.Xulq.Namunalar)
			TestTrue(FString::Printf(TEXT("%s: namunadagi sonlar ma'noga mos (%s)"), *P.Key, *N.Value), UBozorAiMiya::SonlarMosmi(N.Key, N.Value));
		Shaxslar.Add(MoveTemp(K));
	}
	AddInfo(FString::Printf(TEXT("%d ta sotuvchi xulqi"), Shaxslar.Num()));

	if (!UBozorAiMiya::FayllarBormi())
	{
		AddWarning(TEXT("AI/miya yo'q - LLM qismi o'tkazib yuborildi"));
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
	if (!Kut([M] { return M->Tayyormi(); }, 120.0))
	{
		M->Toxtat();
		Gi->RemoveFromRoot();
		AddError(TEXT("llama-server 120 s da tayyor bo'lmadi"));
		return false;
	}
	// sinovda kutish ko'proq (videokarta band bo'lsa ham javob kelsin)
	IConsoleVariable* Kutish = IConsoleManager::Get().FindConsoleVariable(TEXT("bozor.MiyaKutish"));
	const float EskiKutish = Kutish ? Kutish->GetFloat() : 4.f;
	if (Kutish) Kutish->Set(15.f);
	const TCHAR* Manolar[] = {
		TEXT("Bir kilo olma 15 000 so'm."),
		TEXT("Bundan arzoni yo'q. 12 500 so'm."),
		TEXT("Yana keling!") };
	FString Tsv = TEXT("sotuvchi\tfe'l\tma'no\tjavob\tAI\n");
	int32 Ai = 0, Jami = 0;
	for (const FBozorShaxs& K : Shaxslar)
		for (const TCHAR* Mano : Manolar)
		{
			TSharedRef<TOptional<TPair<FString, bool>>> J = MakeShared<TOptional<TPair<FString, bool>>>();
			M->QaytaYoz(K, K.Xulq.BoshMunosabat, TEXT(""), Mano, {}, [J](const FString& Javob, bool bAi) { *J = TPair<FString, bool>(Javob, bAi); });
			if (!Kut([J] { return J->IsSet(); }, 30.0)) { AddError(FString::Printf(TEXT("%s: javob kelmadi"), *K.Ism)); continue; }
			const TPair<FString, bool>& R = J->GetValue();
			++Jami;
			Ai += R.Value ? 1 : 0;
			TestTrue(TEXT("sonlar mos"), UBozorAiMiya::SonlarMosmi(Mano, R.Key));
			AddInfo(FString::Printf(TEXT("%s [%s] %s -> %s%s"), *K.Ism, BozorSotuv::FelNomi(K.Fel), Mano, *R.Key, R.Value ? TEXT("") : TEXT(" (tayyor)")));
			Tsv += FString::Printf(TEXT("%s\t%s\t%s\t%s\t%d\n"), *K.Ism, BozorSotuv::FelNomi(K.Fel), Mano, *R.Key, R.Value ? 1 : 0);
		}
	// bolaning ohangi: bir xil ma'no qo'pol (-1), oddiy (0) va odobli (1) gapga - sotuvchi har birida boshqacha munosabat bildirishi kerak
	for (const TCHAR* Ism : {TEXT("Tursun ota"), TEXT("Gulchehra opa"), TEXT("Jasur aka"), TEXT("Doniyor aka")})
	{
		const FBozorShaxs* K = Shaxslar.FindByPredicate([Ism](const FBozorShaxs& X) { return X.Ism == Ism; });
		if (!K) continue;
		for (int32 Odob = -1; Odob <= 1; ++Odob)
		{
			// munosabat gapi (QopolIzoh/OdobIzoh) LLM'ga berilmaydi (ABozorOyinchi uni javob oldiga qo'shadi) - bu yerda faqat
			// qo'pol gapdagi jiddiy ohang va savolning saqlanishi
			const TCHAR* Mano = TEXT("Bir kilo pomidor 8 500 so'm. Necha kilo kerak?");
			TSharedRef<TOptional<TPair<FString, bool>>> J = MakeShared<TOptional<TPair<FString, bool>>>();
			M->QaytaYoz(*K, 50, TEXT(""), Mano, {}, [J](const FString& Javob, bool bAi) { *J = TPair<FString, bool>(Javob, bAi); }, Odob);
			if (!Kut([J] { return J->IsSet(); }, 30.0)) continue;
			const TPair<FString, bool>& R = J->GetValue();
			++Jami;
			Ai += R.Value ? 1 : 0;
			TestTrue(TEXT("savol saqlangan"), R.Key.Contains(TEXT("?")));
			AddInfo(FString::Printf(TEXT("[ohang %+d] %s -> %s%s"), Odob, Ism, *R.Key, R.Value ? TEXT("") : TEXT(" (tayyor)")));
			Tsv += FString::Printf(TEXT("%s (ohang %+d)\t%s\t%s\t%s\t%d\n"), Ism, Odob, BozorSotuv::FelNomi(K->Fel), Mano, *R.Key, R.Value ? 1 : 0);
		}
	}
	// erkin suhbat (ovozli chat): savdodan tashqari gaplar - sotuvchi o'z shaxsida javob beradi, son/narx aytmaydi, tarixni eslaydi
	struct FErkin { const TCHAR* Ism; const TCHAR* Mollar; TArray<const TCHAR*> Gaplar; };
	const FErkin Erkinlar[] = {
		{TEXT("Qodir ota"), TEXT("sarimsoq"), {TEXT("Ota, siz bu bozorda qachondan beri ishlaysiz?"), TEXT("Oldin bozor qanaqa edi?"), TEXT("Rahmat, qiziq ekan.")}},
		{TEXT("Sherzod aka"), TEXT("ko'kat"), {TEXT("Aka, ko'katlaringiz qayerdan keladi?"), TEXT("Rostdanmi? Tog'dan-a?")}},
		{TEXT("Mohira opa"), TEXT("olma, anor, behi"), {TEXT("Opa, bugun kayfiyatingiz zo'r-ku!"), TEXT("Qaysi mevangiz eng shirin?")}},
		{TEXT("Tursun ota"), TEXT("limon, apelsin"), {TEXT("Ota, nega xafa ko'rinasiz?")}},
	};
	int32 ErkinAi = 0, ErkinJami = 0;
	for (const FErkin& E : Erkinlar)
	{
		const FBozorShaxs* K = Shaxslar.FindByPredicate([&E](const FBozorShaxs& X) { return X.Ism == E.Ism; });
		if (!K) continue;
		TArray<TPair<FString, FString>> Tarix;
		for (const TCHAR* Gap : E.Gaplar)
		{
			TSharedRef<TOptional<TPair<FString, bool>>> J = MakeShared<TOptional<TPair<FString, bool>>>();
			M->Suhbatlash(*K, 50, Tarix, Gap, E.Mollar, TEXT("Tushunmadim."), [J](const FString& Javob, bool bAi) { *J = TPair<FString, bool>(Javob, bAi); });
			if (!Kut([J] { return J->IsSet(); }, 30.0)) continue;
			const TPair<FString, bool>& R = J->GetValue();
			++ErkinJami;
			ErkinAi += R.Value ? 1 : 0;
			// erkin javobda narx bo'lmasin ("qirq yil" - mayli, "o'n ming so'm" - yo'q)
			TestFalse(TEXT("erkin javobda narx yo'q"), R.Value && R.Key.Contains(TEXT("so'm")) && !UBozorAiMiya::SonlarMosmi(FString(), R.Key));
			AddInfo(FString::Printf(TEXT("[erkin] %s | bola: %s -> %s%s"), E.Ism, Gap, *R.Key, R.Value ? TEXT("") : TEXT(" (tayyor)")));
			Tsv += FString::Printf(TEXT("%s (erkin)\t%s\t%s\t%s\t%d\n"), E.Ism, BozorSotuv::FelNomi(K->Fel), Gap, *R.Key, R.Value ? 1 : 0);
			Tarix.Emplace(Gap, R.Key);
		}
	}
	AddInfo(FString::Printf(TEXT("Erkin suhbat AI javoblari: %d/%d"), ErkinAi, ErkinJami));
	if (Kutish) Kutish->Set(EskiKutish);
	AddInfo(FString::Printf(TEXT("AI javoblari: %d/%d"), Ai, Jami));
	FFileHelper::SaveStringToFile(Tsv, *(FPaths::ProjectSavedDir() / TEXT("Miya/xulq_sinov.tsv")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	M->Toxtat();
	Gi->RemoveFromRoot();
	return true;
}

#endif

// Savdolashuv: bola gapi va suhbat tarixi bilan (bulut AI) - har fe'ldagi sotuvchi bolaning gapiga o'z xarakterida munosabat
// bildiradi, ma'nodagi sonlar o'zgarmaydi. Bozor.Xulq dan alohida: u 300 s lik test limitiga yaqin
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBozorSavdolashTest, "Bozor.Savdolash", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBozorSavdolashTest::RunTest(const FString& Parameters)
{
	FString Matn;
	TSharedPtr<FJsonObject> Ildiz;
	if (!FFileHelper::LoadFileToString(Matn, *(FPaths::ProjectContentDir() / TEXT("Bozor/Data/xulq.json"))) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Matn), Ildiz) || !Ildiz)
	{
		AddError(TEXT("xulq.json o'qilmadi"));
		return false;
	}
	TArray<FBozorShaxs> Shaxslar;
	for (const auto& P : Ildiz->GetObjectField(TEXT("sotuvchilar"))->Values)
	{
		const TSharedPtr<FJsonObject> O = P.Value->AsObject();
		FBozorShaxs K;
		BozorSotuv::XulqniOqi(*O, K.Xulq);
		K.Ism = K.Xulq.Ism;
		K.Yosh = K.Xulq.Yosh;
		K.bAyol = K.Ism.EndsWith(TEXT(" opa")) || K.Ism.EndsWith(TEXT(" xola"));
		FString Fel;
		O->TryGetStringField(TEXT("fel"), Fel);
		const int64 V = StaticEnum<EBozorFel>()->GetValueByNameString(Fel.Left(1).ToUpper() + Fel.Mid(1));
		if (V != INDEX_NONE) K.Fel = static_cast<EBozorFel>(V);
		TestTrue(FString::Printf(TEXT("%s: savdo uslubi, kayfiyat va hayot bor"), *P.Key), !K.Xulq.SavdoUslubi.IsEmpty() && !K.Xulq.Kayfiyat.IsEmpty() && K.Xulq.Hayot.Num() > 0);
		Shaxslar.Add(MoveTemp(K));
	}
	if (!UBozorAiMiya::FayllarBormi())
	{
		AddWarning(TEXT("AI miya yo'q - LLM qismi o'tkazib yuborildi"));
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
	if (!Kut([M] { return M->Tayyormi(); }, 120.0))
	{
		M->Toxtat();
		Gi->RemoveFromRoot();
		AddError(TEXT("AI miya 120 s da tayyor bo'lmadi"));
		return false;
	}
	// savdolashuv: bola gapi va suhbat tarixi bilan - sotuvchi bolaning gapiga o'z xarakterida munosabat bildiradi, sonlar o'zgarmaydi
	struct FNavbat { const TCHAR* Bola; const TCHAR* Mano; int32 Savdolashuv; };
	const FNavbat Navbatlar[] = {
		{TEXT("Assalomu alaykum! Pomidor necha pul?"), TEXT("Vaalaykum assalom. Bir kilo pomidor 12 000 so'm."), 0},
		{TEXT("Voy, juda qimmat-ku, arzonroq qiling"), TEXT("Mayli, 11 000 so'm qilaman."), 1},
		{TEXT("Yana ozgina tushiring, iltimos"), TEXT("Bundan arzoni yo'q. 11 000 so'm."), 2},
		{TEXT("Pomidoringiz juda chiroyli ekan!"), TEXT("Rahmat! Necha kilo beray?"), 2},
		{TEXT("Ikki kilo bering"), TEXT("Mana, 2 kilo pomidor. Jami 22 000 so'm bo'ldi."), 2},
	};
	int32 SavdoAi = 0, SavdoJami = 0;
	for (const TCHAR* Ism : {TEXT("Sherzod aka"), TEXT("Erkin aka"), TEXT("Gulchehra opa"), TEXT("Ilhom aka")})
	{
		const FBozorShaxs* K = Shaxslar.FindByPredicate([Ism](const FBozorShaxs& X) { return X.Ism == Ism; });
		if (!K) continue;
		AddInfo(FString::Printf(TEXT("[savdo] %s - bugun: %s"), Ism, *K->Xulq.Kayfiyat));
		TArray<TPair<FString, FString>> Tarix;
		for (const FNavbat& N : Navbatlar)
		{
			TSharedRef<TOptional<TPair<FString, bool>>> J = MakeShared<TOptional<TPair<FString, bool>>>();
			// o'yindagi kabi: mol nomi faqat ma'noda bo'lsa javobda ham qolishi shart (ABozorOyinchi::SotuvchiAytdi)
			TArray<FString> Saqla;
			if (FString(N.Mano).Contains(TEXT("pomidor"))) Saqla.Add(TEXT("pomidor"));
			M->QaytaYoz(*K, 50, N.Bola, N.Mano, Saqla,[J](const FString& Javob, bool bAi) { *J = TPair<FString, bool>(Javob, bAi); }, 0, &Tarix, N.Savdolashuv);
			if (!Kut([J] { return J->IsSet(); }, 30.0)) continue;
			const TPair<FString, bool>& R = J->GetValue();
			++SavdoJami;
			SavdoAi += R.Value ? 1 : 0;
			TestTrue(TEXT("savdo: sonlar mos"), UBozorAiMiya::SonlarMosmi(N.Mano, R.Key));
			AddInfo(FString::Printf(TEXT("[savdo] %s | bola: %s -> %s%s"), Ism, N.Bola, *R.Key, R.Value ? TEXT("") : TEXT(" (tayyor)")));
			Tarix.Emplace(N.Bola, R.Key);
		}
	}
	AddInfo(FString::Printf(TEXT("Savdolashuv AI javoblari: %d/%d"), SavdoAi, SavdoJami));
	M->Toxtat();
	Gi->RemoveFromRoot();
	return true;
}
