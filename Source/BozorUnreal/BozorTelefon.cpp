#include "BozorTelefon.h"

#include "BozorNavoiy.h"
#include "BozorSotuv.h"
#include "BozorSozlamalar.h"
#include "CanvasItem.h"
#include "Components/AudioComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundWave.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogBozorTelefon, Log, All);

namespace
{
	const float JiringOraligi = 1.7f;     // qo'ng'iroq tovushi takrorlanishi, s
	const float OziUlanadi = 15.f;        // javob berilmasa shuncha s dan keyin o'zi ulanadi
	const float NavoiyKutish = 4.f;       // ona gapi uchun ovoz shuncha s ichida kelmasa - faqat matn

	// 2D tovush, berilgan ovoz sinfida (sozlamalardagi balandlik va SC_Ovoz'ning fon ducking'i ishlasin).
	// bAutoDestroy=false - qaytgan komponentni ushlab turish xavfsiz; to'xtatganda Yoqot() bilan yo'q qilinadi
	UAudioComponent* Chal(UObject* Dunyo, USoundBase* Tovush, USoundClass* Sinf, float Balandlik, bool bUshlab = false)
	{
		if (!Dunyo || !Tovush) return nullptr;
		UAudioComponent* A = UGameplayStatics::CreateSound2D(Dunyo, Tovush, Balandlik, 1.f, 0.f, nullptr, false, !bUshlab);
		if (!A) return nullptr;
		if (Sinf) A->SoundClassOverride = Sinf;
		A->Play();
		return A;
	}

	void Yoqot(TObjectPtr<UAudioComponent>& A)
	{
		if (IsValid(A))
		{
			A->Stop();
			A->DestroyComponent();
		}
		A = nullptr;
	}

	// burchaklari yumaloq to'rtburchak (Canvas'da yo'q: 3 ta to'rtburchak + 4 ta doira). Doira (NGon) shaffoflikni
	// qo'llamaydi - shaffof rangda burchaklar to'g'ri qoladi
	void Yumaloq(UCanvas* C, float X, float Y, float W, float H, float R, const FLinearColor& Rang)
	{
		if (Rang.A < 0.99f) R = 0.f;
		R = FMath::Min(R, FMath::Min(W, H) * 0.5f);
		if (R <= 0.f)
		{
			FCanvasTileItem T(FVector2D(X, Y), FVector2D(W, H), Rang);
			T.BlendMode = SE_BLEND_Translucent;
			C->DrawItem(T);
			return;
		}
		auto Tort = [C, &Rang](float X0, float Y0, float W0, float H0)
		{
			FCanvasTileItem T(FVector2D(X0, Y0), FVector2D(W0, H0), Rang);
			T.BlendMode = SE_BLEND_Translucent;
			C->DrawItem(T);
		};
		Tort(X + R, Y, W - 2.f * R, H);
		Tort(X, Y + R, R, H - 2.f * R);
		Tort(X + W - R, Y + R, R, H - 2.f * R);
		const FVector2D Burchak[] = { {X + R, Y + R}, {X + W - R, Y + R}, {X + R, Y + H - R}, {X + W - R, Y + H - R} };
		for (const FVector2D& B : Burchak)
		{
			FCanvasNGonItem N(B, FVector2D(R, R), 24, Rang);
			C->DrawItem(N);
		}
	}

	void Doira(UCanvas* C, const FVector2D& M, float R, const FLinearColor& Rang)
	{
		FCanvasNGonItem N(M, FVector2D(R, R), 40, Rang);
		C->DrawItem(N);
	}

	void Rasmcha(UCanvas* C, UTexture2D* T, const FVector2D& M, float R, const FLinearColor& Rang = FLinearColor::White)
	{
		if (!T || !T->GetResource()) return;
		FCanvasTileItem I(M - FVector2D(R, R), T->GetResource(), FVector2D(2.f * R, 2.f * R), Rang);
		I.BlendMode = SE_BLEND_Translucent;
		C->DrawItem(I);
	}

