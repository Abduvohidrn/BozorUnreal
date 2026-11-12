// Tushun() sinovi: bola erkin gapirganda (klaviatura yoki mikrofon/STT) niyat to'g'ri aniqlanadimi.
// STT (FastConformer) chiqishi: sonlar so'z bilan ("o'n ikki ming"), tutuq belgisi ʻ, tinish belgilari yo'q.
// Ishga tushirish: UnrealEditor-Cmd.exe <uproject> -ExecCmds="Automation RunTests Bozor.Tushun; Quit" -unattended -nullrhi -nosplash

#include "BozorSotuv.h"
#include "BozorQuloq.h"
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

#endif
