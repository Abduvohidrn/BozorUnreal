#include "BozorSotuv.h"
#include "BozorNpc.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Dom/JsonObject.h"
#include "GameFramework/GameModeBase.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogBozorSotuv, Log, All);

namespace
{
	// kichik harf, tutuq belgilarining hammasi ', tinish belgilari bo'shliq
	FString Normal(const FString& In)
	{
		FString S = In.ToLower();
		for (TCHAR& C : S)
		{
			if (C == 0x2BB || C == 0x2BC || C == 0x2018 || C == 0x2019 || C == TEXT('`') || C == 0xB4) C = TEXT('\'');
			else if (!FChar::IsAlnum(C) && C != TEXT('\'') && C != TEXT('.') && C != TEXT(',')) C = TEXT(' ');
		}
		return S;
	}

	bool Bor(const FString& S, std::initializer_list<const TCHAR*> Sozlar)
	{
		for (const TCHAR* W : Sozlar)
			if (S.Contains(W)) return true;
		return false;
	}

	bool Raqam(const FString& W) { return W.Len() > 0 && W[0] >= TEXT('0') && W[0] <= TEXT('9'); }

	// sonni bildiruvchi so'z: birlik/o'nlik qo'shiladi, yuz/ming/million ko'paytiradi, yarim - 0.5
	enum class ESonTur : uint8 { Yoq, Qosh, Yuz, Ming, Million, Yarim };
	struct FSonSoz { ESonTur Tur = ESonTur::Yoq; float Qiymat = 0.f; bool bQoshimcha = false; bool bTa = false; };

	// "ikki", "ikkita", "mingdan", "o'nga" - so'z o'zagi va ruxsat etilgan qo'shimcha ("uchun", "biroz" son emas)
	FSonSoz SonSoz(FString W)
	{
		while (W.Len() > 1 && (W.EndsWith(TEXT(".")) || W.EndsWith(TEXT(",")))) W.LeftChopInline(1);
		struct FOzak { const TCHAR* Soz; ESonTur Tur; float Qiymat; };
		static const FOzak Ozaklar[] = {
			{TEXT("bir"), ESonTur::Qosh, 1}, {TEXT("ikki"), ESonTur::Qosh, 2}, {TEXT("uch"), ESonTur::Qosh, 3},
			{TEXT("to'rt"), ESonTur::Qosh, 4}, {TEXT("tort"), ESonTur::Qosh, 4}, {TEXT("besh"), ESonTur::Qosh, 5},
			{TEXT("olti"), ESonTur::Qosh, 6}, {TEXT("yetti"), ESonTur::Qosh, 7}, {TEXT("sakkiz"), ESonTur::Qosh, 8},
			{TEXT("to'qqiz"), ESonTur::Qosh, 9}, {TEXT("toqqiz"), ESonTur::Qosh, 9},
			{TEXT("o'n"), ESonTur::Qosh, 10}, {TEXT("on"), ESonTur::Qosh, 10}, {TEXT("yigirma"), ESonTur::Qosh, 20},
			{TEXT("o'ttiz"), ESonTur::Qosh, 30}, {TEXT("ottiz"), ESonTur::Qosh, 30}, {TEXT("qirq"), ESonTur::Qosh, 40},
			{TEXT("ellik"), ESonTur::Qosh, 50}, {TEXT("oltmish"), ESonTur::Qosh, 60}, {TEXT("yetmish"), ESonTur::Qosh, 70},
			{TEXT("sakson"), ESonTur::Qosh, 80}, {TEXT("to'qson"), ESonTur::Qosh, 90}, {TEXT("toqson"), ESonTur::Qosh, 90},
			{TEXT("yuz"), ESonTur::Yuz, 100}, {TEXT("ming"), ESonTur::Ming, 1000}, {TEXT("million"), ESonTur::Million, 1e6f},
			{TEXT("mln"), ESonTur::Million, 1e6f}, {TEXT("yarim"), ESonTur::Yarim, 0.5f},
			// "bitta" ("bir"+"ta" emas, "bit" yolg'iz son emas - pastda faqat "ta..." bilan)
			{TEXT("bit"), ESonTur::Qosh, 1}};
		static const TCHAR* Qoshimchalar[] = {TEXT("ta"), TEXT("tasi"), TEXT("tasini"), TEXT("tadan"), TEXT("dan"), TEXT("ga"), TEXT("ni"), TEXT("lik"), TEXT("talik"), TEXT("ning"),
			TEXT("tagina"), TEXT("gina"), TEXT("tasidan")};
		// eng uzun o'zak birinchi ("oltmish" "olti"dan oldin)
		FSonSoz Eng;
		int32 EngUzun = 0;
		for (const auto& O : Ozaklar)
		{
			const int32 L = FCString::Strlen(O.Soz);
			if (L <= EngUzun || !W.StartsWith(O.Soz, ESearchCase::CaseSensitive)) continue;
			const FString Qolgan = W.RightChop(L);
			bool bOk = Qolgan.IsEmpty();
			for (const TCHAR* Q : Qoshimchalar) bOk |= Qolgan == Q;
			if (FCString::Strcmp(O.Soz, TEXT("bit")) == 0) bOk = Qolgan.StartsWith(TEXT("ta"));   // "bitta", "bittasini"; "bit" emas
			if (!bOk) continue;
			Eng = FSonSoz();
			Eng.Tur = O.Tur;
			Eng.Qiymat = O.Qiymat;
			Eng.bQoshimcha = !Qolgan.IsEmpty();
			Eng.bTa = Qolgan.StartsWith(TEXT("ta"));
			EngUzun = L;
		}
		return Eng;
	}

	// "12 000", "12.000", "12 ming" -> 12000; "1,5" -> 1.5; "bir yarim" -> 1.5;
	// "o'n ikki ming besh yuz" -> 12500; "ikki yuz ming" -> 200000; "ikkita" -> 2 (bTa - dona)
	struct FSon { float Qiymat; int32 Joy; bool bTa = false; };
	TArray<FSon> Sonlar(const TArray<FString>& T)
	{
		TArray<FSon> Out;
		for (int32 i = 0; i < T.Num(); ++i)
		{
			const int32 Boshi = i;
			double Jami = 0.0, Joriy = 0.0;
			bool bBor = false, bTa = false;
			if (Raqam(T[i]))
			{
				FString W = T[i];
				const int32 Nuqta = FMath::Max(W.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromEnd), W.Find(TEXT(","), ESearchCase::CaseSensitive, ESearchDir::FromEnd));
				if (Nuqta != INDEX_NONE && W.Len() - Nuqta - 1 == 3) W = W.Replace(TEXT("."), TEXT("")).Replace(TEXT(","), TEXT(""));
				Joriy = FCString::Atof(*W.Replace(TEXT(","), TEXT(".")));
				while (i + 1 < T.Num() && T[i + 1].Len() == 3 && T[i + 1].IsNumeric()) Joriy = Joriy * 1000.0 + FCString::Atof(*T[++i]);
				bBor = true;
				++i;
			}
			for (; i < T.Num(); ++i)
			{
				const FSonSoz S = SonSoz(T[i]);
				if (S.Tur == ESonTur::Yoq) break;
				switch (S.Tur)
				{
				case ESonTur::Qosh:
				case ESonTur::Yarim: Joriy += S.Qiymat; break;
				case ESonTur::Yuz: Joriy = (Joriy > 0.0 ? Joriy : 1.0) * 100.0; break;
				default: Jami += (Joriy > 0.0 ? Joriy : 1.0) * S.Qiymat; Joriy = 0.0; break;   // ming, million
				}
				bBor = true;
				bTa |= S.bTa;
				if (S.bQoshimcha) { ++i; break; }   // "mingdan", "ikkita" - son shu yerda tugaydi
			}
			if (bBor) Out.Add({float(Jami + Joriy), Boshi, bTa});
			i = FMath::Max(Boshi, i - 1);   // tashqi tsikl ++i qiladi
		}
		return Out;
	}

	// tovush/xato yozuv: "pamidor" ~ "pomidor" (5+ harfli so'zda bitta harf farqi)
	int32 Farq(const FString& A, const FString& B)
	{
		TArray<int32> D, Y;
		D.SetNum(B.Len() + 1);
		Y.SetNum(B.Len() + 1);
		for (int32 j = 0; j <= B.Len(); ++j) D[j] = j;
		for (int32 i = 1; i <= A.Len(); ++i)
		{
			Y[0] = i;
			for (int32 j = 1; j <= B.Len(); ++j)
				Y[j] = FMath::Min3(D[j] + 1, Y[j - 1] + 1, D[j - 1] + (A[i - 1] == B[j - 1] ? 0 : 1));
			Swap(D, Y);
		}
		return D[B.Len()];
	}

	// gap so'zi (qo'shimchasi bilan: "pamidorni") mol so'zining 1 harf farqli shakli bilan boshlanadimi
	bool TaxminanBor(const FString& Mol, const FString& W)
	{
		if (Mol.Len() < 5 || W.Len() < Mol.Len() - 1) return false;
		for (int32 L = Mol.Len() - 1; L <= Mol.Len() + 1; ++L)
			if (L <= W.Len() && Farq(W.Left(L), Mol) <= 1) return true;
		return false;
	}

	// gapda shu so'z bilan boshlanadigan so'z bormi ("pomidordan" ham mos). "olma" "olmayman" bilan adashmasin
	bool SozBor(const FString& S, const FString& Soz)
	{
		static const TCHAR* Emas[] = {TEXT("olmay"), TEXT("olmas"), TEXT("olmang")};
		const FString Kalit = TEXT(" ") + Soz;
		for (int32 Joy = S.Find(Kalit); Joy != INDEX_NONE; Joy = S.Find(Kalit, ESearchCase::CaseSensitive, ESearchDir::FromStart, Joy + 1))
		{
			int32 Oxir = S.Find(TEXT(" "), ESearchCase::CaseSensitive, ESearchDir::FromStart, Joy + 1);
			const FString Token = S.Mid(Joy + 1, Oxir == INDEX_NONE ? MAX_int32 : Oxir - Joy - 1);
			bool bEmas = false;
			for (const TCHAR* E : Emas) bEmas |= Token.StartsWith(E);
			if (!bEmas) return true;
		}
		return false;
	}

	const FString& Tanla(const TArray<FString>& V) { return V[FMath::RandRange(0, V.Num() - 1)]; }

	FString MollarRoyxati(const TArray<FBozorMahsulot>& M)
	{
		FString S;
		for (int32 i = 0; i < M.Num(); ++i)
			S += (i == 0 ? TEXT("") : (i == M.Num() - 1 ? TEXT(" va ") : TEXT(", "))) + M[i].Nomi;
		return S;
	}

	int32 Yaxlit500(double V) { return FMath::Max(500, FMath::RoundToInt(V / 500.0) * 500); }
}

FString BozorSotuv::Som(int64 N)
{
	FString S = FString::Printf(TEXT("%lld"), FMath::Abs(N));
	for (int32 i = S.Len() - 3; i > 0; i -= 3) S.InsertAt(i, TEXT(' '));
	return N < 0 ? TEXT("-") + S : S;
}

FString BozorSotuv::Miqdor(float M)
{
	FString S = FString::SanitizeFloat(FMath::RoundToFloat(M * 10.f) / 10.f);
	S.RemoveFromEnd(TEXT(".0"));
	return S.Replace(TEXT("."), TEXT(","));
}

// ================= hamyon: kupyuralar

UBozorHamyon::UBozorHamyon()
{
	Pul = 0;
	for (const auto& K : Kupyura) Pul += K.Key * K.Value;
	BoshlangichPul = Pul;
}

