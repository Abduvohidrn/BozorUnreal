#include "BozorOvoz.h"

#include "BozorNpc.h"
#include "BozorSotuv.h"
#include "BozorOgizMorph.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "UnrealClient.h"
#include "AudioDevice.h"
#include "TimerManager.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/CommandLine.h"
#include "HAL/IConsoleManager.h"
#include "Misc/Paths.h"
#include "AudioMixerBlueprintLibrary.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWave.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogBozorOvoz, Log, All);

// Qadam balandligi: o'lchovda qadamlar bozor fonidan ~7 dB past edi (fon pasaytirilgandan keyin ham) - eshitilmasdi.
// 3x bilan yengil qadam fon ustidan biroz ajralib turadi (2026-11-16)
static TAutoConsoleVariable<float> CVarQadamBalandligi(TEXT("bozor.QadamBalandligi"), 3.f,
	TEXT("O'yinchi qadamlari balandligi ko'paytuvchisi (1 - asl)"));

TWeakObjectPtr<ABozorSotuvchiOvozi> ABozorSotuvchiOvozi::Instance;

namespace
{
	const TCHAR* Female[] = { TEXT("a1"), TEXT("a2") };
	const TCHAR* Male[] = { TEXT("e1"), TEXT("e2") };
	const FName Chaqiriq(TEXT("chaqiriq"));
}

ABozorSotuvchiOvozi::ABozorSotuvchiOvozi()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ABozorSotuvchiOvozi::BeginPlay()
{
	Super::BeginPlay();
	Instance = this;
	LoadDialog();
	if (FParse::Param(FCommandLine::Get(), TEXT("OgizSinov")))
	{
		FTimerHandle H;
		GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateUObject(this, &ABozorSotuvchiOvozi::OgizSinov, 0), 6.f, false);
	}
	for (USoundBase* C : Clips)
	{
		if (!C) continue;
		// sot_<tur>_<bolim>_<persona>_<nn>
		TArray<FString> P;
		C->GetName().ParseIntoArray(P, TEXT("_"));
		if (P.Num() < 5 || P[0] != TEXT("sot")) continue;
		ByKey.FindOrAdd(P[1] + TEXT("_") + P[2] + TEXT("_") + P[3]).Add(C);
	}
	if (ByKey.Num() == 0)
	{
		// suhbat gaplari (SayGap) baribir ishlaydi
		UE_LOG(LogBozorOvoz, Warning, TEXT("[BozorOvoz] sotuvchi kliplari yo'q - chaqiriqlar o'chirildi"));
		return;
	}
	NextAmbient = GetWorld()->GetTimeSeconds() + FMath::FRandRange(1.f, 3.f);
	UE_LOG(LogBozorOvoz, Display, TEXT("[BozorOvoz] klip=%d to'plam=%d"), Clips.Num(), ByKey.Num());
}

void ABozorSotuvchiOvozi::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Instance.Get() == this) Instance = nullptr;
	Super::EndPlay(Reason);
}

// ABozorNpc chaqiradi; Chance - gapirish ehtimoli (har savdo gapida ham gapirsa, ko'p bo'lib ketadi)
void ABozorSotuvchiOvozi::Say(ABozorNpc* Vendor, FName Kind, float Chance)
{
	ABozorSotuvchiOvozi* O = Instance.Get();
	if (!O || O->ByKey.IsEmpty() || !Vendor) return;
	if (Kind == TEXT("savdo")) Chance *= O->SavdoChance;
	else if (Kind == TEXT("rahmat")) Chance *= O->RahmatChance;
	if (FMath::FRand() < Chance) O->TrySay(Vendor, Kind);
}

namespace
{
	// 0..999 999 999 -> o'zbekcha so'z ("12000" -> "o'n ikki ming"); 100 -> "yuz", 1000 -> "ming"
	FString SonSoz(int64 N)
	{
		static const TCHAR* Bir[] = {TEXT(""), TEXT("bir"), TEXT("ikki"), TEXT("uch"), TEXT("to'rt"), TEXT("besh"), TEXT("olti"), TEXT("yetti"), TEXT("sakkiz"), TEXT("to'qqiz")};
		static const TCHAR* On[] = {TEXT(""), TEXT("o'n"), TEXT("yigirma"), TEXT("o'ttiz"), TEXT("qirq"), TEXT("ellik"), TEXT("oltmish"), TEXT("yetmish"), TEXT("sakson"), TEXT("to'qson")};
		if (N == 0) return TEXT("nol");
		auto Uch = [&](int32 X)
		{
			TArray<FString> W;
			if (X >= 100) { if (X / 100 > 1) W.Add(Bir[X / 100]); W.Add(TEXT("yuz")); }
			if (X % 100 >= 10) W.Add(On[(X % 100) / 10]);
			if (X % 10) W.Add(Bir[X % 10]);
			return FString::Join(W, TEXT(" "));
		};
		TArray<FString> W;
		if (const int32 M = int32(N / 1000000 % 1000)) { W.Add(Uch(M)); W.Add(TEXT("million")); }
		if (const int32 K = int32(N / 1000 % 1000)) { if (K > 1) W.Add(Uch(K)); W.Add(TEXT("ming")); }
		if (const int32 R = int32(N % 1000)) W.Add(Uch(R));
		return FString::Join(W, TEXT(" "));
	}
}

FString ABozorSotuvchiOvozi::NormalGap(const FString& Matn)
{
	// "12 000" (BozorSotuv::Som) -> "12000": raqamlar orasidagi bo'shliq ming ajratgichi
	FString S;
	for (int32 i = 0; i < Matn.Len(); ++i)
	{
		const TCHAR C = Matn[i];
		if (C == TEXT(' ') && i > 0 && FChar::IsDigit(Matn[i - 1]) && i + 3 < Matn.Len() && FChar::IsDigit(Matn[i + 1])
			&& FChar::IsDigit(Matn[i + 2]) && FChar::IsDigit(Matn[i + 3]) && (i + 4 >= Matn.Len() || !FChar::IsDigit(Matn[i + 4])))
			continue;
		S.AppendChar(C);
	}
	FString Out;
	for (int32 i = 0; i < S.Len();)
	{
		TCHAR C = S[i];
		if (FChar::IsDigit(C))
		{
			int64 N = 0;
			while (i < S.Len() && FChar::IsDigit(S[i]) && N < 1000000000) N = N * 10 + (S[i++] - TEXT('0'));
			// miqdor: "1,5" -> "bir yarim", "0,5" -> "yarim" (BozorSotuv::Miqdor)
			const bool bYarim = i + 1 < S.Len() && (S[i] == TEXT(',') || S[i] == TEXT('.')) && S[i + 1] == TEXT('5')
				&& (i + 2 >= S.Len() || !FChar::IsDigit(S[i + 2]));
			Out += TEXT(" ") + (bYarim && N == 0 ? FString() : SonSoz(N)) + (bYarim ? TEXT(" yarim ") : TEXT(" "));
			if (bYarim) i += 2;
			continue;
		}
		// barcha apostrof turlari -> ', harf va ' dan boshqasi bo'shliq
		if (C == 0x02BB || C == 0x02BC || C == 0x2018 || C == 0x2019 || C == TEXT('`')) C = TEXT('\'');
		C = FChar::ToLower(C);
		Out.AppendChar(FChar::IsAlpha(C) || C == TEXT('\'') ? C : TEXT(' '));
		++i;
	}
	TArray<FString> W;
	Out.ParseIntoArrayWS(W);
	return FString::Join(W, TEXT(" "));
}

