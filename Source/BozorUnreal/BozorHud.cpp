#include "BozorHud.h"

#include "BozorAvtobus.h"
#include "BozorOyinchi.h"
#include "BozorSozlamalar.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Sound/SoundBase.h"
#include "UnrealClient.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const FLinearColor Qogozrang(0.96f, 0.93f, 0.83f, 1.f);
	const FLinearColor Siyoh(0.07f, 0.12f, 0.38f, 1.f);
	const FLinearColor Chiziq(0.45f, 0.62f, 0.85f, 0.55f);
	const FLinearColor Hoshiya(0.85f, 0.25f, 0.25f, 0.6f);

}

ABozorHud::ABozorHud()
{
	PrimaryActorTick.bCanEverTick = true;
	static ConstructorHelpers::FObjectFinder<USoundBase> S(TEXT("/Game/Bozor/Audio/SFX/sfx_pul_01.sfx_pul_01"));
	if (S.Succeeded()) Shitir = S.Object;
}

UBozorHamyon* ABozorHud::Hamyon() const
{
	const ABozorOyinchi* O = PlayerOwner ? Cast<ABozorOyinchi>(PlayerOwner->GetPawn()) : nullptr;
	return O ? O->Hamyon.Get() : nullptr;
}

void ABozorHud::Qogoz(bool bOch)
{
	if (bOchiq == bOch) return;
	bOchiq = bOch;
	if (bOch) OchilganVaqt = GetWorld()->GetTimeSeconds();
	else bBirinchi = false;
	if (Shitir) UGameplayStatics::PlaySound2D(this, Shitir, bOch ? 0.8f : 0.5f, bOch ? 1.f : 1.15f);
}

void ABozorHud::Tick(float Dt)
{
	Super::Tick(Dt);
	if (!bFaol)
	{
		// intro tugaguncha qog'oz yo'q; intro bo'lgan bo'lsa - darhol cho'ntakdan chiqariladi
		const ABozorAvtobusIntro* Intro = nullptr;
		for (TActorIterator<ABozorAvtobusIntro> It(GetWorld()); It; ++It) Intro = *It;
		if (Intro && Intro->BoshqaruvKutilmoqda()) return;
		bFaol = true;
		FaolVaqti = GetWorld()->GetTimeSeconds();
		bTest =FParse::Param(FCommandLine::Get(), TEXT("AvtobusTest"));
		if (Intro && Intro->Oynaldi())
		{
			Qogoz(true);
			bBirinchi = true;
			bIntroBor = true;
			BozorJoyi = Intro->QarashNuqtasi;
		}
		return;
	}

	const double Hozir = GetWorld()->GetTimeSeconds();
	APlayerController* P = PlayerOwner.Get();
	const APawn* Pw = P ? P->GetPawn() : nullptr;
	if (P)
	{
		// Tab bosib turilganda ochiq, qo'yib yuborilsa yopiladi. Introdan keyingi birinchi ochilishda Tab bosilsa - shu rejimga o'tadi
		if (P->IsInputKeyDown(UBozorSozlamalar::Get()->Tugma(TEXT("Royxat"))))
		{
			bBirinchi = false;
			Qogoz(true);
		}
		else if (bOchiq && !bBirinchi) Qogoz(false);
		else if (bOchiq && bBirinchi && Hozir - OchilganVaqt > 1.5)
		{
			const bool bYurdi = Pw && Pw->GetVelocity().Size2D() > 30.f;
			// sinovda qog'oz o'zi yopiladi - maslahat ham suratga tushsin
			if (bYurdi || P->WasInputKeyJustPressed(UBozorSozlamalar::Get()->Tugma(TEXT("Gaplashish"))) || P->WasInputKeyJustPressed(EKeys::Enter) || (bTest && Hozir - OchilganVaqt > 2.5))
				Qogoz(false);
		}
	}
	Ochiqlik = FMath::FInterpConstantTo(Ochiqlik, bOchiq ? 1.f : 0.f, Dt, 2.8f);

	// birinchi qog'oz yopilgach: maslahat va belgi. Maslahat o'yinchi ~4 m yurib, 6 s o'tgach (yoki 15 s dan keyin) yo'qoladi;
	// belgi darvozaga 6 m qolganda
	if (bIntroBor && Pw)
	{
		if (MaslahatBosh < 0.0 && !bOchiq && !bBirinchi)
		{
			MaslahatBosh = Hozir;
			MaslahatJoy = Pw->GetActorLocation();
			bBelgi = true;
		}
		if (MaslahatBosh >= 0.0 && !bMaslahatTugadi)
		{
			const double O = Hozir - MaslahatBosh;
			if ((O > 6.0 && FVector::Dist2D(Pw->GetActorLocation(), MaslahatJoy) > 400.f) || O > 15.0) bMaslahatTugadi = true;
		}
		if (bBelgi && !bQaytish && FVector::Dist2D(Pw->GetActorLocation(), BozorJoyi) < 600.f) bBelgi = false;
	}
	MaslahatAlfa = FMath::FInterpConstantTo(MaslahatAlfa, MaslahatBosh >= 0.0 && !bMaslahatTugadi ? 1.f : 0.f, Dt, 2.f);
	QaytishniTekshir();
	BelgiAlfa = FMath::FInterpConstantTo(BelgiAlfa, bBelgi && !bOchiq ? 1.f : 0.f, Dt, 2.f);

	if (bTest && !bTestSurat && Ochiqlik >= 1.f)
	{
		bTestSurat = true;
		FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots/AvtobusTest/4c_royxat.png"), false, false);
	}
	if (bTest && !bMaslahatSurat && MaslahatAlfa >= 1.f)
	{
		bMaslahatSurat = true;
		FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots/AvtobusTest/4d_maslahat.png"), false, false);
	}
}