void UBozorHamyon::Boshla(const FBozorDaraja& D)
{
	Kupyura = D.Kupyura;
	Pul = 0;
	for (const auto& K : Kupyura) Pul += K.Key * K.Value;
	BoshlangichPul = Pul;
	Royxat = D.Royxat;
	Eslatma = D.Eslatma;
	Savat.Reset();
	Xaridlar.Reset();
	TaroziRejasi = D.Tarozi;
	XatoQaytimSezildi = XatoQaytimOtkazildi = Suhbatlar = Salomsiz = Qopol = Rahmatlar = Tejaldi = 0;
	TaroziSezildi = TaroziOtkazildi = Tortishlar = 0;
}

bool UBozorHamyon::RoyxatTugadimi() const
{
	for (const FBozorRoyxatQator& R : Royxat)
		if (!R.bOlindi) return false;
	return Royxat.Num() > 0;
}

int32 UBozorHamyon::Yulduzlar() const
{
	if (!RoyxatTugadimi()) return 0;
	const bool bOdob = Salomsiz == 0 && Qopol == 0;
	const bool bHisob = XatoQaytimOtkazildi == 0 && TaroziOtkazildi == 0 && Tejaldi > 0;
	return 1 + (bOdob ? 1 : 0) + (bHisob ? 1 : 0);
}

TArray<FBozorNatijaQator> UBozorHamyon::NatijaQatorlari() const
{
	TArray<FBozorNatijaQator> Out;
	for (const FBozorRoyxatQator& R : Royxat)
	{
		FBozorNatijaQator& Q = Out.AddDefaulted_GetRef();
		Q.Nomi = R.Nomi;
		Q.Birlik = R.Birlik;
		Q.Kerak = R.Miqdor;
	}
	for (const FBozorXarid& X : Xaridlar)
	{
		FBozorNatijaQator* Q = Out.FindByPredicate([&X](const FBozorNatijaQator& Q) { return Q.Nomi.Equals(X.Nomi, ESearchCase::IgnoreCase); });
		if (!Q)
		{
			Q = &Out.AddDefaulted_GetRef();
			Q->Nomi = X.Nomi;
			if (Q->Nomi.Len()) Q->Nomi[0] = FChar::ToUpper(Q->Nomi[0]);
		}
		if (Q->Olindi <= 0.f) { Q->Narx = X.Narx; Q->Kelishildi = X.Kelishildi; }
		Q->Birlik = X.Birlik;
		Q->Olindi += X.Olindi;
		Q->Tolandi += X.Tolandi;
	}
	return Out;
}

FString FBozorNatijaQator::Baho() const
{
	if (Kerak < 0.f) return TEXT("ro'yxatda yo'q");
	if (Olindi <= 0.f) return TEXT("olinmadi");
	if (FMath::IsNearlyEqual(Olindi, Kerak, 0.05f)) return TEXT("to'g'ri");
	return Olindi > Kerak ? TEXT("ko'p") : TEXT("kam");
}

// ================= narx taxtachasi

ABozorNarxTaxta::ABozorNarxTaxta()
{
	PrimaryActorTick.bCanEverTick = false;
	USceneComponent* Ildiz = CreateDefaultSubobject<USceneComponent>(TEXT("Ildiz"));
	SetRootComponent(Ildiz);
	Taxta = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Taxta"));
	Taxta->SetupAttachment(Ildiz);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Kub(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Kub.Succeeded()) Taxta->SetStaticMesh(Kub.Object);
	Taxta->SetRelativeScale3D(FVector(0.015f, 0.30f, 0.17f));   // 1.5 x 30 x 17 sm, X - xaridor tomonga
	Taxta->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Taxta->SetCastShadow(false);
	auto Matn = [this, Ildiz](const TCHAR* Nom, float Z, float Kattalik)
	{
		UTextRenderComponent* T = CreateDefaultSubobject<UTextRenderComponent>(Nom);
		T->SetupAttachment(Ildiz);
		T->SetRelativeLocation(FVector(1.2f, 0.f, Z));
		T->SetHorizontalAlignment(EHTA_Center);
		T->SetVerticalAlignment(EVRTA_TextCenter);
		T->SetWorldSize(Kattalik);
		T->SetTextRenderColor(FColor(240, 240, 225));
		T->SetCastShadow(false);
		return T;
	};
	Nomi = Matn(TEXT("Nomi"), 3.8f, 4.2f);
	Narxi = Matn(TEXT("Narxi"), -3.2f, 5.6f);
}

void ABozorNarxTaxta::Yoz(const FBozorMahsulot& M)
{
	if (UMaterialInstanceDynamic* D = Taxta->CreateDynamicMaterialInstance(0))
		D->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.02f, 0.07f, 0.04f));   // bo'r taxta
	Nomi->SetText(FText::FromString(FString::Printf(TEXT("%s, 1 %s"), *M.Nomi, *M.Birlik)));
	Narxi->SetText(FText::FromString(FString::Printf(TEXT("%s so'm"), *BozorSotuv::Som(M.Narx))));
}

const TArray<int32>& UBozorHamyon::Turlari()
{
	static const TArray<int32> T = {200000, 100000, 50000, 20000, 10000, 5000, 2000, 1000, 500};
	return T;
}

TArray<int32> UBozorHamyon::Ajrat(int32 Summa)
{
	TArray<int32> Out;
	for (int32 K : Turlari())
		while (Summa >= K) { Out.Add(K); Summa -= K; }
	return Out;
}

void UBozorHamyon::Qosh(int32 Qiymat, int32 N)
{
	Kupyura.FindOrAdd(Qiymat) += N;
	Pul += Qiymat * N;
}

bool UBozorHamyon::Ol(int32 Qiymat)
{
	int32* N = Kupyura.Find(Qiymat);
	if (!N || *N <= 0) return false;
	if (--*N == 0) Kupyura.Remove(Qiymat);
	Pul -= Qiymat;
	return true;
}

bool UBozorHamyon::AniqYigi(int32 Summa, TArray<int32>& Out) const
{
	Out.Reset();
	for (int32 K : Turlari())
		for (int32 N = Soni(K); N > 0 && Summa >= K; --N) { Out.Add(K); Summa -= K; }
	return Summa == 0;
}

FString UBozorHamyon::Tarkibi() const
{
	TArray<FString> Q;
	for (int32 K : Turlari())
		if (const int32 N = Soni(K)) Q.Add(FString::Printf(TEXT("%d × %s"), N, *BozorSotuv::Som(K)));
	return FString::Join(Q, TEXT(", "));
}

void BozorSotuv::XulqniOqi(const FJsonObject& O, FBozorXulq& X)
{
	O.TryGetStringField(TEXT("ism"), X.Ism);
	O.TryGetNumberField(TEXT("yosh"), X.Yosh);
	O.TryGetStringField(TEXT("tavsif"), X.Tavsif);
	O.TryGetStringField(TEXT("odatlar"), X.Odatlar);
	O.TryGetStringField(TEXT("yoqtiradi"), X.Yoqtiradi);
	O.TryGetStringField(TEXT("yoqtirmaydi"), X.Yoqtirmaydi);
	O.TryGetStringField(TEXT("savdoUslubi"), X.SavdoUslubi);
	O.TryGetStringArrayField(TEXT("hayot"), X.Hayot);
	// bugungi kayfiyat: har o'yinda boshqacha - sotuvchi har kuni bir xil bo'lmasin
	TArray<FString> Kayfiyatlar;
	if (O.TryGetStringArrayField(TEXT("kayfiyatlar"), Kayfiyatlar) && Kayfiyatlar.Num())
		X.Kayfiyat = Kayfiyatlar[FMath::RandRange(0, Kayfiyatlar.Num() - 1)];
	O.TryGetNumberField(TEXT("boshMunosabat"), X.BoshMunosabat);
	O.TryGetNumberField(TEXT("salomTalab"), X.SalomTalab);
	O.TryGetNumberField(TEXT("tanbeh"), X.Tanbeh);
	O.TryGetStringField(TEXT("qopolgaMunosabat"), X.QopolgaMunosabat);
	X.SalomTalab = FMath::Clamp(X.SalomTalab, 0.f, 1.f);
	X.Tanbeh = FMath::Clamp(X.Tanbeh, 0.f, 1.f);
	const TSharedPtr<FJsonObject>* Ovoz = nullptr;
	if (O.TryGetObjectField(TEXT("ovoz"), Ovoz))
	{
		(*Ovoz)->TryGetNumberField(TEXT("ohang"), X.Ohang);
		(*Ovoz)->TryGetNumberField(TEXT("tezlik"), X.OvozTezlik);
		(*Ovoz)->TryGetStringField(TEXT("namuna"), X.OvozNamuna);
		(*Ovoz)->TryGetStringField(TEXT("server"), X.ServerOvoz);
		(*Ovoz)->TryGetNumberField(TEXT("temp"), X.Temp);
		X.Ohang = FMath::Clamp(X.Ohang, -6.f, 6.f);
		X.OvozTezlik = FMath::Clamp(X.OvozTezlik, 0.8f, 1.2f);
	}
	const TArray<TSharedPtr<FJsonValue>>* Nam = nullptr;
	if (O.TryGetArrayField(TEXT("namunalar"), Nam))
		for (const TSharedPtr<FJsonValue>& V : *Nam)
		{
			const TArray<TSharedPtr<FJsonValue>>& A = V->AsArray();
			if (A.Num() >= 2) X.Namunalar.Emplace(A[0]->AsString(), A[1]->AsString());
		}
}

const TCHAR* BozorSotuv::FelNomi(EBozorFel F)
{
	switch (F)
	{
	case EBozorFel::Mehribon: return TEXT("mehribon");
	case EBozorFel::Qattiq: return TEXT("qattiq");
	case EBozorFel::Shoshqaloq: return TEXT("shoshqaloq");
	case EBozorFel::Ayyor: return TEXT("ayyor");
	}
	return TEXT("?");
}

// ================= tayyor javoblar