void ABozorSotuvchiOvozi::LoadDialog()
{
	Dialog.Reset();
	DialogSinov.Reset();
	const FString Yol = FPaths::ProjectContentDir() / TEXT("Bozor/Data/ovoz_gaplar.json");
	FString Matn;
	TSharedPtr<FJsonObject> Ildiz;
	if (!FFileHelper::LoadFileToString(Matn, *Yol) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Matn), Ildiz) || !Ildiz)
	{
		UE_LOG(LogBozorOvoz, Log, TEXT("[BozorOvoz] suhbat ovozlari yo'q (%s)"), *Yol);
		return;
	}
	const TArray<TSharedPtr<FJsonValue>>* Ovozlar = nullptr;
	int32 Jami = 0;
	if (Ildiz->TryGetArrayField(TEXT("ovozlar"), Ovozlar))
	{
		for (const TSharedPtr<FJsonValue>& V : *Ovozlar)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			const TSharedPtr<FJsonObject>* Ib = nullptr;
			if (!O || !O->TryGetObjectField(TEXT("iboralar"), Ib)) continue;
			FDialogOvoz& D = Dialog.FindOrAdd(O->GetStringField(TEXT("fel")) + (O->GetBoolField(TEXT("ayol")) ? TEXT(" ayol") : TEXT(" erkak")));
			for (const TPair<FString, TSharedPtr<FJsonValue>>& P : (*Ib)->Values)
			{
				const FString K = NormalGap(P.Key);
				if (K.IsEmpty()) continue;
				D.Iboralar.Add(K, FSoftObjectPath(P.Value->AsString()));
				int32 Soz = 1;
				for (TCHAR C : K) Soz += C == TEXT(' ') ? 1 : 0;
				D.EngUzun = FMath::Max(D.EngUzun, Soz);
				++Jami;
			}
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* Sinov = nullptr;
	if (Ildiz->TryGetArrayField(TEXT("sinov"), Sinov))
		for (const TSharedPtr<FJsonValue>& V : *Sinov)
			if (const TSharedPtr<FJsonObject> O = V->AsObject())
				DialogSinov.Emplace(O->GetStringField(TEXT("fel")), O->GetStringField(TEXT("matn")));
	UE_LOG(LogBozorOvoz, Display, TEXT("[BozorOvoz] suhbat ovozlari: %d ovoz, %d ibora"), Dialog.Num(), Jami);
	if (FParse::Param(FCommandLine::Get(), TEXT("OvozTest")))
	{
		FTimerHandle H;
		GetWorldTimerManager().SetTimer(H, this, &ABozorSotuvchiOvozi::DialogTest, 5.f, false);
	}
}

bool ABozorSotuvchiOvozi::Qopla(const FDialogOvoz& Ovoz, const FString& Normal, TArray<FSoftObjectPath>& Out) const
{
	Out.Reset();
	TArray<FString> W;
	Normal.ParseIntoArray(W, TEXT(" "));
	for (int32 i = 0; i < W.Num();)
	{
		bool bTopildi = false;
		for (int32 n = FMath::Min(Ovoz.EngUzun, W.Num() - i); n >= 1 && !bTopildi; --n)
		{
			FString K = W[i];
			for (int32 j = 1; j < n; ++j) K += TEXT(" ") + W[i + j];
			if (const FSoftObjectPath* P = Ovoz.Iboralar.Find(K))
			{
				Out.Add(*P);
				i += n;
				bTopildi = true;
			}
		}
		if (!bTopildi)
		{
			UE_LOG(LogBozorOvoz, Verbose, TEXT("[BozorOvoz] qoplanmadi: \"%s\" (%d-so'z \"%s\")"), *Normal, i, *W[i]);
			Out.Reset();
			return false;
		}
	}
	return Out.Num() > 0;
}

// -OvozTest: normallash, har ovozda namunaviy gaplarning qoplanishi, kliplar yuklanishi va sotuvchi og'zidan chalinishi;
// natija logda "[OvozTest]" qatorlarida, keyin o'yin yopiladi
void ABozorSotuvchiOvozi::DialogTest()
{
	int32 Xato = 0;
	const TPair<const TCHAR*, const TCHAR*> Juft[] = {
		{TEXT("Mayli, siz uchun 10 000 so'm qilaman."), TEXT("Mayli, siz uchun o'n ming so'm qilaman.")},
		{TEXT("Xo'p, 12 000, bo'ldi, oling!"), TEXT("Xo'p, o'n ikki ming, bo'ldi, oling!")},
		{TEXT("Bu 1 500 so’m"), TEXT("Bu ming besh yuz so'm")},
		{TEXT("Mana: 1,5 kilo."), TEXT("Mana bir yarim kilo")},
		{TEXT("0,5 kilo"), TEXT("yarim kilo")},
	};
	for (const auto& J : Juft)
	{
		const bool bOk = NormalGap(J.Key) == NormalGap(J.Value);
		Xato += bOk ? 0 : 1;
		UE_LOG(LogBozorOvoz, Display, TEXT("[OvozTest] normal %s: \"%s\" | \"%s\""), bOk ? TEXT("OK") : TEXT("XATO"), *NormalGap(J.Key), *NormalGap(J.Value));
	}
	// har ovoz: namunaviy gaplar qanchasi to'liq qoplanadi, iboralar yuklanadimi
	for (const TPair<FString, FDialogOvoz>& D : Dialog)
	{
		int32 Yuklandi = 0;
		for (const TPair<FString, FSoftObjectPath>& P : D.Value.Iboralar) Yuklandi += P.Value.TryLoad() ? 1 : 0;
		int32 Sinov = 0, Qoplandi = 0;
		TArray<FSoftObjectPath> Q;
		for (const TPair<FString, FString>& S : DialogSinov)
		{
			if (!D.Key.StartsWith(S.Key + TEXT(" "))) continue;
			++Sinov;
			if (Qopla(D.Value, NormalGap(S.Value), Q)) ++Qoplandi;
			else UE_LOG(LogBozorOvoz, Display, TEXT("[OvozTest] %s qoplanmadi: %s"), *D.Key, *S.Value);
		}
		Xato += Yuklandi == D.Value.Iboralar.Num() ? 0 : 1;
		UE_LOG(LogBozorOvoz, Display, TEXT("[OvozTest] ovoz %s: ibora=%d yuklandi=%d gap %d/%d qoplandi"), *D.Key, D.Value.Iboralar.Num(), Yuklandi, Qoplandi, Sinov);
	}
	// har rasta (fe'l + jins) uchun bitta gapni haqiqatan chalib ko'radi
	int32 Rasta = 0, Aytdi = 0;
	if (const UBozorSotuvTizimi* Tz = UBozorSotuvTizimi::Get(this))
	{
		for (const FBozorRasta& R : Tz->GetRastalar())
		{
			if (!R.Sotuvchi) continue;
			++Rasta;
			const FString Fel = BozorSotuv::FelNomi(R.Fel);
			for (const TPair<FString, FString>& S : DialogSinov)
				if (S.Key == Fel) { Aytdi += SayLine(R.Sotuvchi, Fel, S.Value) ? 1 : 0; break; }
		}
	}
	Xato += Aytdi == Rasta ? 0 : 1;
	UE_LOG(LogBozorOvoz, Display, TEXT("[OvozTest] rasta=%d ovozli=%d xato=%d"), Rasta, Aytdi, Xato);
	UKismetSystemLibrary::QuitGame(this, nullptr, EQuitPreference::Quit, false);
}

bool ABozorSotuvchiOvozi::SayLine(ABozorNpc* Vendor, const FString& Fel, const FString& Matn)
{
	ABozorSotuvchiOvozi* O = Instance.Get();
	if (!O || !Vendor || O->Dialog.Num() == 0) return false;
	// erkak sotuvchidan ayol ovozi (yoki aksincha) chiqmasin
	const FDialogOvoz* D = O->Dialog.Find(Fel + (Vendor->Ayolmi() ? TEXT(" ayol") : TEXT(" erkak")));
	if (!D) return false;
	const FString Kalit = NormalGap(Matn);
	TArray<FSoftObjectPath> Yollar;
	if (!O->Qopla(*D, Kalit, Yollar)) return false;
	TArray<USoundBase*> Bolaklar;
	float Uzunlik = 0.f;
	for (const FSoftObjectPath& P : Yollar)
	{
		USoundBase* C = Cast<USoundBase>(P.TryLoad());
		if (!C)
		{
			UE_LOG(LogBozorOvoz, Warning, TEXT("[BozorOvoz] suhbat klipi yuklanmadi: %s"), *P.ToString());
			return false;
		}
		O->DialogLoaded.AddUnique(C);
		Bolaklar.Add(C);
		Uzunlik += C->GetDuration();
	}
	FVoice& V = O->Get(Vendor);
	V.Navbat.Reset();
	V.GapNavbat.Reset();
	V.Src->Stop();   // suhbat gapi chaqiriqdan muhimroq (Stop BolakTugadi'ni chaqirsa ham navbat bo'sh)
	V.Src->SetVolumeMultiplier(O->Volume);
	V.Src->SetPitchMultiplier(1.f);   // yozilgan ohang - xarakterning o'zi, o'zgartirmaymiz
	V.Src->SetSound(Bolaklar[0]);
	V.Navbat = TArray<USoundBase*>(Bolaklar.GetData() + 1, Bolaklar.Num() - 1);
	V.Src->Play();
	V.Last = Bolaklar.Last();
	V.bCall = false;
	V.NextTime = O->GetWorld()->GetTimeSeconds() + Uzunlik + 0.06f * Bolaklar.Num() + 0.5;
	UE_LOG(LogBozorOvoz, Display, TEXT("[BozorOvoz] suhbat: %s (%s %s) %d bo'lak, %.1f s: %s"), *Vendor->GetActorNameOrLabel(), *Fel,
		Vendor->Ayolmi() ? TEXT("ayol") : TEXT("erkak"), Bolaklar.Num(), Uzunlik, *Kalit);
	return true;
}