void ABozorHud::DrawHUD()
{
	Super::DrawHUD();
	if (!bFaol || !Canvas) return;
	const ABozorOyinchi* Oy = PlayerOwner ? Cast<ABozorOyinchi>(PlayerOwner->GetPawn()) : nullptr;
	if (Oy && Oy->NatijaKorinadi())
	{
		NatijaChiz();
		return;
	}
	if (BelgiAlfa > 0.f) BelgiChiz();
	// daraja nomi (boshida 5 s)
	if (GetWorld()->GetTimeSeconds() - FaolVaqti < 5.0)
		if (const UBozorSotuvTizimi* Tz = UBozorSotuvTizimi::Get(this))
			if (const FBozorDaraja* D = Tz->GetDaraja())
			{
				const float S = Canvas->ClipY / 1080.f;
				const float A = FMath::Clamp(static_cast<float>(5.0 - (GetWorld()->GetTimeSeconds() - FaolVaqti)), 0.f, 1.f);
				FCanvasTextItem T(FVector2D(Canvas->ClipX * 0.5f, 60.f * S), FText::FromString(FString::Printf(TEXT("%d-daraja: %s"), D->Raqam, *D->Nomi)),
					GEngine->GetLargeFont(), FLinearColor(1.f, 1.f, 1.f, A));
				T.Scale = FVector2D(2.2f * S);
				T.bCentreX = true;
				T.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.8f * A));
				Canvas->DrawItem(T);
			}
	// ro'yxat tugagani haqida eslatma (6 s)
	if (bQaytish && GetWorld()->GetTimeSeconds() - QaytishVaqti < 6.0)
	{
		const float S = Canvas->ClipY / 1080.f;
		FCanvasTextItem T(FVector2D(Canvas->ClipX * 0.5f, 120.f * S), FText::FromString(TEXT("Ro'yxatdagi hamma narsa olindi! Endi chiqishga qayting")),
			GEngine->GetLargeFont(), FLinearColor(1.f, 0.85f, 0.3f, 1.f));
		T.Scale = FVector2D(1.8f * S);
		T.bCentreX = true;
		T.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.8f));
		Canvas->DrawItem(T);
	}
	if (MaslahatAlfa > 0.f) MaslahatChiz();
	if (Ochiqlik > 0.f) QogozChiz(Ochiqlik);
	SotuvChiz();

	// yopiq paytda burchakda eslatma (maslahat ko'rinib turganda - unda Tab bor)
	if (Ochiqlik < 1.f && MaslahatAlfa < 1.f)
	{
		const float S = Canvas->ClipY / 1080.f;
		FCanvasTextItem T(FVector2D(40.f * S, Canvas->ClipY - 70.f * S), FText::FromString(UBozorSozlamalar::TugmaNomi(UBozorSozlamalar::Get()->Tugma(TEXT("Royxat"))).ToString() + TEXT(" (bosib turing) — ro'yxat")), GEngine->GetMediumFont(),
			FLinearColor(1.f, 1.f, 1.f, 0.75f * (1.f - Ochiqlik) * (1.f - MaslahatAlfa)));
		T.Scale = FVector2D(1.3f * S);
		T.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.6f * (1.f - Ochiqlik) * (1.f - MaslahatAlfa)));
		Canvas->DrawItem(T);
	}
}

void ABozorHud::QogozChiz(float A)
{
	const UBozorHamyon* H = Hamyon();
	if (!H) return;
	UFont* Shrift = GEngine->GetLargeFont();
	const float S = Canvas->ClipY / 1080.f;
	const float Yuz = 1.f - FMath::Pow(1.f - A, 3.f);   // ease-out: cho'ntakdan tez chiqib, sekin to'xtaydi

	const float Qator = 66.f * S;
	const float W = 580.f * S;
	const float Bosh = 130.f * S;   // sarlavha joyi
	const int32 N = H->Royxat.Num();
	const float Hgt = Bosh + Qator * FMath::Max(N, 1) + 245.f * S;
	const float X = Canvas->ClipX - W - 90.f * S;
	const float Y = FMath::Lerp(Canvas->ClipY + 30.f * S, (Canvas->ClipY - Hgt) * 0.5f, Yuz);

	auto Tort = [this](float X0, float Y0, float W0, float H0, const FLinearColor& C)
	{
		FCanvasTileItem T(FVector2D(X0, Y0), FVector2D(W0, H0), C);
		T.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(T);
	};
	auto Chiz = [this](float X0, float Y0, float X1, float Y1, const FLinearColor& C, float Q)
	{
		FCanvasLineItem L(FVector2D(X0, Y0), FVector2D(X1, Y1));
		L.SetColor(C);
		L.LineThickness = Q;
		Canvas->DrawItem(L);
	};
	auto Yoz = [this, Shrift](const FString& Matn, float X0, float Y0, float Kattalik, const FLinearColor& C)
	{
		FCanvasTextItem T(FVector2D(X0, Y0), FText::FromString(Matn), Shrift, C);
		T.Scale = FVector2D(Kattalik);
		Canvas->DrawItem(T);
	};

	// soya va qog'oz
	Tort(X + 10.f * S, Y + 12.f * S, W, Hgt, FLinearColor(0.f, 0.f, 0.f, 0.35f));
	Tort(X, Y, W, Hgt, Qogozrang);
	// daftar chiziqlari va chap hoshiya
	for (float Ly = Y + Bosh; Ly < Y + Hgt - 20.f * S; Ly += Qator)
		Chiz(X + 12.f * S, Ly, X + W - 12.f * S, Ly, Chiziq, 1.5f * S);
	Chiz(X + 62.f * S, Y, X + 62.f * S, Y + Hgt, Hoshiya, 2.f * S);

	const float Kt = 2.1f * S;   // matn kattaligi (GetLargeFont kichik - 10-13 yoshli bola uzoqdan o'qisin)
	Yoz(TEXT("Xarid ro'yxati"), X + 80.f * S, Y + 38.f * S, 2.6f * S, Siyoh);

	for (int32 I = 0; I < N; ++I)
	{
		const FBozorRoyxatQator& R = H->Royxat[I];
		const float Ly = Y + Bosh + Qator * I;          // qator ostidagi chiziq ustidan yoziladi
		const float Ty = Ly + Qator * 0.5f - 22.f * S;
		// katakcha
		const float Kx = X + 20.f * S, Ky = Ty + 8.f * S, Kk = 28.f * S;
		Chiz(Kx, Ky, Kx + Kk, Ky, Siyoh, 2.f * S);
		Chiz(Kx + Kk, Ky, Kx + Kk, Ky + Kk, Siyoh, 2.f * S);
		Chiz(Kx + Kk, Ky + Kk, Kx, Ky + Kk, Siyoh, 2.f * S);
		Chiz(Kx, Ky + Kk, Kx, Ky, Siyoh, 2.f * S);
		const FString Matn = FString::Printf(TEXT("%s — %s %s"), *R.Nomi, *BozorSotuv::Miqdor(R.Miqdor), *R.Birlik);
		Yoz(Matn, X + 80.f * S, Ty, Kt, R.bOlindi ? Siyoh * FLinearColor(1.f, 1.f, 1.f, 0.5f) : Siyoh);
		if (R.bOlindi)
		{
			// belgi va ustidan chizish
			Chiz(Kx + 4.f * S, Ky + Kk * 0.5f, Kx + Kk * 0.4f, Ky + Kk - 3.f * S, FLinearColor(0.1f, 0.5f, 0.15f), 3.f * S);
			Chiz(Kx + Kk * 0.4f, Ky + Kk - 3.f * S, Kx + Kk + 6.f * S, Ky - 6.f * S, FLinearColor(0.1f, 0.5f, 0.15f), 3.f * S);
			float Tw = 0.f, Th = 0.f;
			Canvas->TextSize(Shrift, Matn, Tw, Th, Kt, Kt);
			Chiz(X + 78.f * S, Ty + Th * 0.55f, X + 82.f * S + Tw, Ty + Th * 0.5f, Siyoh, 2.f * S);
		}
	}

	// pul va eslatma
	const float Py = Y + Bosh + Qator * FMath::Max(N, 1) + 20.f * S;
	Yoz(FString::Printf(TEXT("Pul: %s so'm"), *BozorSotuv::Som(H->Pul)), X + 80.f * S, Py, Kt, Siyoh);
	Yoz(FString::Printf(TEXT("(%s)"), *H->Tarkibi()), X + 80.f * S, Py + 52.f * S, 1.5f * S, Siyoh * FLinearColor(1.f, 1.f, 1.f, 0.75f));
	Yoz(H->Eslatma, X + 80.f * S, Py + 110.f * S, 1.8f * S, FLinearColor(0.55f, 0.1f, 0.1f));

	// birinchi ochilishda: qanday yopilishi va keyin qanday ochilishi (Tab bilan ochilganda yozuv kerak emas)
	if (!bBirinchi) return;
	FCanvasTextItem T(FVector2D(X, Y + Hgt + 18.f * S),
		FText::FromString(TEXT("Yura boshlang — keyin ro'yxatni Tab'ni bosib turib ko'rasiz")),
		GEngine->GetMediumFont(), FLinearColor(1.f, 1.f, 1.f, 0.85f * Yuz));
	T.Scale = FVector2D(1.6f * S);
	T.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.7f * Yuz));
	Canvas->DrawItem(T);
}