FBozorTushuncha UBozorTayyorJavob::Tushun(const FString& Gap, const FBozorSuhbatHolati& Holat)
{
	FBozorTushuncha T;
	const FString S = TEXT(" ") + Normal(Gap) + TEXT(" ");
	TArray<FString> Soz;
	S.ParseIntoArrayWS(Soz);
	const TArray<FSon> Son = Sonlar(Soz);

	// kalit so'z bir harf xatosi bilan ham (mikrofon: "qanch", "raxmat", "arzan")
	auto Yaqin = [&Soz](std::initializer_list<const TCHAR*> Sozlar)
	{
		for (const TCHAR* K : Sozlar)
			for (const FString& G : Soz)
				if (TaxminanBor(K, G)) return true;
		return false;
	};
	T.bSalomBor = Bor(S, {TEXT("salom"), TEXT("assalom"), TEXT("salam"), TEXT("alaykum"), TEXT("aleykum")}) || Yaqin({TEXT("assalomu"), TEXT("alaykum")});
	// Odob: kattaga murojaat, iltimos, uzr. Qo'pollik odobdan ustun ("aka, ahmoqmisiz" - qo'pol):
	//  - haqorat/so'kish o'zaklari (qo'shimchasi bilan ham: "ahmoqmisan", "tentaksiz");
	//  - kattaga "sen" deb murojaat va yalang buyruq ("non ber", "tort", "tez qil") - o'zbekchada kattaga qo'pol;
	//  - "hoy", "yo'qol", "jim bo'l", "aldayapsiz" kabilar
	static const TCHAR* const QopolOzak[] = {
		TEXT("ahmoq"), TEXT("axmoq"), TEXT("tentak"), TEXT("jinni"), TEXT("eshak"), TEXT("hayvon"), TEXT("la'nat"), TEXT("lanat"),
		TEXT("yaramas"), TEXT("aldoqchi"), TEXT("firibgar"), TEXT("o'g'ri"), TEXT("ogri"), TEXT("xasis"), TEXT("ochko'z"), TEXT("befarosat"),
		TEXT("nodon"), TEXT("dovdir"), TEXT("galvars"), TEXT("tupur"), TEXT("yo'qol"), TEXT("yoqol"), TEXT("itvachcha"), TEXT("harom"),
		TEXT("cho'chqa"), TEXT("chochqa"), TEXT("qanjiq"), TEXT("padar"), TEXT("onangni"), TEXT("dalbayob"), TEXT("jalab"), TEXT("sharmanda")};
	static const TCHAR* const QopolSoz[] = {   // butun so'z
		TEXT("hoy"), TEXT("it"), TEXT("sen"), TEXT("senga"), TEXT("seni"), TEXT("sening"), TEXT("sendan"), TEXT("senmi"), TEXT("bersang"),
		TEXT("ber"), TEXT("bergin"), TEXT("tort"), TEXT("ayt"), TEXT("qil"), TEXT("bo'l"), TEXT("ket"), TEXT("kel"), TEXT("ol"), TEXT("aldama"),
		TEXT("aldading"), TEXT("aldayapsan"), TEXT("aldayapsiz"), TEXT("bilasanmi"), TEXT("kar"), TEXT("ko'r"), TEXT("bunaqa")};
	bool bQopol = Bor(S, {TEXT("tez bo'l"), TEXT("jim bo'l"), TEXT("ovozingni"), TEXT("bor-e"), TEXT("kallang"), TEXT("miyang")});
	for (const FString& W0 : Soz)
	{
		FString W = W0;
		while (W.Len() > 1 && (W.EndsWith(TEXT(".")) || W.EndsWith(TEXT(",")) || W.EndsWith(TEXT("!")) || W.EndsWith(TEXT("?")))) W.LeftChopInline(1);
		for (const TCHAR* Q : QopolOzak) bQopol |= W.StartsWith(Q);
		for (const TCHAR* Q : QopolSoz) bQopol |= W == Q;
	}
	if (bQopol) T.Odob = -1;
	else if (Bor(S, {TEXT("iltimos"), TEXT("marhamat"), TEXT("amaki"), TEXT(" opa"), TEXT("xola"), TEXT(" aka"), TEXT("ota "), TEXT("kechirasiz"), TEXT(" uzr"),
		TEXT("hurmatli"), TEXT("rahmat"), TEXT("raxmat"), TEXT("qulluq")})) T.Odob = 1;

	// qaysi mol haqida (eng uzun mos so'z - "qizil qalampir" "qalampir"dan ustun)
	int32 EngUzun = 0;
	for (int32 i = 0; i < Holat.Mollar.Num(); ++i)
	{
		TArray<FString> Sozlar = Holat.Mollar[i].Sozlar;
		if (Sozlar.IsEmpty()) Sozlar.Add(Holat.Mollar[i].Nomi.ToLower());
		for (const FString& W : Sozlar)
			if (W.Len() > EngUzun && SozBor(S, W)) { T.Mol = i; EngUzun = W.Len(); }
	}
	// aniq topilmasa - bir harf xatosi bilan ("pamidor": mikrofon yoki yozuv xatosi)
	for (int32 i = 0; T.Mol == INDEX_NONE && i < Holat.Mollar.Num(); ++i)
		for (const FString& W : Holat.Mollar[i].Sozlar)
			for (const FString& G : Soz)
				if (T.Mol == INDEX_NONE && TaxminanBor(W, G)) T.Mol = i;

	// miqdor: "kilo" so'zi (yoki "ikkita") va kichik son, pul: katta son
	float Kilo = -1.f, Pul = -1.f;
	bool bKilo = Bor(S, {TEXT("kilo"), TEXT(" kg"), TEXT("dona"), TEXT("bog'"), TEXT(" ta "), TEXT("qadoq")});
	for (const FSon& N : Son) bKilo |= N.bTa;
	for (const FSon& N : Son)
	{
		if (N.Qiymat >= 500.f) Pul = FMath::Max(Pul, N.Qiymat);
		else if (bKilo && Kilo < 0.f) Kilo = N.Qiymat;
	}

	const bool bSavdo = Bor(S, {TEXT("arzon"), TEXT("tushir"), TEXT("kamaytir"), TEXT("qimmat"), TEXT("chegirma"), TEXT("kamroq"), TEXT("kelish"), TEXT("insof")}) || Yaqin({TEXT("arzon"), TEXT("tushiring"), TEXT("qimmat")});
	const bool bNarx = Bor(S, {TEXT("necha"), TEXT("qancha"), TEXT("narx")}) || Yaqin({TEXT("qancha"), TEXT("necha"), TEXT("narxi")});
	const bool bRad = Bor(S, {TEXT("kerak emas"), TEXT("olmayman"), TEXT("kerakmas")});
	const bool bXayr = Bor(S, {TEXT("rahmat"), TEXT("raxmat"), TEXT("xayr"), TEXT("sog' bo'ling")}) || Yaqin({TEXT("rahmat")});
	const bool bRozi = Bor(S, {TEXT("olaman"), TEXT("mayli"), TEXT("xo'p"), TEXT(" xop "), TEXT("bering"), TEXT("tortib"), TEXT("torting"), TEXT("olay"), TEXT("kerak")});

	if (Holat.Bosqich == EBozorBosqich::TaroziTekshir && Bor(S, {TEXT(" ko'p"), TEXT(" kop "), TEXT(" kam"), TEXT("so'ragan"), TEXT("noto'g'ri"), TEXT("xato")})) T.Harakat = EBozorHarakat::TaroziAyt;
	else if (Holat.Bosqich == EBozorBosqich::TaroziTekshir && (bXayr || bRozi)) T.Harakat = EBozorHarakat::TaroziRozi;
	else if (Holat.Bosqich == EBozorBosqich::QaytimTekshir && Bor(S, {TEXT(" kam"), TEXT("yetmaydi"), TEXT("xato")})) T.Harakat = EBozorHarakat::QaytimKam;
	else if (Holat.Bosqich == EBozorBosqich::QaytimTekshir && (bXayr || bRozi)) T.Harakat = EBozorHarakat::QaytimRozi;
	else if (Holat.Bosqich == EBozorBosqich::PulKutilmoqda && Pul > 0.f) { T.Harakat = EBozorHarakat::PulQabul; T.Qiymat = Pul; }
	else if (Kilo > 0.f) { T.Harakat = EBozorHarakat::Tortish; T.Qiymat = Kilo; }
	// pul kutilmayotganda aytilgan katta son - narx taklifi ("o'n mingdan bering", "o'n ming qilib bering")
	else if (Pul > 0.f && Holat.Bosqich != EBozorBosqich::PulKutilmoqda) { T.Harakat = EBozorHarakat::Savdolashish; T.Qiymat = Pul; }
	else if (bSavdo) T.Harakat = EBozorHarakat::Savdolashish;
	else if (bNarx) T.Harakat = EBozorHarakat::NarxAytish;
	else if (bRad || (bXayr && Holat.Bosqich != EBozorBosqich::PulKutilmoqda)) T.Harakat = EBozorHarakat::Xayr;
	else if (bRozi) T.Harakat = (Holat.JoriyNarx > 0 || T.Mol != INDEX_NONE) ? EBozorHarakat::Tortish : EBozorHarakat::NarxAytish;
	else if (T.Mol != INDEX_NONE) T.Harakat = EBozorHarakat::MolTanlash;
	else if (T.bSalomBor) T.Harakat = EBozorHarakat::Salom;
	return T;
}

FString UBozorTayyorJavob::Izoh(const FString& Kalit)
{
	for (const TMap<FString, TArray<FString>>* M : {static_cast<const TMap<FString, TArray<FString>>*>(&FelGaplari), Asosiy})
		if (const TArray<FString>* V = M ? M->Find(Kalit) : nullptr; V && V->Num()) return Tanla(*V);
	return FString();
}