bool ABozorSotuvchiOvozi::SayWave(ABozorNpc* Vendor, USoundWave* Ovoz)
{
	ABozorSotuvchiOvozi* O = Instance.Get();
	if (!O || !Vendor || !Ovoz) return false;
	// 3D so'nish, ovoz sinfi (fon ducking) va bir vaqtdagi gaplar chegarasi - yozilgan kliplar bilan bir xil
	USoundBase* Namuna = O->Clips.Num() ? O->Clips[0].Get() : nullptr;
	if (Namuna)
	{
		Ovoz->AttenuationSettings = Namuna->AttenuationSettings;
		Ovoz->SoundClassObject = Namuna->SoundClassObject;
		Ovoz->ConcurrencySet = Namuna->ConcurrencySet;
	}
	O->Jonli.Add(Ovoz);
	while (O->Jonli.Num() > 6) O->Jonli.RemoveAt(0);
	FVoice& V = O->Get(Vendor);
	V.Navbat.Reset();
	V.GapNavbat.Reset();
	V.Src->Stop();
	// suhbatdagi jonli gap - o'yinchi aynan shuni tinglayapti: bozor shovqini va chaqiriqlardan balandroq
	V.Src->SetVolumeMultiplier(O->Volume * O->JonliBalandlik);
	// sotuvchining o'z ovoz tezligi (xulq.json "ovoz.tezlik"): keksalar sekinroq va pastroq. Chalish shuncha uzayadi -
	// to'xtash vaqtlari (pastda va JonliDavom) shunga bo'linadi
	const UBozorSotuvTizimi* Tz = UBozorSotuvTizimi::Get(Vendor);
	const FBozorRasta* R = Tz ? Tz->Top(Vendor) : nullptr;
	const float Tez = R ? R->Xulq.OvozTezlik : 1.f;
	V.Src->SetPitchMultiplier(Tez);
	V.Src->SetSound(Ovoz);
	V.Src->Play();
	V.Last = Ovoz;
	V.bCall = false;
	const double Now = O->GetWorld()->GetTimeSeconds();
	V.NextTime = Now + Ovoz->Duration / Tez + 0.5;
	// procedural tovush ma'lumoti tugagach ham jim "chalinib" turadi - vaqti kelganda o'zimiz to'xtatamiz
	TWeakObjectPtr<UAudioComponent> Src(V.Src);
	TWeakObjectPtr<USoundWave> W(Ovoz);
	O->GetWorldTimerManager().SetTimer(O->JonliTugash, FTimerDelegate::CreateLambda([Src, W]()
	{
		if (Src.IsValid() && W.IsValid() && Src->Sound == W.Get()) Src->Stop();
	}), Ovoz->Duration / Tez + 0.15f, false);
	UE_LOG(LogBozorOvoz, Display, TEXT("[BozorOvoz] jonli: %s %.1f s"), *Vendor->GetActorNameOrLabel(), Ovoz->Duration);
	return true;
}

void ABozorSotuvchiOvozi::JonliDavom(USoundWave* Ovoz, float Qolgan)
{
	ABozorSotuvchiOvozi* O = Instance.Get();
	if (!O || !Ovoz) return;
	for (TPair<TWeakObjectPtr<ABozorNpc>, FVoice>& P : O->Voices)
	{
		FVoice& V = P.Value;
		if (!V.Src || V.Src->Sound != Ovoz) continue;
		V.NextTime = O->GetWorld()->GetTimeSeconds() + Qolgan / FMath::Max(0.5f, V.Src->PitchMultiplier) + 0.5;
		TWeakObjectPtr<UAudioComponent> Src(V.Src);
		TWeakObjectPtr<USoundWave> W(Ovoz);
		O->GetWorldTimerManager().SetTimer(O->JonliTugash, FTimerDelegate::CreateLambda([Src, W]()
		{
			if (Src.IsValid() && W.IsValid() && Src->Sound == W.Get()) Src->Stop();
		}), FMath::Max(0.05f, Qolgan / FMath::Max(0.5f, V.Src->PitchMultiplier) + 0.15f), false);
		return;
	}
}

void ABozorSotuvchiOvozi::Jimla(ABozorNpc* Vendor)
{
	ABozorSotuvchiOvozi* O = Instance.Get();
	FVoice* V = O && Vendor ? O->Voices.Find(Vendor) : nullptr;
	if (!V || !V->Src) return;
	V->Navbat.Reset();
	V->GapNavbat.Reset();
	V->Src->FadeOut(0.12f, 0.f);   // keskin kesilsa "chirt" eshitiladi
	O->GetWorldTimerManager().ClearTimer(O->JonliTugash);
}

void ABozorSotuvchiOvozi::JimGapir(ABozorNpc* Vendor, const FString& Matn)
{
	ABozorSotuvchiOvozi* O = Instance.Get();
	if (!O || !Vendor) return;
	// "[Tarozida: ...]" - o'yin belgisi, aytilmaydi
	FString Gap = Matn;
	const int32 Q = Gap.Find(TEXT("["));
	if (Q > 0) Gap.LeftInline(Q);
	Gap.TrimStartAndEndInline();
	if (Gap.IsEmpty() || Gap == TEXT("...")) return;
	FVoice& V = O->Get(Vendor);
	if (V.Src->IsPlaying()) return;
	// ~15 harf/s (o'zbekcha so'zlashuv tezligi), gap oxiridagi nuqta - qisqa to'xtash
	V.JimGacha = O->GetWorld()->GetTimeSeconds() + FMath::Clamp(Gap.Len() * 0.065f, 0.6f, 9.f);
}

bool ABozorSotuvchiOvozi::Gapiryapti(ABozorNpc* Vendor)
{
	ABozorSotuvchiOvozi* O = Instance.Get();
	const FVoice* V = O && Vendor ? O->Voices.Find(Vendor) : nullptr;
	return V && V->Src && V->Src->IsPlaying();
}

// bo'lak tugadi - shu sotuvchining navbatdagi bo'lagi (bo'laklar orasida tabiiy qisqa nafas: klip boshidagi 15 ms so'nish)
void ABozorSotuvchiOvozi::BolakTugadi(UAudioComponent* Src)
{
	for (TPair<TWeakObjectPtr<ABozorNpc>, FVoice>& P : Voices)
	{
		FVoice& V = P.Value;
		if (V.Src != Src || V.Navbat.Num() == 0) continue;
		USoundBase* Keyingi = V.Navbat[0];
		V.Navbat.RemoveAt(0);
		V.Src->SetSound(Keyingi);
		V.Src->Play();
		return;
	}
}

void ABozorSotuvchiOvozi::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	OgizYangila(DeltaSeconds);
	// suhbat gaplari: bo'lak tugashi bilan keyingisi
	for (TPair<TWeakObjectPtr<ABozorNpc>, FVoice>& P : Voices)
	{
		FVoice& V = P.Value;
		if (V.GapNavbat.IsEmpty() || !V.Src || V.Src->IsPlaying()) continue;
		if (V.GapI < V.GapNavbat.Num()) NavbatDavom(V);
		else V.GapNavbat.Reset();
	}
	if (ByKey.IsEmpty()) return;
	const double Now = GetWorld()->GetTimeSeconds();
	FVector L;
	if (!ListenerLocation(L)) return;
	if (Now >= NextScan)
	{
		NextScan = Now + 5.0;
		Vendors.Reset();
		for (TActorIterator<ABozorNpc> It(GetWorld()); It; ++It)
		{
			if (It->NpcRole == EBozorRole::Sotuvchi) Vendors.Add(*It);
		}
	}
	if (Now < NextAmbient) return;
	NextAmbient = Now + FMath::FRandRange(AmbientGap.X, AmbientGap.Y);
	// o'yinchi atrofidagi bo'sh sotuvchilardan biri baqiradi
	TArray<ABozorNpc*> Near;
	for (const TWeakObjectPtr<ABozorNpc>& V : Vendors)
	{
		if (V.IsValid() && !V->Deal.IsValid() && FVector::DistSquared(V->GetActorLocation(), L) < AmbientRadius * AmbientRadius) Near.Add(V.Get());
	}
	if (Near.Num() > 0) TrySay(Near[FMath::RandRange(0, Near.Num() - 1)], FMath::FRand() < 0.85f ? Chaqiriq : FName(TEXT("salom")));
}