// Pastda, o'rtada: tugmalar va nima qilishi. Tugma - oq qopqoq ichida qora harf
void ABozorHud::MaslahatChiz()
{
	const float S = Canvas->ClipY / 1080.f;
	const float A = MaslahatAlfa;
	UFont* Shrift = GEngine->GetLargeFont();
	const float Kt = 1.7f * S;
	struct FBand { TArray<FString> Tugmalar; FString Nima; };
	auto Tn = [](const TCHAR* Amal) { return UBozorSozlamalar::TugmaNomi(UBozorSozlamalar::Get()->Tugma(Amal)).ToString(); };
	const FBand Bandlar[] = {
		{ { Tn(TEXT("Oldinga")), Tn(TEXT("Chapga")), Tn(TEXT("Orqaga")), Tn(TEXT("Onga")) }, TEXT("yurish") },
		{ { TEXT("Sichqoncha") }, TEXT("atrofga qarash") },
		{ { Tn(TEXT("Royxat")) }, TEXT("ro'yxat (bosib turing)") },
	};
	const float Pad = 14.f * S, Oraliq = 8.f * S, BandOraliq = 46.f * S, QopH = 46.f * S;

	auto Olcham = [this, Shrift, Kt](const FString& T) { float W = 0.f, H = 0.f; Canvas->TextSize(Shrift, T, W, H, Kt, Kt); return FVector2D(W, H); };
	// umumiy kenglik
	float Jami = 0.f;
	for (const FBand& B : Bandlar)
	{
		for (const FString& T : B.Tugmalar) Jami += FMath::Max(QopH, Olcham(T).X + 2.f * Pad) + Oraliq;
		Jami += Olcham(B.Nima).X + BandOraliq;
	}
	Jami -= BandOraliq;

	const float PanelH = QopH + 2.f * 18.f * S + 44.f * S;
	const float X0 = (Canvas->ClipX - Jami) * 0.5f;
	const float Y0 = Canvas->ClipY - PanelH - 60.f * S + (1.f - A) * 30.f * S;   // pastdan sal ko'tarilib chiqadi

	auto Tort = [this](float X, float Y, float W, float H, const FLinearColor& C)
	{
		FCanvasTileItem T(FVector2D(X, Y), FVector2D(W, H), C);
		T.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(T);
	};
	auto Yoz = [this, Shrift](const FString& M, float X, float Y, float K, const FLinearColor& C, bool bSoya)
	{
		FCanvasTextItem T(FVector2D(X, Y), FText::FromString(M), Shrift, C);
		T.Scale = FVector2D(K);
		if (bSoya) T.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.7f * C.A));
		Canvas->DrawItem(T);
	};

	Tort(X0 - 30.f * S, Y0, Jami + 60.f * S, PanelH, FLinearColor(0.f, 0.f, 0.f, 0.5f * A));
	Yoz(TEXT("Bozor darvozasi tomon yuring"), X0, Y0 + 12.f * S, 1.45f * S, FLinearColor(1.f, 0.85f, 0.3f, A), true);

	float X = X0;
	const float Yq = Y0 + 18.f * S + 44.f * S;
	for (const FBand& B : Bandlar)
	{
		for (const FString& T : B.Tugmalar)
		{
			const FVector2D O = Olcham(T);
			const float W = FMath::Max(QopH, O.X + 2.f * Pad);
			Tort(X, Yq + 3.f * S, W, QopH, FLinearColor(0.f, 0.f, 0.f, 0.6f * A));          // qopqoq soyasi
			Tort(X, Yq, W, QopH, FLinearColor(0.95f, 0.95f, 0.93f, 0.95f * A));
			Yoz(T, X + (W - O.X) * 0.5f, Yq + (QopH - O.Y) * 0.5f, Kt, FLinearColor(0.08f, 0.08f, 0.1f, A), false);
			X += W + Oraliq;
		}
		const FVector2D O = Olcham(B.Nima);
		Yoz(B.Nima, X + 4.f * S, Yq + (QopH - O.Y) * 0.5f, Kt, FLinearColor(1.f, 1.f, 1.f, A), true);
		X += O.X + BandOraliq;
	}
}