	void Matn(UCanvas* C, const FString& M, float X, float Y, float K, const FLinearColor& Rang, bool bMarkaz = true, bool bSoya = false)
	{
		FCanvasTextItem T(FVector2D(X, Y), FText::FromString(M), GEngine->GetLargeFont(), Rang);
		T.Scale = FVector2D(K);
		T.bCentreX = bMarkaz;
		if (bSoya) T.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.7f * Rang.A));
		C->DrawItem(T);
	}

	// matnni berilgan kenglikka so'zma-so'z bo'ladi
	TArray<FString> Bolib(UCanvas* C, const FString& M, float Kenglik, float K)
	{
		TArray<FString> Sozlar, Qatorlar;
		M.ParseIntoArrayWS(Sozlar);
		FString Q;
		for (const FString& S : Sozlar)
		{
			const FString Sinov = Q.IsEmpty() ? S : Q + TEXT(" ") + S;
			float W = 0.f, H = 0.f;
			C->TextSize(GEngine->GetLargeFont(), Sinov, W, H, K, K);
			if (W > Kenglik && !Q.IsEmpty())
			{
				Qatorlar.Add(Q);
				Q = S;
			}
			else Q = Sinov;
		}
		if (!Q.IsEmpty()) Qatorlar.Add(Q);
		return Qatorlar;
	}
}