bool ABozorSotuvchiOvozi::ListenerLocation(FVector& Out) const
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC) return false;
	FVector Front, Right;
	PC->GetAudioListenerPosition(Out, Front, Right);
	return true;
}

bool ABozorSotuvchiOvozi::TrySay(ABozorNpc* Vendor, FName Kind)
{
	FVector L;
	if (!ListenerLocation(L) || FVector::DistSquared(Vendor->GetActorLocation(), L) > HearDistance * HearDistance) return false;
	const double Now = GetWorld()->GetTimeSeconds();
	FVoice& V = Get(Vendor);
	if (V.Src->IsPlaying() || Now < V.NextTime || V.Navbat.Num() || V.GapNavbat.Num()) return false;   // suhbat gapi o'rtasida baqirmaydi
	const bool bCall = Kind == Chaqiriq;
	if (bCall && ActiveCalls() >= MaxCalls) return false;
	const bool bSavdo = Kind == TEXT("savdo");
	if (bSavdo && Now < NextSavdo) return false;
	USoundBase* Clip = Pick(V, Kind, Now);
	if (!Clip) return false;           // hamma gap yaqinda aytilgan - takrorlamay jim turadi
	V.Src->SetSound(Clip);
	V.Src->SetVolumeMultiplier(Volume * FMath::FRandRange(VolumeRange.X, VolumeRange.Y));
	V.Src->SetPitchMultiplier(FMath::FRandRange(PitchRange.X, PitchRange.Y));
	V.Src->Play();
	V.Last = Clip;
	Heard.Add(SourceKey(Clip), Now);
	if (bSavdo) NextSavdo = Now + Clip->GetDuration() + SavdoGap;
	V.bCall = bCall;
	V.NextTime = Now + Clip->GetDuration() + (bCall ? CallCooldown * FMath::FRandRange(0.8f, 1.6f) : 0.3f);
	UE_LOG(LogBozorOvoz, Verbose, TEXT("[BozorOvoz] %s: %s"), *Vendor->GetActorNameOrLabel(), *Clip->GetName());
	return true;
}

bool ABozorSotuvchiOvozi::SayGap(ABozorNpc* Vendor, const TArray<FString>& Bolaklar, const FString& Uslub)
{
	ABozorSotuvchiOvozi* O = Instance.Get();
	if (!O || !Vendor || Bolaklar.IsEmpty()) return false;
	FVoice& V = O->Get(Vendor);
	// ayol personalar (a1..a3) bitta ayol ovozida, erkaklar (e1, e2) bitta erkak ovozida - farqi pitch'da (NavbatDavom)
	const FString Ovoz = V.Persona.StartsWith(TEXT("e")) ? TEXT("erkak") : TEXT("ayol");
	const FString U = Uslub.Replace(TEXT("'"), TEXT(""));
	TArray<USoundBase*> Navbat;
	TArray<FString> Yoq;
	for (const FString& B : Bolaklar)
	{
		// qolip matni - sotuvchi uslubida (bo'lmasa xotirjam), son/mol/birlik/miqdor - uslubsiz
		const bool bMatn = B.StartsWith(TEXT("G_"));
		USoundBase* S = O->Bolak(B + TEXT("_") + Ovoz + (bMatn ? TEXT("_") + U : FString()));
		if (!S && bMatn) S = O->Bolak(B + TEXT("_") + Ovoz + TEXT("_xotirjam"));
		if (S) Navbat.Add(S);
		else Yoq.Add(B);
	}
	if (Yoq.Num())
	{
		UE_LOG(LogBozorOvoz, Display, TEXT("[BozorOvoz] gap ovozsiz: %d/%d bo'lak topilmadi (%s): %s"), Yoq.Num(), Bolaklar.Num(), *Ovoz, *FString::Join(Yoq, TEXT(" ")));
		return false;
	}
	V.Navbat.Reset();
	V.Src->Stop();
	V.GapNavbat = MoveTemp(Navbat);
	V.GapI = 0;
	V.bCall = false;
	O->NavbatDavom(V);
	UE_LOG(LogBozorOvoz, Log, TEXT("[BozorOvoz] gap: %s, %d bo'lak, %s/%s"), *Vendor->GetActorNameOrLabel(), V.GapNavbat.Num(), *V.Persona, *U);
	return true;
}

void ABozorSotuvchiOvozi::NavbatDavom(FVoice& V)
{
	static const TMap<FString, float> Pitch = { {TEXT("a2"), 1.07f}, {TEXT("a3"), 0.93f}, {TEXT("e2"), 0.92f} };
	const float* P = Pitch.Find(V.Persona);
	USoundBase* S = V.GapNavbat[V.GapI++];
	V.Src->SetSound(S);
	V.Src->SetVolumeMultiplier(Volume);
	V.Src->SetPitchMultiplier(P ? *P : 1.f);
	V.Src->Play();
	V.Last = S;
	V.NextTime = GetWorld()->GetTimeSeconds() + S->GetDuration() + 2.f;   // gapdan keyin darhol baqirmaydi
}

USoundBase* ABozorSotuvchiOvozi::Bolak(const FString& Nom)
{
	if (const TObjectPtr<USoundBase>* S = GapKesh.Find(Nom)) return *S;
	USoundBase* S = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Bozor/Audio/Gaplar/%s.%s"), *Nom, *Nom), nullptr, LOAD_NoWarn | LOAD_Quiet);
	GapKesh.Add(Nom, S);
	return S;
}

int32 ABozorSotuvchiOvozi::ActiveCalls() const
{
	int32 N = 0;
	for (const TPair<TWeakObjectPtr<ABozorNpc>, FVoice>& P : Voices)
	{
		N += P.Value.bCall && P.Value.Src && P.Value.Src->IsPlaying() ? 1 : 0;
	}
	return N;
}

ABozorSotuvchiOvozi::FVoice& ABozorSotuvchiOvozi::Get(ABozorNpc* Vendor)
{
	if (FVoice* Found = Voices.Find(Vendor); Found && Found->Src) return *Found;
	FVoice V;
	V.Section = Bolim(Vendor->GetActorLocation());
	// keksalar (WalkStyle 2/3) - keksa ovoz; qolganlar navbatma-navbat: yonma-yon rastalar bir xil ovozda gapirmasin
	V.Persona = Vendor->WalkStyle == 3 ? TEXT("a3") : Vendor->WalkStyle == 2 ? TEXT("e2")
		: Vendor->bFemale ? Female[FemaleCount++ % UE_ARRAY_COUNT(Female)] : Male[MaleCount++ % UE_ARRAY_COUNT(Male)];
	// og'iz balandligida (oyoqdan ~155 sm), sotuvchi bilan birga buriladi
	UAudioComponent* A = NewObject<UAudioComponent>(Vendor, TEXT("SotuvchiOvozi"));
	A->bAutoActivate = false;
	A->bAutoDestroy = false;
	const float Half = Vendor->GetCapsuleComponent() ? Vendor->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 88.f;
	A->SetupAttachment(Vendor->GetRootComponent());
	A->SetRelativeLocation(FVector(15.f, 0.f, 155.f - Half));
	A->RegisterComponent();
	A->OnAudioFinishedNative.AddUObject(this, &ABozorSotuvchiOvozi::BolakTugadi);   // suhbat gapining keyingi bo'lagi
	// og'iz uchun ovoz balandligi konverti: delegate Play'dan oldin bog'langan bo'lsagina hisoblanadi
	A->OnAudioSingleEnvelopeValueNative.AddWeakLambda(this, [this](const UAudioComponent* C, const USoundWave*, const float E)
	{
		for (TPair<TWeakObjectPtr<ABozorNpc>, FVoice>& P : Voices)
			if (P.Value.Src == C)
			{
				P.Value.Konvert = E;
				P.Value.KonvertVaqt = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
				return;
			}
	});
	V.Src = A;
	UE_LOG(LogBozorOvoz, Log, TEXT("[BozorOvoz] %s: %s, %s"), *Vendor->GetActorNameOrLabel(), *V.Persona, *V.Section);
	return Voices.Add(Vendor, MoveTemp(V));
}