// Sotuvchi oldida: pastda o'rtada "[E] Gaplashish". Suhbat paytida (va tugagach 3 s): pastda oyna - bolaning oxirgi gapi
// kulrangda, sotuvchi gapi oq rangda, so'zlar bo'yicha qatorlarga bo'linadi
void ABozorHud::SotuvChiz()
{
	const ABozorOyinchi* O = PlayerOwner ? Cast<ABozorOyinchi>(PlayerOwner->GetPawn()) : nullptr;
	if (!O) return;
	const float S = Canvas->ClipY / 1080.f;
	UFont* Shrift = GEngine->GetLargeFont();
	const double Hozir = GetWorld()->GetTimeSeconds();

	auto Tort = [this](float X, float Y, float W, float H, const FLinearColor& C)
	{
		FCanvasTileItem T(FVector2D(X, Y), FVector2D(W, H), C);
		T.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(T);
	};
	auto Yoz = [this, Shrift](const FString& M, float X, float Y, float K, const FLinearColor& C)
	{
		FCanvasTextItem T(FVector2D(X, Y), FText::FromString(M), Shrift, C);
		T.Scale = FVector2D(K);
		T.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.7f * C.A));
		Canvas->DrawItem(T);
	};
	auto Olcham = [this, Shrift](const FString& M, float K) { float W = 0.f, H = 0.f; Canvas->TextSize(Shrift, M, W, H, K, K); return FVector2D(W, H); };
	auto Qatorla = [&Olcham](const FString& M, float K, float Kenglik)
	{
		TArray<FString> Sozlar, Qatorlar;
		M.ParseIntoArrayWS(Sozlar);
		FString Q;
		for (const FString& W : Sozlar)
		{
			const FString Sinov = Q.IsEmpty() ? W : Q + TEXT(" ") + W;
			if (!Q.IsEmpty() && Olcham(Sinov, K).X > Kenglik) { Qatorlar.Add(Q); Q = W; }
			else Q = Sinov;
		}
		if (!Q.IsEmpty()) Qatorlar.Add(Q);
		return Qatorlar;
	};
	auto Tugma = [&](const FString& Harf, float X, float Y, float A)
	{
		const float Q = 46.f * S;
		Tort(X, Y + 3.f * S, Q, Q, FLinearColor(0.f, 0.f, 0.f, 0.6f * A));
		Tort(X, Y, Q, Q, FLinearColor(0.95f, 0.95f, 0.93f, 0.95f * A));
		const FVector2D O = Olcham(Harf, 1.7f * S);
		FCanvasTextItem T(FVector2D(X + (Q - O.X) * 0.5f, Y + (Q - O.Y) * 0.5f), FText::FromString(Harf), Shrift, FLinearColor(0.08f, 0.08f, 0.1f, A));
		T.Scale = FVector2D(1.7f * S);
		Canvas->DrawItem(T);
		return Q;
	};

	// E belgisi
	if (!O->Suhbatda() && O->GetYaqinSotuvchi() && Ochiqlik < 0.5f)
	{
		const FString M = TEXT("Gaplashish");
		const FVector2D Om = Olcham(M, 1.7f * S);
		const float W = 46.f * S + 16.f * S + Om.X;
		const float X = (Canvas->ClipX - W) * 0.5f, Y = Canvas->ClipY * 0.62f;
		Tort(X - 20.f * S, Y - 14.f * S, W + 40.f * S, 74.f * S, FLinearColor(0.f, 0.f, 0.f, 0.45f));
		const float Q = Tugma(UBozorSozlamalar::TugmaNomi(UBozorSozlamalar::Get()->Tugma(TEXT("Gaplashish"))).ToString(), X, Y, 1.f);
		Yoz(M, X + Q + 16.f * S, Y + (Q - Om.Y) * 0.5f, 1.7f * S, FLinearColor::White);
	}

	// "sotuvchi band" kabi qisqa xabar (3 s)
	if (Hozir - O->GetXabarVaqti() < 3.0 && !O->GetXabar().IsEmpty())
	{
		const FVector2D Om = Olcham(O->GetXabar(), 1.5f * S);
		Yoz(O->GetXabar(), (Canvas->ClipX - Om.X) * 0.5f, Canvas->ClipY * 0.55f, 1.5f * S, FLinearColor(1.f, 0.75f, 0.3f, 1.f));
	}

	// suhbat oynasi: bolaning oxirgi gapi, sotuvchi (kayfiyati bilan) va gapi, pastida javob variantlari
	TanlovJoylari.Reset();
	const float A = O->Suhbatda() ? 1.f : FMath::Clamp(1.f - static_cast<float>(Hozir - O->GetGapVaqti() - 3.0), 0.f, 1.f);
	if (A <= 0.f || O->GetSotuvchiGapi().IsEmpty()) return;
	const TArray<FBozorTanlov> Tanlovlar = O->GetTanlovlar();
	const float W = FMath::Min(1200.f * S, Canvas->ClipX - 80.f * S);
	const float Ichki = W - 60.f * S;
	const float Kt = 1.75f * S;
	const TArray<FString> Gap = Qatorla(O->GetSotuvchiGapi(), Kt, Ichki);
	const float QatorH = Olcham(TEXT("Ay"), Kt).Y * 1.15f;
	const bool bMenGapi = !O->GetOxirgiGapim().IsEmpty();
	const float TugmaH = 54.f * S, TugmaOraliq = 8.f * S;
	// kupyuralar - bir qatorda plitka; qolgan variantlar - qatorma-qator
	const float PlitkaW = 150.f * S, PlitkaH = 74.f * S, PlitkaOraliq = 10.f * S;
	int32 Plitkalar = 0;
	for (const FBozorTanlov& T : Tanlovlar) Plitkalar += T.Tur == EBozorTanlovTur::KupyuraQoy ? 1 : 0;
	const int32 BirQatorda = FMath::Max(1, FMath::FloorToInt((Ichki + PlitkaOraliq) / (PlitkaW + PlitkaOraliq)));
	const int32 PlitkaQatorlari = FMath::DivideAndRoundUp(Plitkalar, BirQatorda);
	const UBozorSuhbat* Sh0 = O->GetSuhbat();
	const bool bTolov = Sh0 && Sh0->Tolovda() && O->Suhbatda();
	const float TanlovH = Tanlovlar.Num() ? 24.f * S + (Tanlovlar.Num() - Plitkalar) * (TugmaH + TugmaOraliq) + PlitkaQatorlari * (PlitkaH + PlitkaOraliq) : 0.f;
	const float H = 30.f * S + (bMenGapi ? 44.f * S : 0.f) + 44.f * S + QatorH * Gap.Num() + (bTolov ? 48.f * S : 0.f) + TanlovH + 70.f * S;
	const float X = (Canvas->ClipX - W) * 0.5f;
	const float Y = Canvas->ClipY - H - 40.f * S;
	Tort(X, Y, W, H, FLinearColor(0.02f, 0.03f, 0.06f, 0.72f * A));
	Tort(X, Y, 6.f * S, H, FLinearColor(1.f, 0.78f, 0.25f, 0.9f * A));
	float Ty = Y + 22.f * S;
	if (bMenGapi)
	{
		Yoz(FString::Printf(TEXT("Siz: %s"), *O->GetOxirgiGapim()), X + 30.f * S, Ty, 1.35f * S, FLinearColor(0.75f, 0.78f, 0.82f, A));
		Ty += 44.f * S;
	}
	Yoz(TEXT("Sotuvchi"), X + 30.f * S, Ty, 1.4f * S, FLinearColor(1.f, 0.78f, 0.25f, A));
	// sotuvchining kayfiyati - bolaning tanlovlari unga qanday ta'sir qilayotgani ko'rinsin
	if (const UBozorSuhbat* Sh = O->GetSuhbat())
	{
		const int32 M = Sh->GetHolat().Munosabat;
		const FString K = M >= 70 ? TEXT("kayfiyati: yaxshi") : M >= 30 ? TEXT("kayfiyati: oddiy") : TEXT("kayfiyati: yomon");
		const FLinearColor R = M >= 70 ? FLinearColor(0.45f, 0.9f, 0.45f, A) : M >= 30 ? FLinearColor(0.8f, 0.8f, 0.8f, A) : FLinearColor(1.f, 0.4f, 0.35f, A);
		Yoz(K, X + 30.f * S + Olcham(TEXT("Sotuvchi"), 1.4f * S).X + 24.f * S, Ty + 4.f * S, 1.2f * S, R);
	}
	Ty += 44.f * S;
	for (const FString& Q : Gap)
	{
		Yoz(Q, X + 30.f * S, Ty, Kt, FLinearColor(1.f, 1.f, 1.f, A));
		Ty += QatorH;
	}

	// to'lov: peshtaxtadagi kupyuralar va kerakli summa
	if (bTolov)
	{
		int32 Jami = 0;
		TArray<FString> K;
		for (int32 N : Sh0->GetPeshtaxta()) { Jami += N; K.Add(BozorSotuv::Som(N)); }
		const FString M = K.IsEmpty()
			? FString::Printf(TEXT("Narxi: %s so'm. Kupyuralarni bosib peshtaxtaga qo'ying"), *BozorSotuv::Som(Sh0->GetHolat().Summa))
			: FString::Printf(TEXT("Peshtaxtada: %s  (jami %s so'm, narxi %s so'm)"), *FString::Join(K, TEXT(" + ")), *BozorSotuv::Som(Jami), *BozorSotuv::Som(Sh0->GetHolat().Summa));
		Ty += 8.f * S;
		Yoz(M, X + 30.f * S, Ty, 1.3f * S, FLinearColor(0.6f, 0.95f, 0.6f, A));
		Ty += 40.f * S;
	}

	// javob variantlari: raqam qopqog'i + matn; sichqoncha ustida - yorqinroq. Kupyuralar - yashil plitkalar
	if (Tanlovlar.Num())
	{
		Ty += 24.f * S;
		float Mx = -1.f, My = -1.f;
		if (PlayerOwner) PlayerOwner->GetMousePosition(Mx, My);
		TanlovJoylari.SetNum(Tanlovlar.Num());
		int32 P = 0;
		for (int32 I = 0; I < Tanlovlar.Num(); ++I)
		{
			if (Tanlovlar[I].Tur != EBozorTanlovTur::KupyuraQoy) continue;
			const float Px = X + 24.f * S + (P % BirQatorda) * (PlitkaW + PlitkaOraliq);
			const float Py = Ty + (P / BirQatorda) * (PlitkaH + PlitkaOraliq);
			const FBox2D Joy(FVector2D(Px, Py), FVector2D(Px + PlitkaW, Py + PlitkaH));
			const bool bUstida = Joy.IsInside(FVector2D(Mx, My));
			Tort(Px, Py, PlitkaW, PlitkaH, bUstida ? FLinearColor(0.35f, 0.7f, 0.35f, 0.95f * A) : FLinearColor(0.2f, 0.45f, 0.25f, 0.9f * A));
			Tort(Px + 4.f * S, Py + 4.f * S, PlitkaW - 8.f * S, 2.f * S, FLinearColor(1.f, 1.f, 1.f, 0.35f * A));
			const FVector2D Om = Olcham(Tanlovlar[I].Matn, 1.45f * S);
			Yoz(Tanlovlar[I].Matn, Px + (PlitkaW - Om.X) * 0.5f, Py + 10.f * S, 1.45f * S, FLinearColor(1.f, 1.f, 1.f, A));
			const FString Son = FString::Printf(TEXT("%d.   × %d"), I + 1, Tanlovlar[I].Soni);
			const FVector2D Os = Olcham(Son, 1.05f * S);
			Yoz(Son, Px + (PlitkaW - Os.X) * 0.5f, Py + PlitkaH - Os.Y - 8.f * S, 1.05f * S, FLinearColor(0.9f, 1.f, 0.9f, 0.85f * A));
			TanlovJoylari[I] = Joy;
			++P;
		}
		if (P > 0) Ty += PlitkaQatorlari * (PlitkaH + PlitkaOraliq);
		for (int32 I = 0; I < Tanlovlar.Num(); ++I)
		{
			if (Tanlovlar[I].Tur == EBozorTanlovTur::KupyuraQoy) continue;
			const FBox2D Joy(FVector2D(X + 24.f * S, Ty), FVector2D(X + W - 24.f * S, Ty + TugmaH));
			const bool bUstida = Joy.IsInside(FVector2D(Mx, My));
			Tort(Joy.Min.X, Joy.Min.Y, Joy.GetSize().X, TugmaH, bUstida ? FLinearColor(1.f, 0.78f, 0.25f, 0.35f * A) : FLinearColor(1.f, 1.f, 1.f, 0.08f * A));
			Tugma(FString::FromInt(I + 1), Joy.Min.X + 6.f * S, Ty + (TugmaH - 46.f * S) * 0.5f, A);
			const FVector2D Om = Olcham(Tanlovlar[I].Matn, 1.5f * S);
			Yoz(Tanlovlar[I].Matn, Joy.Min.X + 70.f * S, Ty + (TugmaH - Om.Y) * 0.5f, 1.5f * S, FLinearColor(1.f, 1.f, 1.f, A));
			TanlovJoylari[I] = Joy;
			Ty += TugmaH + TugmaOraliq;
		}
	}

	if (O->Suhbatda() && !O->Xayrlashdi())
	{
		const FString M = TEXT("xayrlashmasdan ketish");
		const FVector2D Om = Olcham(M, 1.25f * S);
		const float Bx = X + W - Om.X - 30.f * S - 46.f * S - 12.f * S, By = Y + H - 60.f * S;
		const float Q = Tugma(UBozorSozlamalar::TugmaNomi(UBozorSozlamalar::Get()->Tugma(TEXT("Gaplashish"))).ToString(), Bx, By, 0.8f * A);
		Yoz(M, Bx + Q + 12.f * S, By + (Q - Om.Y) * 0.5f, 1.25f * S, FLinearColor(1.f, 1.f, 1.f, 0.7f * A));
	}
}