FString UBozorTayyorJavob::Yoz(EBozorJavob Javob, const FBozorSuhbatHolati& H, int32 Qaytim)
{
	const bool bTanlangan = H.Mollar.IsValidIndex(H.Tanlangan);
	const FString Kalit = StaticEnum<EBozorJavob>()->GetNameStringByValue(static_cast<int64>(Javob));
	const FString KayfiyatKalit = H.Munosabat >= 70 ? Kalit + TEXT("_iliq") : H.Munosabat < 30 ? Kalit + TEXT("_sovuq") : FString();
	// variantlar: fe'l kayfiyati, umumiy kayfiyat, fe'l, umumiy - birinchi bo'sh bo'lmagani
	auto Top = [this](const FString& K) -> const TArray<FString>*
	{
		for (const TMap<FString, TArray<FString>>* M : {static_cast<const TMap<FString, TArray<FString>>*>(&FelGaplari), Asosiy})
			if (const TArray<FString>* V = M ? M->Find(K) : nullptr; V && V->Num()) return V;
		return nullptr;
	};
	const TArray<FString>* Variant = nullptr;
	if (Javob == EBozorJavob::PulOldim && Qaytim == 0) Variant = Top(TEXT("PulOldimAniq"));
	if (!Variant && !KayfiyatKalit.IsEmpty())
		for (const TMap<FString, TArray<FString>>* M : {static_cast<const TMap<FString, TArray<FString>>*>(&FelGaplari), Asosiy})
			if (const TArray<FString>* V = M ? M->Find(KayfiyatKalit) : nullptr; !Variant && V && V->Num()) Variant = V;
	if (!Variant) Variant = Top(Kalit);
	FString Q = Variant ? Tanla(*Variant) : FString(TEXT("..."));
	// mol hali tanlanmagan bo'lsa, birlik va narxni so'raydigan gaplar o'rniga molni so'raydi
	if (!bTanlangan && (Q.Contains(TEXT("{birlik}")) || Q.Contains(TEXT("{narx}"))) && Javob != EBozorJavob::MolSora)
		if (const TArray<FString>* V = Top(TEXT("MolSora"))) Q = Tanla(*V);

	// ovoz bo'laklari: qolip o'rinlar bo'yicha bo'linadi (gaplar_yoz.py bilan bir xil qoida)
	OxirgiBolaklar.Reset();
	{
		auto Son = [this](int64 N)
		{
			if (N >= 1000) OxirgiBolaklar.Add(FString::Printf(TEXT("S_%lldm"), N / 1000));
			if (N % 1000 || N == 0) OxirgiBolaklar.Add(FString::Printf(TEXT("S_%lld"), N % 1000));
		};
		auto Mollar = [this](const TArray<FBozorMahsulot>& M)
		{
			for (int32 i = 0; i < M.Num(); ++i)
			{
				if (i > 0 && i == M.Num() - 1) OxirgiBolaklar.Add(MatnKaliti(TEXT("va")));
				OxirgiBolaklar.Add(TEXT("P_") + M[i].Id);
			}
		};
		int32 Joy = 0;
		while (Joy <= Q.Len())
		{
			const int32 Och = Q.Find(TEXT("{"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Joy);
			const int32 Yop = Och == INDEX_NONE ? INDEX_NONE : Q.Find(TEXT("}"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Och);
			const FString Matn = Q.Mid(Joy, (Yop == INDEX_NONE ? Q.Len() : Och) - Joy).TrimStartAndEnd();
			bool bHarf = false;
			for (TCHAR C : Matn) bHarf |= FChar::IsAlpha(C);
			if (bHarf) OxirgiBolaklar.Add(MatnKaliti(Matn));
			if (Yop == INDEX_NONE) break;
			const FString Orin = Q.Mid(Och + 1, Yop - Och - 1);
			if (Orin == TEXT("mahsulot") && bTanlangan) OxirgiBolaklar.Add(TEXT("P_") + H.Mahsulot.Id);
			else if (Orin == TEXT("mahsulot") || Orin == TEXT("mollar")) Mollar(H.Mollar);
			else if (Orin == TEXT("birlik")) OxirgiBolaklar.Add(TEXT("B_") + H.Mahsulot.Birlik.Replace(TEXT("'"), TEXT("")));
			else if (Orin == TEXT("narx")) Son(H.JoriyNarx > 0 ? H.JoriyNarx : H.Mahsulot.Narx);
			else if (Orin == TEXT("summa")) Son(H.Summa);
			else if (Orin == TEXT("son")) Son(Qaytim);
			else if (Orin == TEXT("miqdor")) OxirgiBolaklar.Add(FString::Printf(TEXT("M_%d"), FMath::RoundToInt((H.HisobMiqdor > 0.f ? H.HisobMiqdor : H.Miqdor) * 10.f)));
			else if (Orin == TEXT("qaytim"))
			{
				for (int32 i = 0; i < H.QaytimKupyuralar.Num(); ++i)
				{
					if (i > 0 && i == H.QaytimKupyuralar.Num() - 1) OxirgiBolaklar.Add(MatnKaliti(TEXT("va")));
					Son(H.QaytimKupyuralar[i]);
				}
				OxirgiBolaklar.Add(MatnKaliti(TEXT("so'm")));
			}
			Joy = Yop + 1;
		}
	}

	FStringFormatNamedArguments A;
	A.Add(TEXT("mahsulot"), bTanlangan ? H.Mahsulot.Nomi : MollarRoyxati(H.Mollar));
	A.Add(TEXT("mollar"), MollarRoyxati(H.Mollar));
	A.Add(TEXT("birlik"), H.Mahsulot.Birlik);
	A.Add(TEXT("narx"), BozorSotuv::Som(H.JoriyNarx > 0 ? H.JoriyNarx : H.Mahsulot.Narx));
	A.Add(TEXT("miqdor"), BozorSotuv::Miqdor(H.HisobMiqdor > 0.f ? H.HisobMiqdor : H.Miqdor));   // sotuvchi aytgani (tarozidagi emas)
	A.Add(TEXT("summa"), BozorSotuv::Som(H.Summa));
	A.Add(TEXT("son"), BozorSotuv::Som(Qaytim));
	{
		// qaytim kupyuralari: "5 000 va 2 000 so'm" (jami aytilmaydi - bola o'zi sanaydi)
		TArray<FString> K;
		for (int32 X : H.QaytimKupyuralar) K.Add(BozorSotuv::Som(X));
		FString S = K.Num() > 1 ? FString::Join(TArrayView<const FString>(K).LeftChop(1), TEXT(", ")) + TEXT(" va ") + K.Last() : (K.Num() ? K[0] : FString(TEXT("0")));
		A.Add(TEXT("qaytim"), S + TEXT(" so'm"));
	}
	Q = FString::Format(*Q, A);
	if (Q.Len()) Q[0] = FChar::ToUpper(Q[0]);
	return Q;
}

FString UBozorTayyorJavob::MatnKaliti(const FString& Bolak)
{
	FTCHARToUTF8 U(*Bolak);
	FMD5 Md5;
	Md5.Update(reinterpret_cast<const uint8*>(U.Get()), U.Length());
	uint8 D[16];
	Md5.Final(D);
	return TEXT("G_") + BytesToHex(D, 4).ToLower();
}

// ================= suhbat: hisob-kitobni shu yerda kod qiladi

void UBozorSuhbat::Boshla(const FBozorRasta& Rasta, UBozorHamyon* InHamyon, UBozorSotuvchiMiya* InMiya, int32 BoshMunosabat)
{
	Holat = FBozorSuhbatHolati();
	OxirgiOdob = 0;
	Holat.Munosabat = FMath::Clamp(BoshMunosabat >= 0 ? BoshMunosabat : Rasta.Xulq.BoshMunosabat, 0, 100);
	Holat.Mollar = Rasta.Mollar;
	Holat.Fel = Rasta.Fel;
	Holat.RadEhtimoli = Rasta.RadEhtimoli;
	Holat.OdobBall = Rasta.OdobBall;
	Holat.QopolBall = Rasta.QopolBall;
	Holat.XatoQaytim = Rasta.XatoQaytim;
	bTolov = false;
	Peshtaxta.Reset();
	Sotuvchi = Rasta.Sotuvchi;
	Hamyon = InHamyon;
	Miya = InMiya;
	bSotibOldi = false;
	bMiqdorTanlash = bMolRoyxati = false;
	if (UBozorTayyorJavob* Tayyor = Cast<UBozorTayyorJavob>(Miya))
	{
		Tayyor->FelGaplari = Rasta.Gaplar;
		Tayyor->Asosiy = Rasta.Asosiy;
	}
	Uslub = Rasta.Uslub;
	SalomTalab = Rasta.Xulq.SalomTalab;
	Tanbeh = Rasta.Xulq.Tanbeh;
	Bolaklar.Reset();
	if (Holat.Mollar.Num() == 1) MolniTanla(0);
}

void UBozorSuhbat::MolniTanla(int32 I)
{
	Holat.Tanlangan = I;
	Holat.Mahsulot = Holat.Mollar[I];
	Holat.Bosqich = EBozorBosqich::Boshlanish;
	Holat.JoriyNarx = 0;
	Holat.Miqdor = Holat.SoralganMiqdor = Holat.HisobMiqdor = 0.f;
	Holat.Tarozi = EBozorTarozi::Togri;
	Holat.Summa = 0;
	Holat.Savdolashuv = 0;
}

FString UBozorSuhbat::Javob(EBozorJavob J, int32 Son)
{
	OxirgiJavob = J;
	if (!Miya) return FString();
	const FString Q = Miya->Yoz(J, Holat, Son);
	Bolaklar.Append(Miya->Bolaklar());
	return Q;
}

FString UBozorSuhbat::KutibOl()
{
	Bolaklar.Reset();
	return Javob(EBozorJavob::KutibOlish);
}

FString UBozorSuhbat::Yoz(const FString& Gap)
{
	if (!Miya) return FString();
	return Bajar(Miya->Tushun(Gap, Holat));
}

FString UBozorSuhbat::Bajar(const FBozorTushuncha& T)
{
	if (!Miya || !Hamyon) return FString();
	Bolaklar.Reset();
	OxirgiOdob = T.Odob;
	OxirgiIzoh.Reset();

	// boshqa molga o'tish (pul kutilayotganda emas) yoki xariddan keyin yangi xarid
	if (Holat.Mollar.IsValidIndex(T.Mol) && Holat.Bosqich != EBozorBosqich::PulKutilmoqda && Holat.Bosqich != EBozorBosqich::QaytimTekshir && Holat.Bosqich != EBozorBosqich::TaroziTekshir
		&& (T.Mol != Holat.Tanlangan || Holat.Bosqich == EBozorBosqich::Tugadi))
		MolniTanla(T.Mol);

	// munosabat: odobli gap oshiradi, qo'pol gap tushiradi (qancha - fe'lga bog'liq)
	auto Munosabat = [this](int32 D) { Holat.Munosabat = FMath::Clamp(Holat.Munosabat + D, 0, 100); };
	if (T.Odob > 0) Munosabat(Holat.OdobBall);
	else if (T.Odob < 0) { Munosabat(Holat.QopolBall); ++Hamyon->Qopol; }
	if (T.Odob > 0 && (T.Harakat == EBozorHarakat::Xayr || T.Harakat == EBozorHarakat::QaytimRozi)) ++Hamyon->Rahmatlar;

	if (Holat.Bosqich == EBozorBosqich::Tugadi)
		return Javob(T.Harakat == EBozorHarakat::Xayr ? EBozorJavob::Xayr : EBozorJavob::Tugagan);
	FString Oldi;
	if (!Holat.bSalomlashdi)
	{
		Holat.bSalomlashdi = true;
		// odob: salomsiz gap boshlansa, faqat salomni talab qiladigan sotuvchi eslatadi (va biroz xafa bo'ladi) -
		// qolganlari e'tibor bermay savdoga o'tadi (xulq.json "salomTalab")
		const bool bEslatadi = !T.bSalomBor && FMath::FRand() < SalomTalab;
		Oldi = T.bSalomBor ? (T.Harakat == EBozorHarakat::Salom ? FString() : Javob(EBozorJavob::Salom))
			: bEslatadi ? Javob(EBozorJavob::SalomBering) : FString();
		Munosabat(T.bSalomBor ? Holat.OdobBall / 2 : bEslatadi ? -10 : 0);
		++Hamyon->Suhbatlar;
		if (!T.bSalomBor) ++Hamyon->Salomsiz;
	}
	const bool bMolKerak = T.Harakat == EBozorHarakat::NarxAytish || T.Harakat == EBozorHarakat::Savdolashish || T.Harakat == EBozorHarakat::Tortish;
	const bool bTanlangan = Holat.Mollar.IsValidIndex(Holat.Tanlangan);
	if (bTanlangan && Holat.JoriyNarx == 0 && T.Harakat != EBozorHarakat::Salom && T.Harakat != EBozorHarakat::Tushunmadi) Holat.JoriyNarx = Holat.Mahsulot.Narx;

	FString Asosiy;
	if (bMolKerak && !bTanlangan)
	{
		Asosiy = Javob(EBozorJavob::MolSora);
	}
	else switch (T.Harakat)
	{
	case EBozorHarakat::Salom:
		Asosiy = Javob(EBozorJavob::Salom);
		break;

	case EBozorHarakat::MolTanlash:
		if (Holat.Bosqich == EBozorBosqich::Boshlanish) Holat.Bosqich = EBozorBosqich::NarxAytildi;
		Asosiy = Javob(EBozorJavob::MolTanlandi);
		break;

	case EBozorHarakat::NarxAytish:
		if (Holat.Bosqich == EBozorBosqich::Boshlanish) Holat.Bosqich = EBozorBosqich::NarxAytildi;
		Asosiy = Javob(EBozorJavob::Narx);
		break;

	case EBozorHarakat::Savdolashish:
	{
		++Holat.Savdolashuv;
		const FBozorMahsulot& M = Holat.Mahsulot;
		// juda yaxshi munosabatda eng past narxdan ham bir qadam pastga tushadi va bir tushirish kattaroq
		const int32 EngPast = FMath::Max(500, M.EngPastNarx - (Holat.Munosabat >= 80 ? M.Qadam : 0));
		const int32 Qadam = Holat.Munosabat >= 70 ? FMath::RoundToInt(M.Qadam * 1.5f / 500.f) * 500 : M.Qadam;
		if (Holat.Munosabat < 10)
		{
			Holat.Bosqich = EBozorBosqich::Tugadi;
			Asosiy = Javob(EBozorJavob::Haydadi);
			break;
		}
		if (Holat.Munosabat < 30)
		{
			// qattiq sotuvchi qo'pollikka narxni ko'taradi (so'ralganidan 2 qadamdan oshmaydi)
			const int32 Yuqori = M.Narx + 2 * M.Qadam;
			if (T.Odob < 0 && Holat.Fel == EBozorFel::Qattiq && Holat.JoriyNarx < Yuqori)
			{
				Holat.JoriyNarx = FMath::Min(Yuqori, Holat.JoriyNarx + M.Qadam);
				Asosiy = Javob(EBozorJavob::NarxniKotardi);
			}
			else Asosiy = Javob(EBozorJavob::QopolgaRad);
		}
		else if (Holat.JoriyNarx <= EngPast)
		{
			Holat.JoriyNarx = EngPast;
			Asosiy = Javob(EBozorJavob::TushirmaymanEngPast);
		}
		else if (Holat.Savdolashuv == 1 && Holat.Munosabat < 70 && FMath::FRand() < Holat.RadEhtimoli)
		{
			Asosiy = Javob(EBozorJavob::TushirmaymanHozircha);
		}
		else
		{
			Holat.JoriyNarx = FMath::Max(EngPast, Holat.JoriyNarx - FMath::Max(500, Qadam));
			Asosiy = Javob(EBozorJavob::TushirdiMan);
		}
		if (Holat.Bosqich == EBozorBosqich::Boshlanish) Holat.Bosqich = EBozorBosqich::NarxAytildi;
		// tortilgandan keyin tushirsa ham summa yangi narxdan
		if (Holat.HisobMiqdor > 0.f) Holat.Summa = FMath::RoundToInt(Holat.JoriyNarx * Holat.HisobMiqdor / 500.f) * 500;
		break;
	}

	case EBozorHarakat::Tortish:
		if (T.Qiymat <= 0.f)
		{
			if (Holat.Bosqich == EBozorBosqich::Boshlanish) Holat.Bosqich = EBozorBosqich::NarxAytildi;
			Asosiy = Javob(EBozorJavob::MiqdorSora);
			break;
		}
	{
		Holat.SoralganMiqdor = FMath::Clamp(T.Qiymat, 0.5f, 20.f);
		Holat.Miqdor = Holat.HisobMiqdor = Holat.SoralganMiqdor;
		// tarozi: daraja rejasidagi navbatdagi xato. "ortiq" - ko'p tortadi (ko'piga pul so'raydi),
		// "kam" - kam tortadi, lekin so'ralganicha deb to'liq pul so'raydi
		const FString Reja = Hamyon->TaroziRejasi.IsValidIndex(Hamyon->Tortishlar) ? Hamyon->TaroziRejasi[Hamyon->Tortishlar] : FString();
		++Hamyon->Tortishlar;
		const bool bKilo = Holat.Mahsulot.Birlik == TEXT("kilo");
		if (Reja == TEXT("ortiq"))
			Holat.Miqdor = Holat.HisobMiqdor = Holat.SoralganMiqdor + MiqdorQadami();
		else if (Reja == TEXT("kam"))
		{
			const float Kam = bKilo ? FMath::RoundToFloat(Holat.SoralganMiqdor * 8.f) / 10.f : Holat.SoralganMiqdor - 1.f;
			if (Kam > 0.f) Holat.Miqdor = Kam;
		}
		Holat.Tarozi = EBozorTarozi::Togri;
		Holat.Summa = FMath::RoundToInt(Holat.JoriyNarx * Holat.HisobMiqdor / 500.f) * 500;
		Holat.Bosqich = EBozorBosqich::TaroziTekshir;
		// bola tarozini (dona bo'lsa - xaltani) o'zi ko'radi
		Asosiy = Javob(EBozorJavob::Tortdim) + FString::Printf(TEXT(" [%s: %s %s]"), bKilo ? TEXT("Tarozida") : TEXT("Xaltada"),
			*BozorSotuv::Miqdor(Holat.Miqdor), *Holat.Mahsulot.Birlik);
		break;
	}

	case EBozorHarakat::TaroziRozi:
		if (Holat.Bosqich != EBozorBosqich::TaroziTekshir)
		{
			Asosiy = Javob(EBozorJavob::Tushunmadim);
			break;
		}
		if (!FMath::IsNearlyEqual(Holat.Miqdor, Holat.SoralganMiqdor))
		{
			Holat.Tarozi = Holat.Miqdor > Holat.SoralganMiqdor ? EBozorTarozi::OrtiqOldi : EBozorTarozi::KamOtkazdi;
			++Hamyon->TaroziOtkazildi;
		}
		Holat.Bosqich = EBozorBosqich::PulKutilmoqda;
		Asosiy = Javob(EBozorJavob::TaroziKelishdi);
		break;

	case EBozorHarakat::TaroziAyt:
		if (Holat.Bosqich != EBozorBosqich::TaroziTekshir)
		{
			Asosiy = Javob(EBozorJavob::Tushunmadim);
			break;
		}
		Holat.Bosqich = EBozorBosqich::PulKutilmoqda;
		if (FMath::IsNearlyEqual(Holat.Miqdor, Holat.SoralganMiqdor))
		{
			Munosabat(-5);   // to'g'ri tortilganga "xato" deyildi
			Asosiy = Javob(EBozorJavob::TaroziTogri);
			break;
		}
		Holat.Tarozi = Holat.Miqdor > Holat.SoralganMiqdor ? EBozorTarozi::OrtiqAytdi : EBozorTarozi::KamAytdi;
		++Hamyon->TaroziSezildi;
		Holat.Miqdor = Holat.HisobMiqdor = Holat.SoralganMiqdor;
		Holat.Summa = FMath::RoundToInt(Holat.JoriyNarx * Holat.HisobMiqdor / 500.f) * 500;
		Asosiy = Javob(EBozorJavob::TaroziUzr);
		break;

	case EBozorHarakat::PulQabul:
	{
		// kupyuralar: peshtaxtaga qo'yilganlari, bo'lmasa (matn orqali) hamyondan aynan shu summa yig'iladi
		const int32 Berildi = FMath::RoundToInt(T.Qiymat);
		TArray<int32> Notalar = Peshtaxta;
		int32 PeshtaxtaJami = 0;
		for (int32 K : Notalar) PeshtaxtaJami += K;
		if (Notalar.IsEmpty() || PeshtaxtaJami != Berildi)
			if (!Hamyon->AniqYigi(Berildi, Notalar)) Notalar.Reset();
		if (Holat.Bosqich != EBozorBosqich::PulKutilmoqda)
		{
			Asosiy = Javob(EBozorJavob::Tushunmadim);
		}
		else if (Notalar.IsEmpty())
		{
			Asosiy = Javob(EBozorJavob::HamyondaYoq);
		}
		else if (Berildi < Holat.Summa)
		{
			Asosiy = Javob(EBozorJavob::PulYetmaydi, Holat.Summa - Berildi);
		}
		else
		{
			for (int32 K : Notalar) Hamyon->Ol(K);
			Peshtaxta.Reset();
			// qaytim kupyuralarda; chalg'iydigan/ayyor sotuvchi ba'zan kamroq beradi (bola sanab sezishi kerak)
			const int32 Qaytim = Berildi - Holat.Summa;
			int32 Kam = 0;
			if (Qaytim >= 2000 && FMath::FRand() < Holat.XatoQaytim) Kam = Qaytim >= 5000 ? 2000 : 1000;
			Holat.QaytimKam = Kam;
			Holat.QaytimKupyuralar = UBozorHamyon::Ajrat(Qaytim - Kam);
			for (int32 K : Holat.QaytimKupyuralar) Hamyon->Qosh(K);
			Hamyon->Savat.Add(FString::Printf(TEXT("%s %s %s"), *BozorSotuv::Miqdor(Holat.Miqdor), *Holat.Mahsulot.Birlik, *Holat.Mahsulot.Nomi));
			Hamyon->Tejaldi += FMath::Max(0, FMath::RoundToInt((Holat.Mahsulot.Narx - Holat.JoriyNarx) * Holat.HisobMiqdor));
			{
				FBozorXarid& X = Hamyon->Xaridlar.AddDefaulted_GetRef();
				X.Nomi = Holat.Mahsulot.Nomi;
				X.Birlik = Holat.Mahsulot.Birlik;
				X.Soralgan = Holat.SoralganMiqdor > 0.f ? Holat.SoralganMiqdor : Holat.Miqdor;
				X.Olindi = Holat.Miqdor;
				X.Narx = Holat.Mahsulot.Narx;
				X.Kelishildi = Holat.JoriyNarx;
				X.Tolandi = Holat.Summa;
				X.Tarozi = Holat.Tarozi;
			}
			// xarid ro'yxatida bo'lsa - jami yetarli miqdorda olinganda belgilanadi
			for (FBozorRoyxatQator& R : Hamyon->Royxat)
				if (R.Nomi.Equals(Holat.Mahsulot.Nomi, ESearchCase::IgnoreCase))
				{
					R.Olindi += Holat.Miqdor;
					if (R.Olindi + KINDA_SMALL_NUMBER >= R.Miqdor) R.bOlindi = true;
				}
			Holat.Bosqich = Qaytim > 0 ? EBozorBosqich::QaytimTekshir : EBozorBosqich::Tugadi;
			bSotibOldi = true;
			Asosiy = Javob(EBozorJavob::PulOldim, Qaytim - Kam);
		}
		break;
	}

	case EBozorHarakat::QaytimKam:
		if (Holat.Bosqich != EBozorBosqich::QaytimTekshir)
		{
			Asosiy = Javob(EBozorJavob::Tushunmadim);
		}
		else if (Holat.QaytimKam > 0)
		{
			for (int32 K : UBozorHamyon::Ajrat(Holat.QaytimKam)) Hamyon->Qosh(K);
			++Hamyon->XatoQaytimSezildi;
			Asosiy = Javob(EBozorJavob::QaytimUzr, Holat.QaytimKam);
			Holat.QaytimKam = 0;
			Holat.Bosqich = EBozorBosqich::Tugadi;
		}
		else
		{
			Munosabat(-5);   // to'g'ri qaytimga "kam" deyildi
			Asosiy = Javob(EBozorJavob::QaytimTogri);
			Holat.Bosqich = EBozorBosqich::Tugadi;
		}
		break;

	case EBozorHarakat::QaytimRozi:
		if (Holat.QaytimKam > 0) ++Hamyon->XatoQaytimOtkazildi;
		Holat.QaytimKam = 0;
		Holat.Bosqich = EBozorBosqich::Tugadi;
		Asosiy = Javob(EBozorJavob::OshBolsin);
		break;

	case EBozorHarakat::Xayr:
		if (Holat.Bosqich == EBozorBosqich::QaytimTekshir && Holat.QaytimKam > 0) ++Hamyon->XatoQaytimOtkazildi;
		Holat.QaytimKam = 0;
		Asosiy = Javob(bSotibOldi ? EBozorJavob::Xayr : EBozorJavob::XayrSotibOlmay);
		Holat.Bosqich = EBozorBosqich::Tugadi;
		break;

	default:
		Asosiy = Javob(EBozorJavob::Tushunmadim);
		break;
	}

	// Bolaning ohangiga munosabat (sotuvchi fe'li/xulqidagi "QopolIzoh"/"OdobIzoh"): javob ma'nosining bir qismi bo'ladi -
	// LLM uni o'z uslubida aytadi (LLM'ga "munosabat bildir" deyilsa 4B model e'tibor bermaydi yoki ko'rsatmani gapga qo'shadi).
	// Qo'pollikka har doim (javob o'zi tanbeh bo'lmasa); odobga ba'zan va ketma-ket emas - har gapda "barakalla" zeriktiradi
	FString Izoh;
	const bool bTanbeh = OxirgiJavob == EBozorJavob::QopolgaRad || OxirgiJavob == EBozorJavob::Haydadi || OxirgiJavob == EBozorJavob::SalomBering;
	// qo'pollikka tanbeh ham har sotuvchida har xil (xulq.json "tanbeh"): ba'zisi e'tibor bermaydi yoki hazilga buradi
	if (T.Odob < 0 && !bTanbeh && FMath::FRand() < Tanbeh) Izoh = Miya->Izoh(TEXT("QopolIzoh"));
	else if (T.Odob > 0 && !bOdobIzohOldin && OxirgiJavob != EBozorJavob::TushirdiMan && OxirgiJavob != EBozorJavob::Salom && FMath::FRand() < 0.4f)
		Izoh = Miya->Izoh(TEXT("OdobIzoh"));
	bOdobIzohOldin = T.Odob > 0 && !Izoh.IsEmpty();
	OxirgiIzoh = Izoh;
	if (!Izoh.IsEmpty()) Oldi = Oldi.IsEmpty() ? Izoh : Izoh + TEXT(" ") + Oldi;

	return Oldi.IsEmpty() ? Asosiy : Oldi + TEXT(" ") + Asosiy;
}

// ================= tanlovlar: bolaning javob variantlari

float UBozorSuhbat::MiqdorQadami() const
{
	return Holat.Mahsulot.Birlik == TEXT("kilo") ? 0.5f : 1.f;
}

TArray<FBozorTanlov> UBozorSuhbat::Tanlovlar() const
{
	TArray<FBozorTanlov> V;
	auto Gap = [&V](const FString& Matn, EBozorHarakat H, int32 Odob = 0, int32 Mol = INDEX_NONE, float Qiymat = 0.f)
	{
		FBozorTanlov& T = V.AddDefaulted_GetRef();
		T.Matn = Matn;
		T.Niyat.Harakat = H;
		T.Niyat.Odob = Odob;
		T.Niyat.Mol = Mol;
		T.Niyat.Qiymat = Qiymat;
		T.Niyat.bSalomBor = H == EBozorHarakat::Salom;
	};
	auto Otish = [&V](const FString& Matn, EBozorTanlovTur Tur)
	{
		FBozorTanlov& T = V.AddDefaulted_GetRef();
		T.Matn = Matn;
		T.Tur = Tur;
	};
	auto Bosh = [](const FString& S) { FString R = S; if (R.Len()) R[0] = FChar::ToUpper(R[0]); return R; };
	const bool bTanlangan = Holat.Mollar.IsValidIndex(Holat.Tanlangan);
	const FBozorMahsulot& M = Holat.Mahsulot;

	// xayrlashib yoki haydalib tugagan suhbat - variant yo'q (oyna o'zi yopiladi)
	if (Holat.Bosqich == EBozorBosqich::Tugadi && !bSotibOldi) return V;
	if (OxirgiJavob == EBozorJavob::Xayr || OxirgiJavob == EBozorJavob::Haydadi) return V;

	if (bMiqdorTanlash)
	{
		const FString Son = FString::Printf(TEXT("%s %s"), *BozorSotuv::Miqdor(TanlovMiqdor), *M.Birlik);
		Gap(FString::Printf(TEXT("Iltimos, %s bering"), *Son), EBozorHarakat::Tortish, 1, INDEX_NONE, TanlovMiqdor);
		Otish(FString::Printf(TEXT("+ %s ko'proq"), *BozorSotuv::Miqdor(MiqdorQadami())), EBozorTanlovTur::MiqdorKop);
		Otish(FString::Printf(TEXT("− %s kamroq"), *BozorSotuv::Miqdor(MiqdorQadami())), EBozorTanlovTur::MiqdorKam);
		Otish(TEXT("Orqaga"), EBozorTanlovTur::Ortga);
		return V;
	}
	if (!Holat.bSalomlashdi)
	{
		Gap(TEXT("Assalomu alaykum!"), EBozorHarakat::Salom, 1);
		const int32 Mol = bTanlangan ? Holat.Tanlangan : 0;
		Gap(FString::Printf(TEXT("%s necha pul?"), *Bosh(Holat.Mollar[Mol].Nomi)), EBozorHarakat::NarxAytish, 0, Mol);
		Gap(TEXT("Hoy, bu nechpul?"), EBozorHarakat::NarxAytish, -1, Mol);
		return V;
	}
	if (Holat.Bosqich == EBozorBosqich::Tugadi && !bMolRoyxati)   // xarid bo'ldi
	{
		Gap(TEXT("Rahmat, xayr!"), EBozorHarakat::Xayr, 1);
		if (Holat.Mollar.Num() > 1) Otish(TEXT("Yana boshqa narsa olaman"), EBozorTanlovTur::MolRoyxati);
		else Gap(FString::Printf(TEXT("Yana %s olaman"), *M.Nomi), EBozorHarakat::MolTanlash, 0, Holat.Tanlangan);
		return V;
	}
	if (!bTanlangan || bMolRoyxati || (Holat.Bosqich == EBozorBosqich::Boshlanish && Holat.JoriyNarx == 0))
	{
		for (int32 i = 0; i < Holat.Mollar.Num() && V.Num() < 4; ++i)
			Gap(FString::Printf(TEXT("%s necha pul?"), *Bosh(Holat.Mollar[i].Nomi)), EBozorHarakat::NarxAytish, 0, i);
		if (bMolRoyxati) Otish(TEXT("Orqaga"), EBozorTanlovTur::Ortga);
		else Gap(TEXT("Hozircha hech narsa kerak emas, rahmat"), EBozorHarakat::Xayr, 1);
		return V;
	}
	if (Holat.Bosqich == EBozorBosqich::QaytimTekshir)
	{
		// bola qaytimni sanaydi: to'g'ri bo'lsa rahmat, kam bo'lsa aytadi
		Gap(TEXT("Rahmat!"), EBozorHarakat::QaytimRozi, 1);
		Gap(TEXT("Kechirasiz, qaytimni kam berdingiz"), EBozorHarakat::QaytimKam);
		return V;
	}
	if (Holat.Bosqich == EBozorBosqich::TaroziTekshir)
	{
		// bola tarozini ko'radi: indamay oladi yoki ko'p/kam tortilganini aytadi (odob bilan yoki qo'pol)
		const FString Soralgan = FString::Printf(TEXT("%s %s"), *BozorSotuv::Miqdor(Holat.SoralganMiqdor), *M.Birlik);
		Gap(TEXT("Mayli, rahmat"), EBozorHarakat::TaroziRozi, 1);
		Gap(FString::Printf(TEXT("Kechirasiz, men %s so'ragandim"), *Soralgan), EBozorHarakat::TaroziAyt, 1);
		Gap(TEXT("Hoy, tarozingiz noto'g'ri-ku!"), EBozorHarakat::TaroziAyt, -1);
		return V;
	}
	if (Holat.Bosqich == EBozorBosqich::PulKutilmoqda && bTolov)
	{
		// kupyuralar (kattasidan): bosilgani peshtaxtaga qo'yiladi; keyin "Mana, X so'm"
		for (int32 K : UBozorHamyon::Turlari())
		{
			const int32 Qoldi = Hamyon->Soni(K) - Peshtaxta.FilterByPredicate([K](int32 X) { return X == K; }).Num();
			if (Qoldi <= 0) continue;
			FBozorTanlov& T = V.AddDefaulted_GetRef();
			T.Matn = BozorSotuv::Som(K);
			T.Tur = EBozorTanlovTur::KupyuraQoy;
			T.Kupyura = K;
			T.Soni = Qoldi;
		}
		int32 Jami = 0;
		for (int32 K : Peshtaxta) Jami += K;
		if (Jami > 0)
		{
			Gap(FString::Printf(TEXT("Mana, %s so'm"), *BozorSotuv::Som(Jami)), EBozorHarakat::PulQabul, 0, INDEX_NONE, static_cast<float>(Jami));
			Otish(TEXT("Pulni qaytarib olish"), EBozorTanlovTur::KupyuraQaytar);
		}
		Otish(TEXT("Orqaga"), EBozorTanlovTur::Ortga);
		return V;
	}
	if (Holat.Bosqich == EBozorBosqich::PulKutilmoqda)
	{
		if (Hamyon->Pul >= Holat.Summa) Otish(TEXT("Pul berish"), EBozorTanlovTur::TolovOch);
		else Otish(TEXT("Pulim yetmaydi, kamroq bering"), EBozorTanlovTur::MiqdorOch);
		Gap(TEXT("Iltimos, ozgina arzonroq qiling"), EBozorHarakat::Savdolashish, 1);
		if (Hamyon->Pul >= Holat.Summa) Otish(TEXT("Miqdorni o'zgartiraman"), EBozorTanlovTur::MiqdorOch);
		Gap(TEXT("Kerak emas, rahmat"), EBozorHarakat::Xayr, 1);
		return V;
	}
	// narx aytildi: savdolashish yoki olish
	Gap(TEXT("Iltimos, ozgina arzonroq qilib bering"), EBozorHarakat::Savdolashish, 1);
	Gap(TEXT("Qimmat-ku! Bunaqa narx bo'lmaydi!"), EBozorHarakat::Savdolashish, -1);
	Otish(FString::Printf(TEXT("Olaman, tortib bering")), EBozorTanlovTur::MiqdorOch);
	if (Holat.Mollar.Num() > 1) Otish(TEXT("Boshqa narsa ko'raman"), EBozorTanlovTur::MolRoyxati);
	else Gap(TEXT("Kerak emas, rahmat"), EBozorHarakat::Xayr, 1);
	return V;
}

bool UBozorSuhbat::Tanla(int32 I, FString& OutMen, FString& OutSotuvchi)
{
	const TArray<FBozorTanlov> V = Tanlovlar();
	if (!V.IsValidIndex(I)) return false;
	const FBozorTanlov& T = V[I];
	switch (T.Tur)
	{
	case EBozorTanlovTur::MiqdorOch:
	{
		bMiqdorTanlash = true;
		// boshlang'ich miqdor: avval tortilgan yoki xarid ro'yxatidagi, bo'lmasa 1
		TanlovMiqdor = Holat.Miqdor > 0.f ? Holat.Miqdor : 1.f;
		if (Holat.Miqdor <= 0.f && Hamyon)
			for (const FBozorRoyxatQator& R : Hamyon->Royxat)
				if (R.Nomi.Equals(Holat.Mahsulot.Nomi, ESearchCase::IgnoreCase)) TanlovMiqdor = R.Miqdor;
		return false;
	}
	case EBozorTanlovTur::MiqdorKop:
		TanlovMiqdor = FMath::Min(20.f, TanlovMiqdor + MiqdorQadami());
		return false;
	case EBozorTanlovTur::MiqdorKam:
		TanlovMiqdor = FMath::Max(MiqdorQadami(), TanlovMiqdor - MiqdorQadami());
		return false;
	case EBozorTanlovTur::MolRoyxati:
		bMolRoyxati = true;
		return false;
	case EBozorTanlovTur::Ortga:
		bMiqdorTanlash = bMolRoyxati = bTolov = false;
		Peshtaxta.Reset();
		return false;
	case EBozorTanlovTur::TolovOch:
		bTolov = true;
		Peshtaxta.Reset();
		return false;
	case EBozorTanlovTur::KupyuraQoy:
		Peshtaxta.Add(T.Kupyura);
		return false;
	case EBozorTanlovTur::KupyuraQaytar:
		Peshtaxta.Reset();
		return false;
	default:
		break;
	}
	bMiqdorTanlash = bMolRoyxati = false;
	OutMen = T.Matn;
	OutSotuvchi = Bajar(T.Niyat);
	// pul yetmasa to'lov oynasi ochiq qoladi (peshtaxtadagi pul ham) - yana kupyura qo'shish mumkin
	if (Holat.Bosqich != EBozorBosqich::PulKutilmoqda)
	{
		bTolov = false;
		Peshtaxta.Reset();
	}
	return true;
}

// ================= rastalar: sotuv.json + sahnadagi mollar

UBozorSotuvTizimi* UBozorSotuvTizimi::Get(const UObject* WorldContext)
{
	const UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	return W ? W->GetSubsystem<UBozorSotuvTizimi>() : nullptr;
}

void UBozorSotuvTizimi::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	Qur(InWorld);
}

const FBozorRasta* UBozorSotuvTizimi::Top(const ABozorNpc* Sotuvchi) const
{
	return Rastalar.FindByPredicate([Sotuvchi](const FBozorRasta& R) { return R.Sotuvchi == Sotuvchi; });
}

const FBozorRasta* UBozorSotuvTizimi::EngYaqin(const FVector& Joy, float MaxMasofa) const
{
	const FBozorRasta* Eng = nullptr;
	float EngD = FMath::Square(MaxMasofa);
	for (const FBozorRasta& R : Rastalar)
	{
		const float D = FVector::DistSquared2D(R.Markaz, Joy);
		if (D < EngD) { EngD = D; Eng = &R; }
	}
	return Eng;
}

void UBozorSotuvTizimi::Eslab(const ABozorNpc* Sotuvchi, int32 Munosabat)
{
	if (Sotuvchi) Xotira.Add(Sotuvchi, Munosabat);
}

int32 UBozorSotuvTizimi::BoshMunosabat(const FBozorRasta& R) const
{
	const int32* M = Xotira.Find(R.Sotuvchi.Get());
	return M ? FMath::RoundToInt(FMath::Lerp(float(*M), float(R.Xulq.BoshMunosabat), 0.3f)) : R.Xulq.BoshMunosabat;
}

FString UBozorSotuvTizimi::Hisobot() const
{
	FString S = FString::Printf(TEXT("%d ta rasta"), Rastalar.Num());
	for (const FBozorRasta& R : Rastalar)
	{
		const ABozorNpc* N = R.Sotuvchi;
		const bool bAyol = N && (N->bFemale || N->WalkStyle == 1 || N->WalkStyle == 3);
		const bool bKeksa = N && (N->WalkStyle == 2 || N->WalkStyle == 3);
		S += FString::Printf(TEXT("\n  stall %d %s (%s, %s%s%s):"), R.Stall, *GetNameSafe(R.Sotuvchi), BozorSotuv::FelNomi(R.Fel),
			bAyol ? TEXT("ayol") : TEXT("erkak"), bKeksa ? TEXT(", keksa") : TEXT(""), R.Xulq.Ism.IsEmpty() ? TEXT("") : *(TEXT(", ") + R.Xulq.Ism));
		for (const FBozorMahsulot& M : R.Mollar)
			S += FString::Printf(TEXT(" %s %s/%s (eng past %s);"), *M.Nomi, *BozorSotuv::Som(M.Narx), *M.Birlik, *BozorSotuv::Som(M.EngPastNarx));
	}
	return S;
}

void UBozorSotuvTizimi::Qur(UWorld& World)
{
	Rastalar.Reset();
	const FString Yol = FPaths::ProjectContentDir() / TEXT("Bozor/Data/sotuv.json");
	FString Matn;
	TSharedPtr<FJsonObject> Ildiz;
	if (!FFileHelper::LoadFileToString(Matn, *Yol) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Matn), Ildiz) || !Ildiz)
	{
		UE_LOG(LogBozorSotuv, Error, TEXT("sotuv.json o'qilmadi: %s"), *Yol);
		return;
	}
	const ABozorNpcManager* Mgr = ABozorNpcManager::Get(&World);
	if (!Mgr)
	{
		UE_LOG(LogBozorSotuv, Warning, TEXT("NPC menejeri yo'q - rastalar qurilmadi"));
		return;
	}

	// katalog
	struct FKatalog { FBozorMahsulot Asos; int32 Min = 0, Max = 0; TArray<FString> Kalitlar; };
	TArray<FKatalog> Katalog;
	for (const TSharedPtr<FJsonValue>& V : Ildiz->GetArrayField(TEXT("mahsulotlar")))
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		FKatalog K;
		K.Asos.Id = O->GetStringField(TEXT("id"));
		K.Asos.Nomi = O->GetStringField(TEXT("nomi"));
		K.Asos.Birlik = O->GetStringField(TEXT("birlik"));
		const TArray<TSharedPtr<FJsonValue>>& N = O->GetArrayField(TEXT("narx"));
		K.Min = static_cast<int32>(N[0]->AsNumber());
		K.Max = static_cast<int32>(N[1]->AsNumber());
		for (const TSharedPtr<FJsonValue>& S : O->GetArrayField(TEXT("sozlar"))) K.Asos.Sozlar.Add(S->AsString().ToLower());
		for (const TSharedPtr<FJsonValue>& S : O->GetArrayField(TEXT("kalitlar"))) K.Kalitlar.Add(S->AsString().ToLower());
		Katalog.Add(MoveTemp(K));
	}

	// fe'llar
	struct FFelMalumot { float EngPast = 0.85f, Qadam = 0.06f, Rad = 0.35f, XatoQaytim = 0.f; int32 Odob = 12, Qopol = -12; TMap<FString, TArray<FString>> Gaplar; FString Uslub = TEXT("xotirjam"); };
	const bool bHammaXato = FParse::Param(FCommandLine::Get(), TEXT("XatoQaytim"));   // sinov: hamma sotuvchi qaytimni kam beradi
	TMap<FString, FFelMalumot> Fellar;
	for (const auto& P : Ildiz->GetObjectField(TEXT("fellar"))->Values)
	{
		const TSharedPtr<FJsonObject> O = P.Value->AsObject();
		FFelMalumot F;
		O->TryGetNumberField(TEXT("engPast"), F.EngPast);
		O->TryGetNumberField(TEXT("qadam"), F.Qadam);
		O->TryGetNumberField(TEXT("rad"), F.Rad);
		O->TryGetNumberField(TEXT("odob"), F.Odob);
		O->TryGetNumberField(TEXT("qopol"), F.Qopol);
		O->TryGetNumberField(TEXT("xatoQaytim"), F.XatoQaytim);
		O->TryGetStringField(TEXT("uslub"), F.Uslub);
		if (bHammaXato) F.XatoQaytim = 1.f;
		const TSharedPtr<FJsonObject>* G = nullptr;
		if (O->TryGetObjectField(TEXT("gaplar"), G))
			for (const auto& Q : (*G)->Values)
				for (const TSharedPtr<FJsonValue>& S : Q.Value->AsArray()) F.Gaplar.FindOrAdd(FString(*Q.Key)).Add(S->AsString());
		Fellar.Add(FString(*P.Key), MoveTemp(F));
	}
	// darajalar va joriysi (?Daraja=N - "open" bilan o'tganda, -Daraja=N - buyruq qatorida)
	// umumiy sotuvchi gaplari (fe'l gaplari bo'lmasa shulardan)
	Gaplar.Reset();
	const TSharedPtr<FJsonObject>* GJ = nullptr;
	if (Ildiz->TryGetObjectField(TEXT("gaplar"), GJ))
		for (const auto& Q : (*GJ)->Values)
			for (const TSharedPtr<FJsonValue>& S : Q.Value->AsArray()) Gaplar.FindOrAdd(FString(*Q.Key)).Add(S->AsString());
	if (Gaplar.IsEmpty()) UE_LOG(LogBozorSotuv, Error, TEXT("sotuv.json: \"gaplar\" yo'q - sotuvchilar javob bera olmaydi"));
	Darajalar.Reset();
	const TArray<TSharedPtr<FJsonValue>>* DJ = nullptr;
	if (Ildiz->TryGetArrayField(TEXT("darajalar"), DJ))
		for (const TSharedPtr<FJsonValue>& V : *DJ)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			FBozorDaraja& D = Darajalar.AddDefaulted_GetRef();
			D.Raqam = Darajalar.Num();
			O->TryGetStringField(TEXT("nomi"), D.Nomi);
			O->TryGetStringField(TEXT("eslatma"), D.Eslatma);
			O->TryGetBoolField(TEXT("xatoQaytim"), D.bXatoQaytim);
			O->TryGetStringArrayField(TEXT("tarozi"), D.Tarozi);
			const TSharedPtr<FJsonObject>* Qj = nullptr;
			if (O->TryGetObjectField(TEXT("qongiroq"), Qj))
			{
				(*Qj)->TryGetStringField(TEXT("kim"), D.Qongiroqchi);
				const TArray<TSharedPtr<FJsonValue>>* Gj = nullptr;
				if ((*Qj)->TryGetArrayField(TEXT("gaplar"), Gj))
					for (const TSharedPtr<FJsonValue>& G : *Gj)
					{
						const TArray<TSharedPtr<FJsonValue>>& A = G->AsArray();
						if (A.Num() >= 2) D.QongiroqGaplar.Emplace(A[0]->AsString() != TEXT("bola"), A[1]->AsString());
					}
			}
			const TSharedPtr<FJsonObject>* Pul = nullptr;
			if (O->TryGetObjectField(TEXT("pul"), Pul))
				for (const auto& K : (*Pul)->Values) D.Kupyura.Add(FCString::Atoi(*FString(*K.Key)), static_cast<int32>(K.Value->AsNumber()));
			const TArray<TSharedPtr<FJsonValue>>* Royxat = nullptr;
			if (O->TryGetArrayField(TEXT("royxat"), Royxat))
				for (const TSharedPtr<FJsonValue>& Q : *Royxat)
				{
					const TArray<TSharedPtr<FJsonValue>>& A = Q->AsArray();
					if (A.Num() < 3) continue;
					FBozorRoyxatQator& R = D.Royxat.AddDefaulted_GetRef();
					R.Nomi = A[0]->AsString();
					if (R.Nomi.Len()) R.Nomi[0] = FChar::ToUpper(R.Nomi[0]);
					R.Miqdor = static_cast<float>(A[1]->AsNumber());
					R.Birlik = A[2]->AsString();
				}
		}
	int32 DarajaRaqami = 1;
	FParse::Value(FCommandLine::Get(), TEXT("Daraja="), DarajaRaqami);
	if (const AGameModeBase* GM = World.GetAuthGameMode()) DarajaRaqami = UGameplayStatics::GetIntOption(GM->OptionsString, TEXT("Daraja"), DarajaRaqami);
	DarajaIndeksi = FMath::Clamp(DarajaRaqami - 1, 0, FMath::Max(0, Darajalar.Num() - 1));
	const FBozorDaraja* Daraja = GetDaraja();
	// xato qaytim: darajada o'chirilgan bo'lsa hech kim xato bermaydi (sinov -XatoQaytim bundan ustun)
	const bool bXatoBor = bHammaXato || !Daraja || Daraja->bXatoQaytim;

	TArray<FString> Tartib;
	Ildiz->TryGetStringArrayField(TEXT("felTartibi"), Tartib);
	if (Tartib.IsEmpty()) Fellar.GetKeys(Tartib);
	TMap<int32, FString> StallFeli;
	const TArray<TSharedPtr<FJsonValue>>* Qolda = nullptr;
	if (Ildiz->TryGetArrayField(TEXT("rastalar"), Qolda))
		for (const TSharedPtr<FJsonValue>& V : *Qolda)
			StallFeli.Add(static_cast<int32>(V->AsObject()->GetNumberField(TEXT("stall"))), V->AsObject()->GetStringField(TEXT("fel")));

	// har sotuvchining xulq-atvori (xulq.json): kalit - NPC nomi ("BozorNpc_267") yoki "stall:N"
	TMap<FString, TSharedPtr<FJsonObject>> Xulqlar;
	{
		FString XMatn;
		TSharedPtr<FJsonObject> XIldiz;
		const FString XYol = FPaths::ProjectContentDir() / TEXT("Bozor/Data/xulq.json");
		if (FFileHelper::LoadFileToString(XMatn, *XYol) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(XMatn), XIldiz) && XIldiz)
		{
			const TSharedPtr<FJsonObject>* S = nullptr;
			if (XIldiz->TryGetObjectField(TEXT("sotuvchilar"), S))
				for (const auto& P : (*S)->Values)
					if (P.Value.IsValid() && P.Value->Type == EJson::Object) Xulqlar.Add(FString(*P.Key), P.Value->AsObject());
		}
		else if (FPaths::FileExists(XYol)) UE_LOG(LogBozorSotuv, Error, TEXT("xulq.json o'qilmadi (JSON xatosi): %s"), *XYol);
	}

	// sotuvchilar va ularning maydoni: sotuvchi + navbat boshlari, chetidan 60 sm
	// har mol uchun: nechta topildi, joylari yig'indisi (o'rtachasi uchun) va eng baland nuqtasi
	struct FMaydon { ABozorNpc* Sotuvchi; int32 Stall; FBox Box; FVector Navbat; TArray<int32> Soni; TArray<FVector> Yigindi; TArray<float> Tepa; TArray<const UStaticMesh*> Mesh; };
	TArray<FMaydon> Maydon;
	for (const FBozorQueueSpot& Q : Mgr->Queues)
	{
		if (!Q.Vendor || Q.Points.IsEmpty()) continue;
		FMaydon* M = Maydon.FindByPredicate([&Q](const FMaydon& X) { return X.Sotuvchi == Q.Vendor; });
		if (!M)
		{
			M = &Maydon.AddDefaulted_GetRef();
			M->Sotuvchi = Q.Vendor;
			M->Stall = Q.Stall;
			M->Box = FBox(ForceInit);
			M->Box += Q.Vendor->GetActorLocation();
			M->Navbat = Q.Points[0];
			M->Soni.SetNumZeroed(Katalog.Num());
			M->Yigindi.SetNumZeroed(Katalog.Num());
			M->Tepa.SetNumZeroed(Katalog.Num());
			M->Mesh.SetNumZeroed(Katalog.Num());
		}
		M->Box += Q.Points[0];
	}
	for (FMaydon& M : Maydon) M.Box = M.Box.ExpandBy(FVector(60.f, 60.f, 250.f));

	// sahnadagi mollar: mesh nomi katalog kalitiga mos kelsa, qaysi maydonda turgani sanaladi
	TMap<const UStaticMesh*, int32> MeshMol;
	auto MolIndeksi = [&](const UStaticMesh* Mesh) -> int32
	{
		if (const int32* I = MeshMol.Find(Mesh)) return *I;
		const FString Nom = Mesh->GetName().ToLower();
		int32 Topildi = INDEX_NONE;
		for (int32 i = 0; i < Katalog.Num() && Topildi == INDEX_NONE; ++i)
			for (const FString& K : Katalog[i].Kalitlar)
				if (Nom.Contains(K)) { Topildi = i; break; }
		MeshMol.Add(Mesh, Topildi);
		return Topildi;
	};
	auto Sana = [&](int32 Mol, const FBox& B, const UStaticMesh* Mesh)
	{
		const FVector P = B.GetCenter();
		for (FMaydon& M : Maydon)
			if (M.Box.IsInsideOrOn(P))
			{
				++M.Soni[Mol];
				M.Yigindi[Mol] += P;
				M.Tepa[Mol] += static_cast<float>(B.Max.Z);   // o'rtacha tepa (bitta baland narsa taxtachani ko'tarib yubormasin)
				// uzatish uchun eng kichigi (uyum emas, bitta dona)
				const UStaticMesh*& Eng = M.Mesh[Mol];
				if (!Eng || Mesh->GetBounds().SphereRadius < Eng->GetBounds().SphereRadius) Eng = Mesh;
			}
	};
	for (TActorIterator<AActor> It(&World); It; ++It)
	{
		TInlineComponentArray<UStaticMeshComponent*> Komp(*It);
		for (const UStaticMeshComponent* C : Komp)
		{
			const UStaticMesh* Mesh = C->GetStaticMesh();
			if (!Mesh) continue;
			const int32 Mol = MolIndeksi(Mesh);
			if (Mol == INDEX_NONE) continue;
			if (const UInstancedStaticMeshComponent* Ism = Cast<UInstancedStaticMeshComponent>(C))
			{
				FTransform Tr;
				const FBox Mahalliy = Mesh->GetBoundingBox();
				for (int32 i = 0; i < Ism->GetInstanceCount(); ++i)
					if (Ism->GetInstanceTransform(i, Tr, true)) Sana(Mol, Mahalliy.TransformBy(Tr), Mesh);
			}
			else Sana(Mol, C->Bounds.GetBox(), Mesh);
		}
	}

	// rastalar: mol topilmagan sotuvchi (mato, kiosk ...) savdoga qo'shilmaydi
	for (const FMaydon& M : Maydon)
	{
		FBozorRasta R;
		R.Sotuvchi = M.Sotuvchi;
		R.Stall = M.Stall;
		R.Markaz = M.Box.GetCenter();
		const FString* Qol = StallFeli.Find(M.Stall);
		FString FelKalit = Qol ? *Qol : (Tartib.Num() ? Tartib[FMath::Abs(M.Stall) % Tartib.Num()] : FString(TEXT("mehribon")));
		const TSharedPtr<FJsonObject>* XP = Xulqlar.Find(M.Sotuvchi->GetName());
		if (!XP) XP = Xulqlar.Find(FString::Printf(TEXT("stall:%d"), M.Stall));
		const TSharedPtr<FJsonObject> XO = XP ? *XP : nullptr;
		if (XO) XO->TryGetStringField(TEXT("fel"), FelKalit);
		const UEnum* E = StaticEnum<EBozorFel>();
		const int64 FelV = E->GetValueByNameString(FelKalit, EGetByNameFlags::CaseSensitive);
		if (FelV == INDEX_NONE)
			for (int32 i = 0; i < E->NumEnums() - 1; ++i)
				if (E->GetNameStringByIndex(i).Equals(FelKalit, ESearchCase::IgnoreCase)) { R.Fel = static_cast<EBozorFel>(E->GetValueByIndex(i)); break; }
		if (FelV != INDEX_NONE) R.Fel = static_cast<EBozorFel>(FelV);
		const FFelMalumot* F = Fellar.Find(FelKalit);
		const FFelMalumot Asl;
		if (!F) F = &Asl;
		R.RadEhtimoli = F->Rad;
		R.OdobBall = F->Odob;
		R.QopolBall = F->Qopol;
		R.XatoQaytim = bXatoBor ? F->XatoQaytim : 0.f;
		R.Gaplar = F->Gaplar;
		R.Asosiy = &Gaplar;
		R.Uslub = F->Uslub;
		float EngPast = F->EngPast, Qadam = F->Qadam;
		if (XO)
		{
			BozorSotuv::XulqniOqi(*XO, R.Xulq);
			// savdo: fe'ldagi qiymatlar ustidan (faqat berilganlari)
			const TSharedPtr<FJsonObject>* Sv = nullptr;
			if (XO->TryGetObjectField(TEXT("savdo"), Sv))
			{
				(*Sv)->TryGetNumberField(TEXT("engPast"), EngPast);
				(*Sv)->TryGetNumberField(TEXT("qadam"), Qadam);
				(*Sv)->TryGetNumberField(TEXT("rad"), R.RadEhtimoli);
				(*Sv)->TryGetNumberField(TEXT("odob"), R.OdobBall);
				(*Sv)->TryGetNumberField(TEXT("qopol"), R.QopolBall);
				double Xq = 0.0;
				if ((*Sv)->TryGetNumberField(TEXT("xatoQaytim"), Xq)) R.XatoQaytim = bXatoBor ? float(bHammaXato ? 1.0 : Xq) : 0.f;
			}
			// o'z gaplari: shu javob turlari fe'l gaplari o'rniga
			const TSharedPtr<FJsonObject>* G = nullptr;
			if (XO->TryGetObjectField(TEXT("gaplar"), G))
				for (const auto& Q : (*G)->Values)
				{
					TArray<FString>& Royxat = R.Gaplar.FindOrAdd(FString(*Q.Key));
					Royxat.Reset();
					for (const TSharedPtr<FJsonValue>& S : Q.Value->AsArray()) Royxat.Add(S->AsString());
				}
		}
		R.Xaridorga = (M.Navbat - M.Sotuvchi->GetActorLocation()).GetSafeNormal2D();
		for (int32 i = 0; i < Katalog.Num(); ++i)
		{
			if (M.Soni[i] == 0) continue;
			FBozorMahsulot P = Katalog[i].Asos;
			P.Narx = Yaxlit500(FMath::RandRange(Katalog[i].Min, Katalog[i].Max));
			P.EngPastNarx = FMath::Min(P.Narx, Yaxlit500(P.Narx * EngPast));
			P.Qadam = Yaxlit500(P.Narx * Qadam);
			R.Mollar.Add(MoveTemp(P));
			FVector J = M.Yigindi[i] / M.Soni[i];
			J.Z = M.Tepa[i] / M.Soni[i];
			R.MolJoylari.Add(J);
			R.MolMeshlari.Add(const_cast<UStaticMesh*>(M.Mesh[i]));
		}
		if (R.Mollar.Num()) Rastalar.Add(MoveTemp(R));
	}
	Rastalar.Sort([](const FBozorRasta& A, const FBozorRasta& B) { return A.Stall < B.Stall; });
	UE_LOG(LogBozorSotuv, Display, TEXT("Sotuv: %d-daraja (%s), %d sotuvchidan %s"), DarajaIndeksi + 1, Daraja ? *Daraja->Nomi : TEXT("-"), Maydon.Num(), *Hisobot());
	Taxtachalar(World);
}