void ABozorSotuvchiOvozi::OgizYangila(float Dt)
{
	const double Now = GetWorld()->GetTimeSeconds();
	for (TPair<TWeakObjectPtr<ABozorNpc>, FVoice>& P : Voices)
	{
		FVoice& V = P.Value;
		ABozorNpc* Npc = P.Key.Get();
		if (!Npc || !V.Src) continue;
		if (!V.bBoshlarTopildi)
		{
			V.bBoshlarTopildi = true;
			TArray<USkeletalMeshComponent*> Qismlar;
			Npc->GetComponents(Qismlar);
			for (USkeletalMeshComponent* Q : Qismlar)
				if (Q->GetSkeletalMeshAsset() && Q->GetSkeletalMeshAsset()->FindMorphTarget(UBozorOgizMorph::MorphNomi)) V.Boshlar.Add(Q);
		}
		if (V.Boshlar.IsEmpty()) continue;
		float Maqsad = 0.f;
		if (!V.Src->IsPlaying() && Now < V.JimGacha)
		{
			// ovozsiz gap: bo'g'in ritmi, gap oxirida yopiladi
			V.Faza += Dt * (9.f + 4.f * FMath::PerlinNoise1D(Now * 0.7f + Npc->GetUniqueID()));
			Maqsad = (0.2f + 0.6f * FMath::Abs(FMath::Sin(V.Faza))) * FMath::Clamp(float(V.JimGacha - Now) / 0.2f, 0.f, 1.f);
		}
		else if (V.Src->IsPlaying())
		{
			V.JimGacha = -100.0;   // haqiqiy ovoz boshlandi
			if (Now - V.KonvertVaqt < 0.25)
			{
				// konvert: so'nuvchi eng kattasiga nisbatan (baland/past gapirgan sotuvchi ham bir xil ochadi), jimlikda yopiq
				V.KonvertMaks = FMath::Max(V.KonvertMaks * FMath::Exp(-Dt * 0.5f), V.Konvert);
				const float N = V.Konvert / FMath::Max(V.KonvertMaks, 0.02f);
				Maqsad = FMath::Clamp((N - 0.12f) / 0.7f, 0.f, 1.f);
			}
			else
			{
				// konvert yo'q (masalan, procedural tovushda hisoblanmasa) - bo'g'in ritmi
				V.Faza += Dt * (9.f + 4.f * FMath::PerlinNoise1D(Now * 0.7f + Npc->GetUniqueID()));
				Maqsad = 0.2f + 0.6f * FMath::Abs(FMath::Sin(V.Faza));
			}
		}
		if (OgizMajburiy >= 0.f && Npc == SinovSotuvchi.Get()) Maqsad = OgizMajburiy;
		// ochilish tez, yopilish sekinroq - lab qimirlashi tabiiy ko'rinadi
		V.Ogiz = OgizMajburiy >= 0.f && Npc == SinovSotuvchi.Get() ? Maqsad : FMath::FInterpTo(V.Ogiz, Maqsad, Dt, Maqsad > V.Ogiz ? 28.f : 14.f);
		if (V.Ogiz < 0.002f && Maqsad == 0.f) V.Ogiz = 0.f;
		for (const TWeakObjectPtr<USkeletalMeshComponent>& B : V.Boshlar)
			if (B.IsValid()) B->SetMorphTarget(UBozorOgizMorph::MorphNomi, V.Ogiz * OgizMaks);
		static const bool bLog = FParse::Param(FCommandLine::Get(), TEXT("OgizLog"));
		static double KeyingiLog = 0.0;
		if (bLog && V.Ogiz > 0.05f && Now > KeyingiLog)
		{
			KeyingiLog = Now + 0.5;
			UE_LOG(LogBozorOvoz, Display, TEXT("[Ogiz] %s og'iz=%.2f %s, %d bosh"), *Npc->GetActorNameOrLabel(), V.Ogiz,
				V.Src->IsPlaying() ? TEXT("ovoz") : TEXT("jim"), V.Boshlar.Num());
		}
	}
}

void ABozorSotuvchiOvozi::SinovKameraJoyla(float Masofa)
{
	ABozorNpc* V = SinovSotuvchi.Get();
	ACameraActor* Cam = SinovKamera.Get();
	if (!V || !Cam) return;
	USkeletalMeshComponent* Bosh = nullptr;
	TArray<USkeletalMeshComponent*> Q;
	V->GetComponents(Q);
	for (USkeletalMeshComponent* C : Q)
		if (C->GetSkeletalMeshAsset() && C->GetSkeletalMeshAsset()->FindMorphTarget(UBozorOgizMorph::MorphNomi)) Bosh = C;
	if (!Bosh) return;
	// og'iz nuqtasi (mesh fazosi: old ~10 sm, og'iz ~ erkak 164 / ayol 157) "head" suyagi bilan hozirgi pozaga ko'chiriladi:
	// dunyo = nuqta * suyak(ref)^-1 * suyak(hozir)
	const FReferenceSkeleton& Ref = Bosh->GetSkeletalMeshAsset()->GetRefSkeleton();
	const int32 Bone = Ref.FindBoneIndex(TEXT("head"));
	FTransform RefCS = FTransform::Identity;
	for (int32 B = Bone; B != INDEX_NONE; B = Ref.GetParentIndex(B)) RefCS = RefCS * Ref.GetRefBonePose()[B];
	const FTransform Kochir = RefCS.Inverse() * Bosh->GetBoneTransform(Bone);
	const FVector Yuz = Kochir.TransformPosition(FVector(0.f, 10.f, V->bFemale ? 157.f : 164.f));
	const FVector Old = Kochir.TransformVector(FVector(0.f, 1.f, 0.f)).GetSafeNormal();
	const FVector Ong = Kochir.TransformVector(FVector(1.f, 0.f, 0.f)).GetSafeNormal();
	// o'yinchi ko'zi balandligidan (bola - sotuvchidan pastroq) va sal yondan
	const FVector Joy = Yuz + Old * Masofa + Ong * Masofa * 0.25f - FVector(0.f, 0.f, Masofa * 0.12f);
	Cam->SetActorLocationAndRotation(Joy, (Yuz - Joy).Rotation());
}