// Ro'yxat tugagach: belgi chiqishga (intro bo'lsa - bozor darvozasi, bo'lmasa o'yin boshlangan joy) buriladi;
// o'yinchi u yerga 5 m yaqinlashsa - natija oynasi
void ABozorHud::QaytishniTekshir()
{
	APlayerController* P = PlayerOwner.Get();
	ABozorOyinchi* O = P ? Cast<ABozorOyinchi>(P->GetPawn()) : nullptr;
	const UBozorHamyon* H = Hamyon();
	if (!O || !H) return;
	if (!bBoshJoy)
	{
		BoshJoy = O->GetActorLocation();
		bBoshJoy = true;
	}
	if (!bQaytish && H->RoyxatTugadimi())
	{
		bQaytish = true;
		QaytishVaqti = GetWorld()->GetTimeSeconds();
		ChiqishJoyi = bIntroBor ? BozorJoyi : BoshJoy;
		bBelgi = true;
	}
	if (bQaytish && !O->NatijaKorinadi() && !O->Suhbatda() && FVector::Dist2D(O->GetActorLocation(), ChiqishJoyi) < 500.f) O->Natija();
}

// Natija: yulduzlar (3 ta romb), xarid, pul, odob, qaytim va maslahat. Enter - qaytadan
void ABozorHud::NatijaChiz()
{
	const UBozorHamyon* H = Hamyon();
	if (!H) return;
	const float S = Canvas->ClipY / 1080.f;
	UFont* Shrift = GEngine->GetLargeFont();
	auto Tort = [this](float X, float Y, float W, float Hh, const FLinearColor& C)
	{
		FCanvasTileItem T(FVector2D(X, Y), FVector2D(W, Hh), C);
		T.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(T);
	};
	auto Yoz = [this, Shrift](const FString& M, float X, float Y, float K, const FLinearColor& C, bool bMarkaz = false)
	{
		FCanvasTextItem T(FVector2D(X, Y), FText::FromString(M), Shrift, C);
		T.Scale = FVector2D(K);
		T.bCentreX = bMarkaz;
		T.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.6f));
		Canvas->DrawItem(T);
	};
	auto Romb = [this](const FVector2D& M, float R, const FLinearColor& C)
	{
		FCanvasTileItem T(M - FVector2D(R, R), FVector2D(2.f * R, 2.f * R), C);
		T.BlendMode = SE_BLEND_Translucent;
		T.Rotation = FRotator(0.f, 45.f, 0.f);
		T.PivotPoint = FVector2D(0.5f, 0.5f);
		Canvas->DrawItem(T);
	};

	// jadval: ro'yxatdagi har mol (kerak / olindi / to'landi / narx / baho), keyin ro'yxatdan tashqari olinganlar
	const TArray<FBozorNatijaQator> Jadval = H->NatijaQatorlari();
	const float QatorH = 40.f * S;
	Tort(0.f, 0.f, Canvas->ClipX, Canvas->ClipY, FLinearColor(0.f, 0.f, 0.f, 0.55f));
	const float W = 1240.f * S, Hh = FMath::Min(Canvas->ClipY - 20.f * S, 780.f * S + QatorH * (Jadval.Num() + 1));
	const float X = (Canvas->ClipX - W) * 0.5f, Y = (Canvas->ClipY - Hh) * 0.5f;
	Tort(X, Y, W, Hh, FLinearColor(0.96f, 0.93f, 0.83f, 0.97f));
	const bool bTugadi = H->RoyxatTugadimi();
	Yoz(bTugadi ? TEXT("Xarid tugadi!") : TEXT("Bozordan qaytish"), Canvas->ClipX * 0.5f, Y + 30.f * S, 2.6f * S, Siyoh, true);

	const int32 Y3 = H->Yulduzlar();
	for (int32 I = 0; I < 3; ++I)
	{
		const FVector2D M(Canvas->ClipX * 0.5f + (I - 1) * 110.f * S, Y + 175.f * S);
		Romb(M, 40.f * S, FLinearColor(0.3f, 0.25f, 0.15f, 0.35f));
		if (I < Y3) Romb(M, 34.f * S, FLinearColor(1.f, 0.75f, 0.1f, 1.f));
	}

	int32 Olindi = 0;
	for (const FBozorRoyxatQator& R : H->Royxat) Olindi += R.bOlindi ? 1 : 0;
	const FString Qatorlar[] = {
		FString::Printf(TEXT("Ro'yxat: %d / %d"), Olindi, H->Royxat.Num()),
		FString::Printf(TEXT("Sarflandi: %s so'm, qoldi: %s so'm"), *BozorSotuv::Som(H->BoshlangichPul - H->Pul), *BozorSotuv::Som(H->Pul)),
		FString::Printf(TEXT("Savdolashib tejaldi: %s so'm"), *BozorSotuv::Som(H->Tejaldi)),
		FString::Printf(TEXT("Odob: salom bilan boshlangan suhbat %d / %d, qo'pol gap %d"), H->Suhbatlar - H->Salomsiz, H->Suhbatlar, H->Qopol),
		FString::Printf(TEXT("Qaytim: kam qaytim sezildi %d, sezilmadi %d"), H->XatoQaytimSezildi, H->XatoQaytimOtkazildi),
		FString::Printf(TEXT("Tarozi: ko'p/kam tortilganini aytdingiz %d, indamadingiz %d"), H->TaroziSezildi, H->TaroziOtkazildi),
	};
	float Ty = Y + 245.f * S;
	for (const FString& Q : Qatorlar)
	{
		Yoz(Q, X + 70.f * S, Ty, 1.55f * S, Siyoh);
		Ty += 44.f * S;
	}

	// xaridlar jadvali
	Ty += 12.f * S;
	const float Ustun[] = {70.f, 300.f, 470.f, 690.f, 880.f, 1080.f};
	const TCHAR* Sarlavha[] = {TEXT("Mahsulot"), TEXT("Kerak edi"), TEXT("Olindi"), TEXT("To'landi"), TEXT("Narx (1 birlik)"), TEXT("Baho")};
	Tort(X + 50.f * S, Ty - 4.f * S, W - 100.f * S, QatorH, FLinearColor(0.3f, 0.25f, 0.15f, 0.18f));
	for (int32 I = 0; I < UE_ARRAY_COUNT(Ustun); ++I) Yoz(Sarlavha[I], X + Ustun[I] * S, Ty, 1.35f * S, Siyoh * FLinearColor(1.f, 1.f, 1.f, 0.75f));
	Ty += QatorH;
	for (const FBozorNatijaQator& Q : Jadval)
	{
		const FString Baho = Q.Baho();
		// to'g'ri - yashil, ko'p - to'q sariq, kam/olinmadi - qizil, ro'yxatda yo'q - kulrang
		const FLinearColor Rang = Baho == TEXT("to'g'ri") ? FLinearColor(0.1f, 0.45f, 0.15f, 1.f)
			: Baho == TEXT("ko'p") ? FLinearColor(0.75f, 0.4f, 0.05f, 1.f)
			: Q.Kerak < 0.f ? FLinearColor(0.35f, 0.35f, 0.35f, 1.f) : FLinearColor(0.7f, 0.12f, 0.1f, 1.f);
		FString OlindiMatn = Q.Olindi > 0.f ? FString::Printf(TEXT("%s %s"), *BozorSotuv::Miqdor(Q.Olindi), *Q.Birlik) : FString(TEXT("—"));
		if (Q.Kerak >= 0.f && Q.Olindi > 0.f && !FMath::IsNearlyEqual(Q.Olindi, Q.Kerak, 0.05f))
			OlindiMatn += FString::Printf(TEXT(" (%s%s)"), Q.Olindi > Q.Kerak ? TEXT("+") : TEXT("−"), *BozorSotuv::Miqdor(FMath::Abs(Q.Olindi - Q.Kerak)));
		const FString Narx = Q.Olindi <= 0.f ? FString(TEXT("—"))
			: Q.Kelishildi < Q.Narx ? FString::Printf(TEXT("%s → %s"), *BozorSotuv::Som(Q.Narx), *BozorSotuv::Som(Q.Kelishildi)) : BozorSotuv::Som(Q.Kelishildi);
		const FString Ustunlar[] = {
			Q.Nomi,
			Q.Kerak >= 0.f ? FString::Printf(TEXT("%s %s"), *BozorSotuv::Miqdor(Q.Kerak), *Q.Birlik) : FString(TEXT("—")),
			OlindiMatn,
			Q.Tolandi > 0 ? FString::Printf(TEXT("%s so'm"), *BozorSotuv::Som(Q.Tolandi)) : FString(TEXT("—")),
			Narx,
			Baho,
		};
		for (int32 I = 0; I < UE_ARRAY_COUNT(Ustun); ++I) Yoz(Ustunlar[I], X + Ustun[I] * S, Ty, 1.35f * S, I == 0 ? Siyoh : Rang);
		Ty += QatorH;
	}

	// eng muhim maslahat (birinchi topilgani)
	FString Maslahat;
	if (!bTugadi) Maslahat = TEXT("Ro'yxatdagi hamma narsani olishni unutmang.");
	else if (H->Salomsiz > 0) Maslahat = TEXT("Sotuvchi bilan gapni salomdan boshlang.");
	else if (H->Qopol > 0) Maslahat = TEXT("Kattalarga qo'pol gapirmang - odobli bolaga sotuvchi arzonroq beradi.");
	else if (H->XatoQaytimOtkazildi > 0) Maslahat = TEXT("Qaytimni har doim sanab oling.");
	else if (H->TaroziOtkazildi > 0) Maslahat = TEXT("Tarozini kuzating: ko'p yoki kam tortilsa, odob bilan ayting.");
	else if (H->Tejaldi == 0) Maslahat = TEXT("Iltimos qilib savdolashsangiz, pulni tejaysiz.");
	else Maslahat = TEXT("Barakalla! Bozorda o'zingizni juda yaxshi tutdingiz.");
	Ty += 20.f * S;
	Yoz(Maslahat, Canvas->ClipX * 0.5f, Ty, 1.7f * S, FLinearColor(0.55f, 0.1f, 0.1f, 1.f), true);
	// daraja va keyingi qadam: o'tilgan bo'lsa Enter - keyingi daraja, R - qaytadan
	const UBozorSotuvTizimi* Tz = UBozorSotuvTizimi::Get(this);
	const FBozorDaraja* D = Tz ? Tz->GetDaraja() : nullptr;
	if (D) Yoz(FString::Printf(TEXT("%d-daraja: %s"), D->Raqam, *D->Nomi), Canvas->ClipX * 0.5f, Y + 92.f * S, 1.4f * S, Siyoh * FLinearColor(1.f, 1.f, 1.f, 0.7f), true);
	const bool bKeyingi = Y3 > 0 && D && D->Raqam < Tz->DarajalarSoni();
	const FString Pastki = bKeyingi ? TEXT("Enter — keyingi daraja     R — qaytadan")
		: Y3 > 0 ? TEXT("Hamma darajalar tugadi!     Enter — yana o'ynash") : TEXT("Enter — qaytadan o'ynash");
	Yoz(Pastki, Canvas->ClipX * 0.5f, Y + Hh - 70.f * S, 1.5f * S, Siyoh * FLinearColor(1.f, 1.f, 1.f, 0.7f), true);
}