void UBozorSotuvTizimi::Taxtachalar(UWorld& World)
{
	// eski taxtachalar (modelga "6 000" chizilgan) haqiqiy narxni ko'rsatmaydi - yashiriladi
	for (TActorIterator<AActor> It(&World); It; ++It)
	{
		TInlineComponentArray<UStaticMeshComponent*> Komp(*It);
		for (UStaticMeshComponent* C : Komp)
			if (C->GetStaticMesh() && C->GetStaticMesh()->GetName().Contains(TEXT("Narx_Taxtacha"))) C->SetVisibility(false);
	}
	// har mol uyumi ustida, xaridor tomonga biroz chiqarib, xaridorga qaratib; yonma-yon mollar bir-birini to'smasin
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	int32 Soni = 0;
	for (const FBozorRasta& R : Rastalar)
	{
		TArray<FVector> Qoyilgan;
		for (int32 i = 0; i < R.Mollar.Num(); ++i)
		{
			FVector J = R.MolJoylari[i] + R.Xaridorga * 18.f + FVector(0.f, 0.f, 16.f);
			for (const FVector& Q : Qoyilgan)
				if (FVector::Dist2D(Q, J) < 34.f && FMath::Abs(Q.Z - J.Z) < 20.f) J.Z = Q.Z + 21.f;
			Qoyilgan.Add(J);
			if (ABozorNarxTaxta* T = World.SpawnActor<ABozorNarxTaxta>(J, R.Xaridorga.Rotation(), P))
			{
				T->Yoz(R.Mollar[i]);
				++Soni;
			}
		}
	}
	UE_LOG(LogBozorSotuv, Display, TEXT("Narx taxtachalari: %d"), Soni);
}