void ABozorSotuvchiOvozi::OgizSinov(int32 Qadam)
{
	const FString Dir = FPaths::ProjectSavedDir() / TEXT("Screenshots/OgizSinov/");
	auto Keyin = [this](int32 Q, float S)
	{
		FTimerHandle H;
		GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateUObject(this, &ABozorSotuvchiOvozi::OgizSinov, Q), S, false);
	};
	ABozorNpc* V = SinovSotuvchi.Get();
	if (Qadam == 0)
	{
		// o'yinchiga eng yaqin, og'iz morph'i bor sotuvchi
		FVector L;
		ListenerLocation(L);
		float Eng = TNumericLimits<float>::Max();
		for (TActorIterator<ABozorNpc> It(GetWorld()); It; ++It)
		{
			if (It->NpcRole != EBozorRole::Sotuvchi) continue;
			TArray<USkeletalMeshComponent*> Q;
			It->GetComponents(Q);
			const bool bBor = Q.ContainsByPredicate([](USkeletalMeshComponent* C) { return C->GetSkeletalMeshAsset() && C->GetSkeletalMeshAsset()->FindMorphTarget(UBozorOgizMorph::MorphNomi); });
			const float D = FVector::DistSquared(It->GetActorLocation(), L);
			if (bBor && D < Eng) { Eng = D; V = *It; }
		}
		if (!V)
		{
			UE_LOG(LogBozorOvoz, Display, TEXT("[OgizSinov] og'iz morph'i bor sotuvchi topilmadi"));
			UKismetSystemLibrary::QuitGame(this, nullptr, EQuitPreference::Quit, false);
			return;
		}
		SinovSotuvchi = V;
		Get(V);
		if (APlayerController* Pc = UGameplayStatics::GetPlayerController(this, 0))
			if (ACameraActor* Cam = GetWorld()->SpawnActor<ACameraActor>())
			{
				Cam->GetCameraComponent()->SetFieldOfView(40.f);
				Pc->SetViewTarget(Cam);
				SinovKamera = Cam;
			}
		SinovKameraJoyla(120.f);
		OgizMajburiy = 0.f;
		Keyin(1, 2.5f);
		return;
	}
	if (!V) return;
	// har surat oldidan kamera boshga qayta to'g'rilanadi (sotuvchi animatsiyada qimirlaydi); keyingi qadam 0.25 s keyin suratga oladi
	if (Qadam == 1) { SinovKameraJoyla(120.f); Keyin(2, 0.25f); return; }
	if (Qadam == 2) { FScreenshotRequest::RequestScreenshot(Dir + TEXT("1_yopiq_120sm.png"), false, false); OgizMajburiy = 1.f; Keyin(30, 0.5f); return; }
	if (Qadam == 30) { SinovKameraJoyla(120.f); Keyin(31, 0.25f); return; }
	if (Qadam == 31) { FScreenshotRequest::RequestScreenshot(Dir + TEXT("2_ochiq_120sm.png"), false, false); Keyin(32, 0.3f); return; }
	if (Qadam == 32) { SinovKameraJoyla(55.f); Keyin(33, 0.25f); return; }
	if (Qadam == 33) { FScreenshotRequest::RequestScreenshot(Dir + TEXT("3_ochiq_55sm.png"), false, false); OgizMajburiy = 0.f; Keyin(34, 0.3f); return; }
	if (Qadam == 34) { SinovKameraJoyla(55.f); Keyin(35, 0.25f); return; }
	if (Qadam == 35) { FScreenshotRequest::RequestScreenshot(Dir + TEXT("3_yopiq_55sm.png"), false, false); Keyin(3, 0.3f); return; }
	if (Qadam == 3)
	{
		SinovKameraJoyla(120.f);
		OgizMajburiy = -1.f;
		// haqiqiy gap: chaqiriq klipi - og'iz konvert bilan qimirlashi kerak
		FVoice& Ov = Get(V);
		for (const TPair<FString, TArray<USoundBase*>>& K : ByKey)
			if (K.Key.StartsWith(TEXT("chaqiriq")) && K.Value.Num())
			{
				Ov.Src->Stop();
				Ov.Src->SetSound(K.Value[0]);
				Ov.Src->Play();
				break;
			}
		Keyin(4, 0.1f);
		return;
	}
	if (Qadam >= 4 && Qadam < 4 + 24)
	{
		const FVoice& Ov = Get(V);
		if (Qadam == 11) SinovKameraJoyla(120.f);
		UE_LOG(LogBozorOvoz, Display, TEXT("[OgizSinov] t=%.1f chalinyapti=%d konvert=%.3f (yangi=%d) og'iz=%.2f"), (Qadam - 4) * 0.1f,
			Ov.Src->IsPlaying() ? 1 : 0, Ov.Konvert, GetWorld()->GetTimeSeconds() - Ov.KonvertVaqt < 0.25 ? 1 : 0, Ov.Ogiz);
		if (Qadam == 12) FScreenshotRequest::RequestScreenshot(Dir + TEXT("4_gapirmoqda.png"), false, false);
		Keyin(Qadam + 1, 0.1f);
		return;
	}
	UKismetSystemLibrary::QuitGame(this, nullptr, EQuitPreference::Quit, false);
}

// sot_<tur>_<bolim>_<persona>_<nn> -> <tur>_<bolim>_<a|e>_<nn>: bitta haqiqiy yozuv ayol (a1..a3) yoki erkak (e1..e2)
// personalarining hammasiga bir xil raqam bilan nusxalangan (bozor_yozuv_joylash.py) - ular bitta manba
FString ABozorSotuvchiOvozi::SourceKey(const USoundBase* Clip)
{
	TArray<FString> P;
	Clip->GetName().ParseIntoArray(P, TEXT("_"));
	return P.Num() >= 5 ? P[1] + TEXT("_") + P[2] + TEXT("_") + P[3].Left(1) + TEXT("_") + P[4] : Clip->GetName();
}

USoundBase* ABozorSotuvchiOvozi::Pick(const FVoice& V, FName Kind, double Now) const
{
	const FString K = Kind.ToString();
	// yaqinda (RepeatWindow) bozorda aytilmagan gaplar
	auto Fresh = [&](const TArray<USoundBase*>* List)
	{
		TArray<USoundBase*> Out;
		if (!List) return Out;
		for (USoundBase* C : *List)
		{
			const double* T = Heard.Find(SourceKey(C));
			if (!T || Now - *T >= RepeatWindow) Out.Add(C);
		}
		return Out;
	};
	TArray<USoundBase*> Cand;
	// chaqiriq: ko'pincha o'z mahsulotini aytadi, ba'zan umumiy "keling, keling"
	if (Kind == Chaqiriq && V.Section != TEXT("umumiy") && FMath::FRand() < 0.65f)
	{
		Cand = Fresh(ByKey.Find(K + TEXT("_") + V.Section + TEXT("_") + V.Persona));
	}
	if (Cand.Num() == 0) Cand = Fresh(ByKey.Find(K + TEXT("_umumiy_") + V.Persona));
	return Cand.Num() > 0 ? Cand[FMath::RandRange(0, Cand.Num() - 1)] : nullptr;
}

// Sotuvchi turgan joydan bo'lim (Unity BozorBuilder.BuildScene tartibi, Unity metrlarida: x = Y/100, z = X/100):
// yopiq segmentlar z 3..15 va 18..30, ichki qator markaziy yo'lak tomonda (|x| < 6), tashqi qator yon yo'lak tomonda, 4 m modullar;
// z > 31.5 ochiq maydon; x > 8, z > 33.8 go'sht-sut pavilyoni; |x| > 10.5 kiosklar
FString ABozorSotuvchiOvozi::Bolim(const FVector& Location)
{
	const float X = Location.Y / 100.f, Ax = FMath::Abs(X), Z = Location.X / 100.f;
	if (X > 8.f && Z > 33.8f) return Z > 42.5f && X < 13.f ? TEXT("sut") : TEXT("gosht");
	if (Z >= 31.5f)
	{
		if (Ax < 5.5f) return X < 0 ? TEXT("guruch") : TEXT("meva");   // yo'lak chetidagi qoplar | meva yashiklari
		if (X > 0) return TEXT("meva");
		return X < -11.f ? TEXT("sabzavot") : TEXT("meva");            // fermerlar soyabonlari
	}
	if (Ax > 10.5f) return TEXT("sabzavot");
	const bool bFront = Z < 16.5f;
	const int32 M = FMath::Clamp(FMath::FloorToInt((Z - (bFront ? 3.f : 18.f)) / 4.f), 0, 2);
	const bool bInner = Ax < 6.f;
	if (bFront) return bInner ? TEXT("sabzavot") : M == 0 ? TEXT("non") : M == 1 ? TEXT("kokat") : TEXT("meva");
	return bInner ? (M < 2 ? TEXT("ziravor") : TEXT("quruq")) : M == 0 ? TEXT("guruch") : M == 1 ? TEXT("atlas") : TEXT("meva");
}

// ============================================================ rasta ovozlari

TWeakObjectPtr<ABozorRastaOvozlari> ABozorRastaOvozlari::Instance;

ABozorRastaOvozlari::ABozorRastaOvozlari()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ABozorRastaOvozlari::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer)
	{
		SetActorTickEnabled(false);
		return;
	}
	Instance = this;
	for (USoundBase* C : Clips)
	{
		// sfx_<tur>_<nn>: tur o'zi ham "_" li bo'lishi mumkin (tarozi_pip)
		FString Name = C ? C->GetName() : FString();
		int32 Last;
		if (!Name.StartsWith(TEXT("sfx_")) || !Name.FindLastChar(TEXT('_'), Last) || Last <= 4) continue;
		ByKind.FindOrAdd(FName(*Name.Mid(4, Last - 4))).Add(C);
	}
	if (ByKind.Num() == 0)
	{
		UE_LOG(LogBozorOvoz, Warning, TEXT("[BozorOvoz] rasta SFX kliplari yo'q - o'chirildi"));
		SetActorTickEnabled(false);
		return;
	}
	// hamma nuqta birdaniga chalinmasin
	const double Now = GetWorld()->GetTimeSeconds();
	for (FBozorRastaNuqta& P : Points) P.NextTime = Now + FMath::FRandRange(1.f, P.Gap.Y * GapScale);
	UE_LOG(LogBozorOvoz, Display, TEXT("[BozorOvoz] rasta SFX: klip=%d tur=%d nuqta=%d"), Clips.Num(), ByKind.Num(), Points.Num());
}

void ABozorRastaOvozlari::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Instance.Get() == this) Instance = nullptr;
	Super::EndPlay(Reason);
}

void ABozorRastaOvozlari::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now < NextScan) return;
	NextScan = Now + 0.2;
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC) return;
	FVector L, Front, Right;
	PC->GetAudioListenerPosition(L, Front, Right);
	for (FBozorRastaNuqta& P : Points)
	{
		if (Now < P.NextTime || P.Kinds.Num() == 0) continue;
		P.NextTime = Now + FMath::FRandRange(P.Gap.X, P.Gap.Y) * GapScale;
		if (FVector::DistSquared(P.Location, L) > ActiveRadius * ActiveRadius) continue;
		PlayKind(P.Kinds[FMath::RandRange(0, P.Kinds.Num() - 1)], P.Location, nullptr);
	}
}