int32 ABozorHud::TanlovQayerda(const FVector2D& Nuqta) const
{
	for (int32 I = 0; I < TanlovJoylari.Num(); ++I)
		if (TanlovJoylari[I].IsInside(Nuqta)) return I;
	return INDEX_NONE;
}

// Bozor darvozasi belgisi: ekranda bo'lsa - darvoza ustida, orqada yoki chetda bo'lsa - ekran chetida, o'sha tomonda.
// Sariq romb va "Bozor · 35 m"
void ABozorHud::BelgiChiz()
{
	APlayerController* P = PlayerOwner.Get();
	if (!P || !P->PlayerCameraManager) return;
	const float S = Canvas->ClipY / 1080.f;
	const float A = BelgiAlfa;
	const FVector K = P->PlayerCameraManager->GetCameraLocation();
	const FVector Joy = bQaytish ? ChiqishJoyi : BozorJoyi;
	const FVector L = P->PlayerCameraManager->GetCameraRotation().UnrotateVector(Joy - K);   // X oldinga, Y o'ngga, Z yuqoriga

	const FVector2D Markaz(Canvas->ClipX * 0.5f, Canvas->ClipY * 0.5f);
	const float Chet = 70.f * S;
	FVector2D E;
	bool bEkranda = false;
	if (L.X > 1.f)
	{
		const FVector Pr = Project(Joy, false);   // Canvas koordinatasida
		E = FVector2D(Pr.X, Pr.Y);
		bEkranda = E.X > Chet && E.X < Canvas->ClipX - Chet && E.Y > Chet && E.Y < Canvas->ClipY - Chet;
	}
	if (!bEkranda)
	{
		// orqada yoki chetda: kamera tekisligidagi yo'nalish bo'yicha ekran chetiga
		FVector2D D(L.Y, -L.Z * 0.3f);
		if (D.IsNearlyZero()) D = FVector2D(1.f, 0.f);
		D.Normalize();
		const float Kx = (Canvas->ClipX * 0.5f - Chet) / FMath::Max(FMath::Abs(D.X), 0.001f);
		const float Ky = (Canvas->ClipY * 0.5f - Chet) / FMath::Max(FMath::Abs(D.Y), 0.001f);
		E = Markaz + D * FMath::Min(Kx, Ky);
	}

	// romb (45° burilgan kvadrat)
	const float R = 18.f * S;
	FCanvasTileItem Romb(E - FVector2D(R, R), FVector2D(2.f * R, 2.f * R), FLinearColor(1.f, 0.8f, 0.15f, 0.95f * A));
	Romb.BlendMode = SE_BLEND_Translucent;
	Romb.Rotation = FRotator(0.f, 45.f, 0.f);
	Romb.PivotPoint = FVector2D(0.5f, 0.5f);
	Canvas->DrawItem(Romb);
	FCanvasTileItem Ichi(E - FVector2D(R * 0.45f, R * 0.45f), FVector2D(R * 0.9f, R * 0.9f), FLinearColor(0.1f, 0.2f, 0.5f, 0.95f * A));
	Ichi.BlendMode = SE_BLEND_Translucent;
	Ichi.Rotation = FRotator(0.f, 45.f, 0.f);
	Ichi.PivotPoint = FVector2D(0.5f, 0.5f);
	Canvas->DrawItem(Ichi);

	const APawn* Pw = P->GetPawn();
	const int32 Metr = Pw ? FMath::RoundToInt(FVector::Dist2D(Pw->GetActorLocation(), Joy) / 100.f) : 0;
	FCanvasTextItem T(E + FVector2D(0.f, R + 22.f * S), FText::FromString(FString::Printf(TEXT("%s · %d m"), bQaytish ? TEXT("Chiqish") : TEXT("Bozor"), Metr)), GEngine->GetLargeFont(),
		FLinearColor(1.f, 1.f, 1.f, A));
	T.Scale = FVector2D(1.4f * S);
	T.bCentreX = true;
	T.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.8f * A));
	Canvas->DrawItem(T);
}