UBozorTelefon::UBozorTelefon()
{
	// Advanced Phone System (Content/PhoneSystem) assetlari
	static ConstructorHelpers::FObjectFinder<UTexture2D> TFon(TEXT("/Game/PhoneSystem/Texture/Other/T_PhoneBG.T_PhoneBG"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> TCall(TEXT("/Game/PhoneSystem/Texture/Icon/T_Call.T_Call"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> TClose(TEXT("/Game/PhoneSystem/Texture/Icon/T_CloseCall.T_CloseCall"));
	static ConstructorHelpers::FObjectFinder<USoundBase> SJiring(TEXT("/Game/PhoneSystem/Demo/Sound/SW_Notify.SW_Notify"));
	static ConstructorHelpers::FObjectFinder<USoundBase> SUlandi(TEXT("/Game/PhoneSystem/Sound/SW_Beep.SW_Beep"));
	// SW_Rejected - inglizcha avtojavob gapi, shuning uchun go'shak qo'yilganda ham qisqa signal (SW_Beep)
	static ConstructorHelpers::FObjectFinder<USoundBase> SUzildi(TEXT("/Game/PhoneSystem/Sound/SW_Beep.SW_Beep"));
	if (TFon.Succeeded()) Fon = TFon.Object;
	if (TCall.Succeeded()) QongiroqBelgi = TCall.Object;
	if (TClose.Succeeded()) TugatishBelgi = TClose.Object;
	if (SJiring.Succeeded()) Jiring = SJiring.Object;
	if (SUlandi.Succeeded()) Ulandi = SUlandi.Object;
	if (SUzildi.Succeeded()) Uzildi = SUzildi.Object;
	// loyiha ovoz sinflari (Scripts/import_audio.py): SC_UI - telefon tovushlari, SC_Ovoz - gap (bozor foni pasayadi)
	static ConstructorHelpers::FObjectFinder<USoundClass> CUI(TEXT("/Game/Bozor/Audio/Tuzilma/SC_UI.SC_UI"));
	static ConstructorHelpers::FObjectFinder<USoundClass> COvoz(TEXT("/Game/Bozor/Audio/Tuzilma/SC_Ovoz.SC_Ovoz"));
	if (CUI.Succeeded()) SinfUI = CUI.Object;
	if (COvoz.Succeeded()) SinfOvoz = COvoz.Object;
}

void UBozorTelefon::Boshla(APlayerController* P, const FBozorDaraja& D)
{
	Pc = P;
	Kim = D.Qongiroqchi.IsEmpty() ? FString(TEXT("Oyijon")) : D.Qongiroqchi;
	Gaplar = D.QongiroqGaplar;
	bTest = FParse::Param(FCommandLine::Get(), TEXT("QongiroqTest"));
	Vaqt = P && P->GetWorld() ? P->GetWorld()->GetTimeSeconds() : 0.0;
	Holat = EHolat::Jiringlash;
	HolatVaqt = Vaqt;
	KeyingiJiring = Vaqt + 0.6;   // telefon chiqib ulgursin
	UE_LOG(LogBozorTelefon, Display, TEXT("[Telefon] %s qo'ng'iroq qilyapti (%d ta gap)"), *Kim, Gaplar.Num());
}

void UBozorTelefon::JavobBer()
{
	Holat = EHolat::Gaplashish;
	HolatVaqt = UlanganVaqt = Vaqt;
	Chal(Pc.Get(), Ulandi, SinfUI, 0.6f);
	UE_LOG(LogBozorTelefon, Display, TEXT("[Telefon] javob berildi"));
	GapBoshla(0);
}

void UBozorTelefon::GapBoshla(int32 N)
{
	Yoqot(Ovoz);
	OvozKutilyapti = -1;
	Gap = N;
	GapVaqt = Vaqt;
	if (!Gaplar.IsValidIndex(N))
	{
		Tugat();
		return;
	}
	const TPair<bool, FString>& G = Gaplar[N];
	GapDavomi = 1.6f + G.Value.Len() * 0.055f;
	UE_LOG(LogBozorTelefon, Display, TEXT("[Telefon] %s: %s"), G.Key ? *Kim : TEXT("Bola"), *G.Value);

	// qo'ng'iroq qilgan odam gapi - Navoiy ishlayotgan bo'lsa ayol ovozida (mehribon)
	UBozorNavoiy* Navoiy = Pc.IsValid() ? UBozorNavoiy::Get(Pc.Get()) : nullptr;
	if (G.Key && Navoiy && Navoiy->Tayyormi() && !bTest)
	{
		OvozKutilyapti = N;
		TWeakObjectPtr<UBozorTelefon> Men(this);
		Navoiy->Ayt(TEXT("mehribon"), true, G.Value, [Men, N](USoundWave* W)
		{
			UBozorTelefon* T = Men.Get();
			if (!T || T->Gap != N || T->Holat != EHolat::Gaplashish) return;
			T->OvozKutilyapti = -1;
			T->Ovoz = Chal(T->Pc.Get(), W, T->SinfOvoz, 1.f, true);
		});
	}
}

void UBozorTelefon::Tugat()
{
	if (Holat == EHolat::Tugatish || Holat == EHolat::Tugadi) return;
	Yoqot(Ovoz);
	Holat = EHolat::Tugatish;
	HolatVaqt = Vaqt;
	Chal(Pc.Get(), Uzildi, SinfUI, 0.5f);
	UE_LOG(LogBozorTelefon, Display, TEXT("[Telefon] qo'ng'iroq tugadi"));
}

void UBozorTelefon::Tick(float Dt)
{
	APlayerController* P = Pc.Get();
	if (!P || !P->GetWorld()) return;
	Vaqt = P->GetWorld()->GetTimeSeconds();
	const double T = Vaqt - HolatVaqt;
	const bool bTugma = P->WasInputKeyJustPressed(UBozorSozlamalar::Get()->Tugma(TEXT("Gaplashish"))) || P->WasInputKeyJustPressed(EKeys::Enter);
	// -QongiroqTest: jiringlash va suhbat suratlari (Saved/Screenshots/QongiroqTest)
	if (bTest)
	{
		const FString Dir = FPaths::ProjectSavedDir() / TEXT("Screenshots/QongiroqTest/");
		if (Holat == EHolat::Jiringlash && T > 1.2 && T - Dt <= 1.2) FScreenshotRequest::RequestScreenshot(Dir + TEXT("1_jiring.png"), true, false);
		if (Holat == EHolat::Gaplashish && Gap == 2 && Vaqt - GapVaqt > 1.0 && Vaqt - GapVaqt - Dt <= 1.0)
			FScreenshotRequest::RequestScreenshot(Dir + TEXT("2_suhbat.png"), true, false);
	}

	switch (Holat)
	{
	case EHolat::Jiringlash:
		Chiqish = FMath::FInterpConstantTo(Chiqish, 1.f, Dt, 2.5f);
		if (Vaqt >= KeyingiJiring)
		{
			Chal(P, Jiring, SinfUI, 0.9f);
			KeyingiJiring = Vaqt + JiringOraligi;
		}
		if ((bTugma && T > 0.5) || T > OziUlanadi || (bTest && T > 2.0)) JavobBer();
		break;

	case EHolat::Gaplashish:
	{
		const double Gt = Vaqt - GapVaqt;
		const bool bOvozChalinyapti = IsValid(Ovoz) && Ovoz->IsPlaying();
		const bool bOvozKutilyapti = OvozKutilyapti == Gap && Gt < NavoiyKutish;
		// ovoz bo'lsa - u tugaguncha (kamida 1 s), bo'lmasa - matn uzunligiga qarab
		const bool bTayyor = Ovoz ? (!bOvozChalinyapti && Gt > 1.0) : (!bOvozKutilyapti && Gt > GapDavomi);
		if ((bTugma && Gt > 0.4) || (bTayyor && Gt > 0.3)) GapBoshla(Gap + 1);
		break;
	}

	case EHolat::Tugatish:
		if (T > 0.6) Chiqish = FMath::FInterpConstantTo(Chiqish, 0.f, Dt, 2.5f);
		if (Chiqish <= 0.f)
		{
			Holat = EHolat::Tugadi;
			HolatVaqt = Vaqt;
		}
		break;

	case EHolat::Tugadi:
		break;
	}
}

void UBozorTelefon::Chiz(UCanvas* C)
{
	if (!C || Holat == EHolat::Tugadi || Chiqish <= 0.f) return;
	const float S = C->ClipY / 1080.f;
	const float A = 1.f - FMath::Pow(1.f - Chiqish, 3.f);   // ease-out
	const double T = Vaqt - HolatVaqt;

	// telefon: o'ng tomonda, pastdan ko'tariladi; jiringlaganda titraydi
	const float W = 310.f * S, H = 630.f * S;
	float X = C->ClipX - W - 110.f * S;
	float Y = FMath::Lerp(C->ClipY + 40.f * S, (C->ClipY - H) * 0.5f + 30.f * S, A);
	if (Holat == EHolat::Jiringlash && FMath::Fmod(T, JiringOraligi) < 0.8)
	{
		X += FMath::Sin(T * 70.0) * 3.5f * S;
		Y += FMath::Cos(T * 55.0) * 1.5f * S;
	}

	// korpus, ekran (asset foni), kamera teshigi
	Yumaloq(C, X, Y, W, H, 34.f * S, FLinearColor(0.02f, 0.02f, 0.025f, 1.f));
	const float E = 12.f * S;
	const float Ex = X + E, Ey = Y + E, Ew = W - 2.f * E, Eh = H - 2.f * E;
	// T_PhoneBG'ning o'zida qora ramka va yumaloq burchaklar bor (burchaklari shaffof): tagiga to'rtburchak chizilmaydi,
	// aks holda korpus bilan rasm ramkasi orasida havorang burchaklar ko'rinadi. Matn o'qilishi uchun rasm sal xiralanadi
	if (Fon && Fon->GetResource())
	{
		FCanvasTileItem F(FVector2D(Ex, Ey), Fon->GetResource(), FVector2D(Ew, Eh), FLinearColor(0.7f, 0.7f, 0.7f, 1.f));
		F.BlendMode = SE_BLEND_Translucent;
		C->DrawItem(F);
	}
	else Yumaloq(C, Ex, Ey, Ew, Eh, 24.f * S, FLinearColor(0.06f, 0.24f, 0.34f, 1.f));
	Doira(C, FVector2D(X + W * 0.5f, Ey + 16.f * S), 6.f * S, FLinearColor(0.f, 0.f, 0.f, 1.f));

	const float Mx = X + W * 0.5f;
	const FDateTime Hozir = FDateTime::Now();
	Matn(C, FString::Printf(TEXT("%02d:%02d"), Hozir.GetHour(), Hozir.GetMinute()), Ex + 34.f * S, Ey + 8.f * S, 1.0f * S, FLinearColor::White, false);

	// kim qo'ng'iroq qilyapti
	const bool bJiring = Holat == EHolat::Jiringlash;
	Matn(C, bJiring ? TEXT("Kiruvchi qo'ng'iroq") : (Holat == EHolat::Tugatish ? TEXT("Qo'ng'iroq tugadi") : TEXT("Mobil")),
		Mx, Ey + 70.f * S, 1.15f * S, FLinearColor(1.f, 1.f, 1.f, 0.8f));
	const FVector2D Avatar(Mx, Ey + 185.f * S);
	if (bJiring)
	{
		// avatar atrofida to'lqin
		const float Q = FMath::Fmod(static_cast<float>(T), 1.2f) / 1.2f;
		Doira(C, Avatar, (62.f + 40.f * Q) * S, FLinearColor(1.f, 1.f, 1.f, 0.25f * (1.f - Q)));
	}
	// avatar: ismning bosh harfi
	Doira(C, Avatar, 62.f * S, FLinearColor(0.93f, 0.55f, 0.45f, 1.f));
	Matn(C, Kim.Left(1).ToUpper(), Avatar.X, Avatar.Y - 33.f * S, 3.4f * S, FLinearColor::White);
	Matn(C, Kim, Mx, Ey + 268.f * S, 2.2f * S, FLinearColor::White, true, true);
	if (!bJiring)
	{
		const int32 Son = FMath::Max(0, static_cast<int32>(Vaqt - UlanganVaqt));
		Matn(C, FString::Printf(TEXT("%02d:%02d"), Son / 60, Son % 60), Mx, Ey + 322.f * S, 1.4f * S, FLinearColor(1.f, 1.f, 1.f, 0.85f));
	}

	// tugmalar: jiringlaganda - rad (qizil) va javob (yashil, urib turadi); suhbatda - faqat tugatish
	const float By = Ey + Eh - 105.f * S, Br = 34.f * S;
	const FLinearColor Qizil(0.9f, 0.2f, 0.2f, 1.f), Yashil(0.2f, 0.75f, 0.3f, 1.f);
	if (bJiring)
	{
		const FVector2D Rad(Mx - 70.f * S, By), Javob(Mx + 70.f * S, By);
		Doira(C, Rad, Br, Qizil);
		Rasmcha(C, TugatishBelgi, Rad, Br * 0.55f);
		const float Urish = 1.f + 0.08f * FMath::Sin(static_cast<float>(T) * 8.f);
		Doira(C, Javob, Br * Urish, Yashil);
		Rasmcha(C, QongiroqBelgi, Javob, Br * 0.55f * Urish);
		Matn(C, FString::Printf(TEXT("%s — javob"), *UBozorSozlamalar::TugmaNomi(UBozorSozlamalar::Get()->Tugma(TEXT("Gaplashish"))).ToString()),
			Javob.X, By + Br + 10.f * S, 1.0f * S, FLinearColor::White);
	}
	else
	{
		const FVector2D Tug(Mx, By);
		Doira(C, Tug, Br, Qizil);
		Rasmcha(C, TugatishBelgi, Tug, Br * 0.55f);
	}

	// suhbat subtitri: ekran pastida
	if (Holat == EHolat::Gaplashish && Gaplar.IsValidIndex(Gap))
	{
		const TPair<bool, FString>& G = Gaplar[Gap];
		const float K = 1.7f * S;
		const float Kenglik = FMath::Min(1100.f * S, C->ClipX - W - 300.f * S);
		const TArray<FString> Qatorlar = Bolib(C, G.Value, Kenglik, K);
		const float Qh = 40.f * S;
		const float Bh = 92.f * S + Qh * Qatorlar.Num();
		const float Sx = (C->ClipX - W - 140.f * S) * 0.5f;   // telefon chap tomonidagi joy markazi
		const float Sy = C->ClipY - Bh - 70.f * S;
		const float Alfa = FMath::Clamp(static_cast<float>(Vaqt - GapVaqt) * 5.f, 0.f, 1.f);
		Yumaloq(C, Sx - Kenglik * 0.5f - 30.f * S, Sy, Kenglik + 60.f * S, Bh, 16.f * S, FLinearColor(0.f, 0.f, 0.f, 0.6f * Alfa));
		Matn(C, G.Key ? Kim : FString(TEXT("Siz")), Sx, Sy + 12.f * S, 1.3f * S,
			G.Key ? FLinearColor(1.f, 0.8f, 0.35f, Alfa) : FLinearColor(0.55f, 0.85f, 1.f, Alfa));
		for (int32 I = 0; I < Qatorlar.Num(); ++I)
			Matn(C, Qatorlar[I], Sx, Sy + 50.f * S + Qh * I, K, FLinearColor(1.f, 1.f, 1.f, Alfa), true, true);
		Matn(C, FString::Printf(TEXT("%s — keyingisi"), *UBozorSozlamalar::TugmaNomi(UBozorSozlamalar::Get()->Tugma(TEXT("Gaplashish"))).ToString()),
			Sx, Sy + Bh - 32.f * S, 0.95f * S, FLinearColor(1.f, 1.f, 1.f, 0.5f * Alfa));
	}
}