void ABozorRastaOvozlari::Play(const UObject* WorldContext, FName Kind, const FVector& Location, AActor* Source)
{
	if (ABozorRastaOvozlari* R = Instance.Get(); R && R->IsActorTickEnabled()) R->PlayKind(Kind, Location, Source);
}

void ABozorRastaOvozlari::PlayNear(const UObject* WorldContext, const TArray<FName>& Kinds, const FVector& Location, float Radius, FName Fallback, AActor* Source)
{
	ABozorRastaOvozlari* R = Instance.Get();
	if (!R || !R->IsActorTickEnabled()) return;
	const FBozorRastaNuqta* Best = nullptr;
	FName BestKind;
	float BestD = Radius * Radius;
	for (const FBozorRastaNuqta& P : R->Points)
	{
		const float D = FVector::DistSquared(P.Location, Location);
		if (D >= BestD) continue;
		for (const FName& K : Kinds)
		{
			if (P.Kinds.Contains(K)) { Best = &P; BestKind = K; BestD = D; break; }
		}
	}
	if (Best) R->PlayKind(BestKind, Best->Location, Source);
	else R->PlayKind(Fallback, Location, Source);
}

void ABozorRastaOvozlari::PlayKind(FName Kind, const FVector& Location, AActor* Source)
{
	const TArray<USoundBase*>* List = ByKind.Find(Kind);
	if (!List || List->Num() == 0) return;
	// ketma-ket bir xil variant chiqmasin; balandlik va pitch har safar biroz boshqa
	int32 I = FMath::RandRange(0, List->Num() - 1);
	USoundBase*& Last = LastClip.FindOrAdd(Kind);
	if ((*List)[I] == Last && List->Num() > 1) I = (I + 1) % List->Num();
	Last = (*List)[I];
	UGameplayStatics::PlaySoundAtLocation(this, Last, Location, FRotator::ZeroRotator,
		Volume * FMath::FRandRange(0.75f, 1.f), FMath::FRandRange(0.94f, 1.06f), 0.f, nullptr, nullptr, Source);
	UE_LOG(LogBozorOvoz, Verbose, TEXT("[BozorOvoz] rasta: %s (%s)"), *Last->GetName(), Source ? *Source->GetActorNameOrLabel() : TEXT("nuqta"));
}

// ============================================================ o'yinchi qadamlari

ABozorQadamOvozlari::ABozorQadamOvozlari()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;   // suyaklar shu kadrda yangilangandan keyin o'qiladi
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ABozorQadamOvozlari::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer)
	{
		SetActorTickEnabled(false);
		return;
	}
	for (USoundBase* C : Clips)
	{
		// qadam_<sirt>_<nn>
		FString Name = C ? C->GetName() : FString();
		int32 Last;
		if (!Name.StartsWith(TEXT("qadam_")) || !Name.FindLastChar(TEXT('_'), Last) || Last <= 6) continue;
		ByKind.FindOrAdd(FName(*Name.Mid(6, Last - 6))).Add(C);
	}
	if (ByKind.Num() == 0)
	{
		UE_LOG(LogBozorOvoz, Warning, TEXT("[BozorOvoz] qadam kliplari yo'q - o'chirildi"));
		SetActorTickEnabled(false);
		return;
	}
	Feet.SetNum(FootBones.Num());
	bTest = FParse::Param(FCommandLine::Get(), TEXT("BozorQadamTest"));
	TestStart = GetWorld()->GetTimeSeconds();
	UE_LOG(LogBozorOvoz, Display, TEXT("[BozorOvoz] qadamlar: klip=%d sirt=%d"), Clips.Num(), ByKind.Num());
}

void ABozorQadamOvozlari::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ACharacter* C = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	UCharacterMovementComponent* Move = C ? C->GetCharacterMovement() : nullptr;
	if (!Move) return;
	if (bTest) UpdateTest(C, GetWorld()->GetTimeSeconds());

	const float Half = C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Base = C->GetActorLocation() - FVector(0.f, 0.f, Half);
	const bool bGround = Move->IsMovingOnGround();
	const float Speed = C->GetVelocity().Size2D();
	USkeletalMeshComponent* Mesh = C->GetMesh();
	// sakrab tushdi: ikki oyoq birga, balandroq
	if (bGround && bWasFalling)
	{
		Step(C, Base, RunSpeed, 1.2f);
		for (FFoot& F : Feet) F.Peak = F.Prev;   // tushgan oyoqlar yana ko'tarilgunicha qadam yo'q
	}
	bWasFalling = Move->IsFalling();
	if (!bGround || Speed < MinSpeed) return;

	// to'liq yurish tezligiga nisbatan ulush. BS_Idle_Walk_Run'da yurish namunasi 300 sm/s da, o'yinchi esa ko'pi bilan 220:
	// sekin yurganda blendspace'da idle ulushi katta - oyoq atigi ~3 sm ko'tariladi (0.35 kirishda qat'iy MinLift=5 bilan qadam umuman yo'q edi)
	const float Frac = FMath::Clamp(Speed / FMath::Max(Move->MaxWalkSpeed, 1.f), 0.3f, 1.f);
	Stride += Speed * DeltaSeconds;   // oxirgi qadamdan beri yurilgan masofa, Step() nolga qaytaradi

	bool bBones = Mesh != nullptr;
	for (const FName& B : FootBones) bBones &= Mesh && Mesh->GetBoneIndex(B) != INDEX_NONE;
	if (!bBones)
	{
		if (Stride >= StrideLength) Step(C, Base, Speed, 1.f);
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	const float Lift = MinLift * Frac;
	// tushish tezligi chegarasi kadr chastotasiga bog'liq emas (avval kadrga 0.2 sm edi - 40 fps da 8 sm/s, 120 fps da sezilmay qolardi)
	const float Down = 8.f * Frac * DeltaSeconds;
	// masofa bo'yicha zaxira qadam oralig'i: tezlikka qarab qisqaradi (220 sm/s da ~112 sm - suyak qadamlari ~82 sm da chiqadi,
	// zaxira ishlamaydi; 77 sm/s da ~49 sm, ~0.63 s). Suyak qadami esa uning yarmidan oldin chiqmaydi - ikkalasi aralashsa ritm buzilmasin
	const float Gap = 1.5f * StrideLength * FMath::Pow(Frac, 0.8f);
	for (int32 i = 0; i < FootBones.Num(); ++i)
	{
		FFoot& F = Feet[i];
		const FVector Foot = Mesh->GetBoneLocation(FootBones[i]);
		const float Z = Foot.Z - Base.Z;
		FVector2D& R = Speed < Move->MaxWalkSpeed * 0.6f ? F.Walk : F.Run;   // test logi: sekin / to'liq tezlik
		R.X = FMath::Min(R.X, Z);
		R.Y = FMath::Max(R.Y, Z);
		F.Peak = FMath::Max(F.Peak, Z);
		if (Z < F.Prev - Down)
		{
			F.bDown = true;
		}
		// pastga tushish tugadi (oyoq to'xtadi yoki ko'tarila boshladi): eng past nuqta - F.Prev
		else if (F.bDown && Z > F.Prev - Down * 0.25f)
		{
			F.bDown = false;
			if (F.Prev < PlantHeight && F.Peak - F.Prev >= Lift && Now - F.LastStep >= MinInterval && Stride >= 0.5f * Gap)
			{
				F.Peak = F.Prev;
				F.LastStep = Now;
				Step(C, FVector(Foot.X, Foot.Y, Base.Z), Speed, 1.f);
			}
		}
		F.Prev = Z;
	}
	// suyakdan qadam chiqmay qoldi (juda sekin, oyoq deyarli ko'tarilmaydi) - jim yurmasin: masofa bo'yicha,
	// oxirgi qadam bosgan oyoqning teskarisidan (chap-o'ng navbati saqlansin)
	// (deyarli to'xtab qolganda - bir joyda turib burilish, devorga tiralib qolish - zaxira qadam yo'q)
	if (Stride >= Gap && Speed >= 40.f)
	{
		int32 Next = 0;
		for (int32 i = 1; i < Feet.Num(); ++i) if (Feet[i].LastStep < Feet[Next].LastStep) Next = i;
		FFoot& F = Feet[Next];
		const FVector Foot = Mesh->GetBoneLocation(FootBones[Next]);
		F.Peak = F.Prev;
		F.LastStep = Now;
		Step(C, FVector(Foot.X, Foot.Y, Base.Z), Speed, 1.f);
	}
}

FName ABozorQadamOvozlari::SurfaceAt(ACharacter* C, const FVector& Foot, FVector& OutPoint) const
{
	FCollisionQueryParams Q(SCENE_QUERY_STAT(BozorQadam), /*bTraceComplex=*/true, C);
	Q.bReturnPhysicalMaterial = true;
	FHitResult Hit;
	OutPoint = Foot;
	if (!GetWorld()->LineTraceSingleByChannel(Hit, Foot + FVector(0.f, 0.f, 40.f), Foot - FVector(0.f, 0.f, 60.f), ECC_Visibility, Q)) return FallbackSurface;
	OutPoint = Hit.ImpactPoint;
	const UPhysicalMaterial* PM = Hit.PhysMaterial.Get();
	if (!PM || PM->SurfaceType == SurfaceType_Default) return FallbackSurface;
	for (const FPhysicalSurfaceName& S : UPhysicsSettings::Get()->PhysicalSurfaces)
	{
		if (S.Type == PM->SurfaceType) return FName(*S.Name.ToString().ToLower());
	}
	return FallbackSurface;
}

void ABozorQadamOvozlari::Step(ACharacter* C, const FVector& Foot, float Speed, float Gain)
{
	Stride = 0.f;
	FVector Point;
	FName Surface = SurfaceAt(C, Foot, Point);
	// ikki sirt chegarasida oyoqlar har xil sirtda bo'lsa (chap - asfalt, o'ng - beton) har qadamda boshqa ovoz chiqardi:
	// gavda ostidagi sirt ustun - chegaradan o'tib bo'lgach qadamlar yangi sirtga o'tadi
	{
		FVector Markaz;
		const FName Gavda = SurfaceAt(C, FVector(C->GetActorLocation().X, C->GetActorLocation().Y, Foot.Z), Markaz);
		if (Gavda != Surface && ByKind.Contains(Gavda)) Surface = Gavda;
	}
	const TArray<USoundBase*>* List = ByKind.Find(Surface);
	if (!List || List->Num() == 0)
	{
		Surface = FallbackSurface;
		List = ByKind.Find(Surface);
	}
	if (!List || List->Num() == 0) return;
	// oxirgi ikki variant qaytarilmaydi (A-B-A-B naqshi ham sezilmasin), qolganlardan teng ehtimol bilan.
	// Oldingisi shu xaritada sirt nomining 1-raqamli nusxasi kaliti bilan saqlanadi (beton -> beton_0)
	const FName PrevKey(Surface, 1);
	LastClip.FindOrAdd(Surface);
	LastClip.FindOrAdd(PrevKey);
	USoundBase*& Last = LastClip[Surface];
	USoundBase*& Prev = LastClip[PrevKey];
	TArray<USoundBase*, TInlineAllocator<8>> Pool;
	for (USoundBase* S : *List) if (S != Last && S != Prev) Pool.Add(S);
	if (Pool.IsEmpty()) for (USoundBase* S : *List) if (S != Last) Pool.Add(S);
	if (Pool.IsEmpty()) Pool.Add((*List)[0]);
	USoundBase* Clip = Pool[FMath::RandRange(0, Pool.Num() - 1)];
	Prev = Last;
	Last = Clip;

	// sekin yurganda ~60%, o'yinchining to'liq yurish tezligida 100% (MaxWalkSpeed 220 - RunSpeed'ga hech yetmaydi)
	const UCharacterMovementComponent* Move = C->GetCharacterMovement();
	const float Full = FMath::Max(Move ? Move->MaxWalkSpeed : RunSpeed, 1.f);
	const float Tez = FMath::Clamp(Speed / Full, 0.f, 1.f);
	const float Loud = FMath::Lerp(0.6f, 1.f, Tez);
	const float Vol = Volume * CVarQadamBalandligi.GetValueOnGameThread() * Loud * Gain * FMath::FRandRange(0.9f, 1.1f);   // har qadam ±10%
	// sekin yurganda yumshoqroq va pastroq, yugurganda o'tkirroq (zarb kuchli - ohang biroz baland)
	const float Pitch = FMath::FRandRange(0.94f, 1.04f) + 0.05f * Tez;

	// PlaySoundAtLocation o'rniga (u ichida shuni qiladi): filtrlar Play'dan oldin qo'yilsin - birinchi zarb ham filtrlangan bo'lsin
	FAudioDevice::FCreateComponentParams Params(GetWorld(), C);
	Params.SetLocation(Point);
	if (UAudioComponent* A = FAudioDevice::CreateComponent(Clip, Params))
	{
		A->SetWorldLocation(Point);
		A->SetVolumeMultiplier(Vol);
		A->SetPitchMultiplier(Pitch);
		A->bAllowSpatialization = Params.ShouldUseAttenuation();
		A->bAutoDestroy = true;
		A->bStopWhenOwnerDestroyed = false;
		// bola yengil, krossovkada: kliplardagi og'ir 80-95 Hz "dup" kesiladi, yuqori shitir biroz yumshaydi;
		// chastotalar har qadamda ozgina boshqacha - bir xil sintez rangi takrorlanmasin
		A->SetHighPassFilterFrequency(FMath::FRandRange(150.f, 210.f));
		A->SetHighPassFilterEnabled(true);
		A->SetLowPassFilterFrequency(FMath::FRandRange(5000.f, 8000.f) + 4000.f * Tez);
		A->SetLowPassFilterEnabled(true);
		A->Play();
	}
	if (bTest) TestSteps.FindOrAdd(Surface)++;
	UE_LOG(LogBozorOvoz, Verbose, TEXT("[BozorOvoz] qadam: %s klip=%s tezlik=%.0f vol=%.2f pitch=%.2f joy=(%.0f, %.0f, %.0f)"),
		*Surface.ToString(), *Clip->GetName(), Speed, Vol, Pitch, Point.X, Point.Y, Point.Z);
}

// Sinov: o'yinchi kirishdan bozor ichiga qarab yuradi (0-12 s sekin, 12-25 s yugurib), keyin natija loglanadi va o'yin yopiladi
void ABozorQadamOvozlari::UpdateTest(ACharacter* C, double Now)
{
	const double T = Now - TestStart;
	if (T < 3.0) return;                       // xarita yuklanib, qahramon yerga tushsin
	// umumiy ovoz chiqishi yoziladi (Saved/Ovoz/qadam_ovoz.wav): qadamlar fon ustidan qanchalik eshitilishini o'lchash uchun
	static bool bYozuv = false;
	if (!bYozuv)
	{
		bYozuv = true;
		UAudioMixerBlueprintLibrary::StartRecordingOutput(this, 0.f);
		UE_LOG(LogBozorOvoz, Display, TEXT("[OvozYozuv] boshlandi"));
	}
	if (T < 28.0)
	{
		C->AddMovementInput(FVector::ForwardVector, T < 15.0 ? 0.35f : 1.f);
		return;
	}
	static bool bYozuvTugadi = false;
	if (!bYozuvTugadi)
	{
		bYozuvTugadi = true;
		UAudioMixerBlueprintLibrary::StopRecordingOutput(this, EAudioRecordingExportType::WavFile, TEXT("qadam_ovoz"),
			FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Ovoz")));
		UE_LOG(LogBozorOvoz, Display, TEXT("[OvozYozuv] tugadi"));
	}
	if (T < 29.5) return;   // fayl yozilib bo'lsin
	FString S;
	for (const TPair<FName, int32>& P : TestSteps) S += FString::Printf(TEXT(" %s=%d"), *P.Key.ToString(), P.Value);
	FString R;
	for (int32 i = 0; i < Feet.Num(); ++i)
	{
		R += FString::Printf(TEXT(" %s sekin=[%.1f..%.1f] tez=[%.1f..%.1f]"), *FootBones[i].ToString(),
			Feet[i].Walk.X, Feet[i].Walk.Y, Feet[i].Run.X, Feet[i].Run.Y);
	}
	UE_LOG(LogBozorOvoz, Display, TEXT("[BozorOvoz] qadam testi:%s | suyak balandligi:%s | yakun joy=(%.0f, %.0f)"), *S, *R, C->GetActorLocation().X, C->GetActorLocation().Y);
	bTest = false;
	UKismetSystemLibrary::QuitGame(this, nullptr, EQuitPreference::Quit, false);
}
