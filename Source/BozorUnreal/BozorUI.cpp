#include "BozorUI.h"

#include "BozorSozlamalar.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/ScaleBox.h"
#include "Components/Slider.h"
#include "Components/Spacer.h"
#include "Styling/CoreStyle.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/DirectionalLight.h"
#include "Components/LightComponent.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/HUD.h"
#include "GameFramework/SpectatorPawn.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "EngineUtils.h"
#include "RHI.h"
#include "RenderCore.h"
#include "Misc/FileHelper.h"
#include "Framework/Application/SlateApplication.h"
#include "MediaPlayer.h"
#include "MediaTexture.h"
#include "FileMediaSource.h"
#include "UObject/UObjectIterator.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "HAL/FileManager.h"

#define LOCTEXT_NAMESPACE "BozorUI"

// ---------------------------------------------------------------- umumiy

namespace BozorUI
{
	const TCHAR* BoshMenyuXaritasi = TEXT("/Game/UI/Maps/BoshMenyu");
	const TCHAR* BozorXaritasi = TEXT("/Game/Bozor/Maps/Bozor");

	FLinearColor Kok() { return FLinearColor::FromSRGBColor(FColor(7, 66, 140)); }
	FLinearColor Sariq() { return FLinearColor::FromSRGBColor(FColor(246, 196, 38)); }

	FSlateFontInfo Shrift(int32 Olcham, const TCHAR* Turi, int32 Oraliq)
	{
		FSlateFontInfo F = FCoreStyle::GetDefaultFontStyle(Turi, Olcham);
		F.LetterSpacing = Oraliq;
		return F;
	}
}

namespace
{
	float Oraliq(float T, float A, float B) { return FMath::Clamp((T - A) / (B - A), 0.f, 1.f); }
	float ChiqishSilliq(float X) { X = FMath::Clamp(X, 0.f, 1.f); return 1.f - FMath::Pow(1.f - X, 3.f); }
	float IkkiTomonSilliq(float X) { X = FMath::Clamp(X, 0.f, 1.f); return X * X * (3.f - 2.f * X); }

	template <class T> T* Yasa(UWidgetTree* Wt) { return Wt->ConstructWidget<T>(T::StaticClass()); }

	UTextBlock* Yozuv(UWidgetTree* Wt, const FText& S, int32 Olcham, const FLinearColor& C, const TCHAR* Turi = TEXT("Bold"), int32 Bosh = 0, bool bSoya = true)
	{
		UTextBlock* T = Yasa<UTextBlock>(Wt);
		T->SetText(S);
		T->SetFont(BozorUI::Shrift(Olcham, Turi, Bosh));
		T->SetColorAndOpacity(FSlateColor(C));
		if (bSoya)
		{
			T->SetShadowOffset(FVector2D(0.f, 2.f));
			T->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.55f));
		}
		return T;
	}

	UImage* Rang(UWidgetTree* Wt, const FLinearColor& C, FVector2D Olcham = FVector2D::ZeroVector)
	{
		UImage* I = Yasa<UImage>(Wt);
		I->SetColorAndOpacity(C);
		if (!Olcham.IsZero()) { FSlateBrush B = I->GetBrush(); B.ImageSize = Olcham; I->SetBrush(B); }
		return I;
	}

	UImage* Rasm(UWidgetTree* Wt, UTexture2D* Tx, const FLinearColor& C = FLinearColor::White)
	{
		UImage* I = Yasa<UImage>(Wt);
		if (Tx) I->SetBrushFromTexture(Tx, true);
		I->SetColorAndOpacity(C);
		return I;
	}

	UOverlaySlot* Qosh(UOverlay* O, UWidget* W, EHorizontalAlignment H = HAlign_Fill, EVerticalAlignment V = VAlign_Fill, FMargin P = FMargin(0.f))
	{
		UOverlaySlot* S = O->AddChildToOverlay(W);
		S->SetHorizontalAlignment(H);
		S->SetVerticalAlignment(V);
		S->SetPadding(P);
		return S;
	}

	UVerticalBoxSlot* Qosh(UVerticalBox* B, UWidget* W, FMargin P = FMargin(0.f), EHorizontalAlignment H = HAlign_Fill)
	{
		UVerticalBoxSlot* S = B->AddChildToVerticalBox(W);
		S->SetPadding(P);
		S->SetHorizontalAlignment(H);
		return S;
	}

	UHorizontalBoxSlot* Qosh(UHorizontalBox* B, UWidget* W, FMargin P = FMargin(0.f), EVerticalAlignment V = VAlign_Center, bool bTold = false)
	{
		UHorizontalBoxSlot* S = B->AddChildToHorizontalBox(W);
		S->SetPadding(P);
		S->SetVerticalAlignment(V);
		if (bTold) S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		return S;
	}

	UTexture2D* Tekstura(const TCHAR* Yol) { return LoadObject<UTexture2D>(nullptr, Yol); }

	// Silliq qorong'ilik: N ta tasma, alfa = C.A * (1-t)^Daraja. bGorizontal - chapdan o'ngga, aks holda yuqoridan pastga;
	// bTeskari - yo'nalish teskari (o'ngdan / pastdan qorong'i). Tekstura kerak emas.
	UWidget* Gradient(UWidgetTree* Wt, const FLinearColor& C, bool bGorizontal, bool bTeskari = false, int32 N = 64, float Daraja = 1.6f)
	{
		auto Alfa = [&](int32 I)
		{
			float T = (I + 0.5f) / N;
			if (bTeskari) T = 1.f - T;
			return C.A * FMath::Pow(1.f - T, Daraja);
		};
		if (bGorizontal)
		{
			UHorizontalBox* B = Yasa<UHorizontalBox>(Wt);
			for (int32 I = 0; I < N; ++I) Qosh(B, Rang(Wt, FLinearColor(C.R, C.G, C.B, Alfa(I))), FMargin(0.f), VAlign_Fill, true);
			return B;
		}
		UVerticalBox* B = Yasa<UVerticalBox>(Wt);
		for (int32 I = 0; I < N; ++I) Qosh(B, Rang(Wt, FLinearColor(C.R, C.G, C.B, Alfa(I))))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		return B;
	}

	// Yumshoq dumaloq nur: ichma-ich shaffof doiralar
	UWidget* Nur(UWidgetTree* Wt, const FLinearColor& C, float Diametr, int32 N = 18)
	{
		UOverlay* O = Yasa<UOverlay>(Wt);
		for (int32 I = 0; I < N; ++I)
		{
			const float D = Diametr * (1.f - (float)I / N);
			UImage* Im = Yasa<UImage>(Wt);
			Im->SetBrush(FSlateRoundedBoxBrush(FLinearColor(C.R, C.G, C.B, C.A / N * 1.8f), D * 0.5f, FVector2f(D, D)));
			Qosh(O, Im, HAlign_Center, VAlign_Center);
		}
		return O;
	}

	// sinov rejimlari - menyusiz to'g'ridan-to'g'ri bozorga
	bool MenyusizMi()
	{
		const TCHAR* Cmd = FCommandLine::Get();
		static const TCHAR* Bayroqlar[] = { TEXT("MenyuYoq"), TEXT("unattended"), TEXT("AvtobusTest"), TEXT("SotuvTest"), TEXT("BozorAutoTest"),
			TEXT("BozorQadamTest"), TEXT("DarajaTest"), TEXT("MenyuFonOl") };
		for (const TCHAR* B : Bayroqlar) if (FParse::Param(Cmd, B)) return true;
		return false;
	}

	void OyinUchunKirish(APlayerController* Pc)
	{
		if (!Pc) return;
		Pc->SetInputMode(FInputModeGameOnly());
		Pc->SetShowMouseCursor(false);
	}

	void UIUchunKirish(APlayerController* Pc, UUserWidget* W)
	{
		if (!Pc) return;
		FInputModeUIOnly M;
		if (W) M.SetWidgetToFocus(W->TakeWidget());
		M.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Pc->SetInputMode(M);
		Pc->SetShowMouseCursor(true);
	}
}

// ---------------------------------------------------------------- tugma

void UBozorTugma::Sozla(const FText& Matn, int32 Olcham, bool bKichik)
{
	Matn0 = Matn;
	Olcham0 = Olcham;
	bKichik0 = bKichik;
	if (Yozuv) Yozuv->SetText(Matn);
}

void UBozorTugma::SetMatn(const FText& Matn, const FLinearColor& Rang)
{
	Matn0 = Matn;
	if (Yozuv)
	{
		Yozuv->SetText(Matn);
		Yozuv->SetColorAndOpacity(FSlateColor(Rang));
	}
}

TSharedRef<SWidget> UBozorTugma::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		Btn = Yasa<UButton>(WidgetTree);
		FSlateBrush Bosh;
		Bosh.DrawAs = ESlateBrushDrawType::NoDrawType;
		FButtonStyle St;
		St.SetNormal(Bosh).SetHovered(Bosh).SetPressed(Bosh).SetDisabled(Bosh);
		St.SetNormalPadding(FMargin(0.f)).SetPressedPadding(FMargin(0.f));
		Btn->SetStyle(St);

		UOverlay* Ov = Yasa<UOverlay>(WidgetTree);
		Fon = Rang(WidgetTree, bKichik0 ? FLinearColor(1.f, 1.f, 1.f, 1.f) : BozorUI::Kok());
		Fon->SetRenderOpacity(bKichik0 ? 0.08f : 0.f);
		Qosh(Ov, Fon);

		UHorizontalBox* Hb = Yasa<UHorizontalBox>(WidgetTree);
		Chiziq = Rang(WidgetTree, BozorUI::Sariq(), FVector2D(6.f, Olcham0 * 1.55f));
		Chiziq->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		Chiziq->SetRenderScale(FVector2D(1.f, 0.f));
		if (!bKichik0) Qosh(Hb, Chiziq, FMargin(0.f), VAlign_Fill);
		Yozuv = ::Yozuv(WidgetTree, Matn0, Olcham0, FLinearColor(1.f, 1.f, 1.f, bKichik0 ? 1.f : 0.86f), bKichik0 ? TEXT("Regular") : TEXT("Bold"), bKichik0 ? 0 : 60);
		Qosh(Hb, Yozuv, bKichik0 ? FMargin(18.f, 8.f) : FMargin(22.f, 12.f, 30.f, 12.f), VAlign_Center, true);
		if (bKichik0) Yozuv->SetJustification(ETextJustify::Center);
		Qosh(Ov, Hb);

		if (UButtonSlot* S = Cast<UButtonSlot>(Btn->AddChild(Ov)))
		{
			S->SetPadding(FMargin(0.f));
			S->SetHorizontalAlignment(HAlign_Fill);
			S->SetVerticalAlignment(VAlign_Fill);
		}
		WidgetTree->RootWidget = Btn;
		Btn->OnClicked.AddDynamic(this, &UBozorTugma::OnBos);
		Btn->OnHovered.AddDynamic(this, &UBozorTugma::OnUst);
		Btn->OnUnhovered.AddDynamic(this, &UBozorTugma::OnKet);
	}
	return Super::RebuildWidget();
}

void UBozorTugma::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const float Maqsad = (bTanlangan || bUstida) ? 1.f : 0.f;
	Holat = FMath::FInterpTo(Holat, Maqsad, InDeltaTime, 14.f);
	if (bKichik0)
	{
		if (Fon) Fon->SetRenderOpacity(0.08f + 0.14f * Holat);
		return;
	}
	const float E = ChiqishSilliq(Holat);
	if (Chiziq) Chiziq->SetRenderScale(FVector2D(1.f, E));
	if (Yozuv)
	{
		Yozuv->SetRenderTranslation(FVector2D(10.f * E, 0.f));
		Yozuv->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.82f + 0.18f * E)));
	}
	if (Fon) Fon->SetRenderOpacity(0.72f * E);
}

void UBozorTugma::OnBos() { if (Bosildi) Bosildi(); }
void UBozorTugma::OnUst() { bUstida = true; if (UstigaKeldi) UstigaKeldi(); }
void UBozorTugma::OnKet() { bUstida = false; }

// ---------------------------------------------------------------- splash

namespace
{
	// vaqt jadvali (soniya)
	constexpr float LogoTugashi = 4.6f;      // 1-logo (TRK) shu paytgacha
	constexpr float NomTugashi = 8.6f;       // 2-logo (o'yin nomi)
	constexpr float BelgiBalandligi = 230.f;
	// T_Logo_Matn 1025x480, T_Logo_Toliq 1540x480 (tekstura yuklanayotganda GetSizeX 32 qaytarishi mumkin - shuning uchun doimiy)
	constexpr float MatnNisbati = 1025.f / 480.f;
}

TSharedRef<SWidget> UBozorSplash::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget) Qur();
	return Super::RebuildWidget();
}

void UBozorSplash::Qur()
{
	SetIsFocusable(true);
	TxBelgi = Tekstura(TEXT("/Game/UI/Logo/T_Logo_Belgi.T_Logo_Belgi"));
	TxMatn = Tekstura(TEXT("/Game/UI/Logo/T_Logo_Matn.T_Logo_Matn"));

	UOverlay* Ov = Yasa<UOverlay>(WidgetTree);
	WidgetTree->RootWidget = Ov;
	Qosh(Ov, Rang(WidgetTree, FLinearColor::Black));

	// orqadagi ko'k nur (kompaniya rangi)
	Porlash = Nur(WidgetTree, FLinearColor::FromSRGBColor(FColor(20, 95, 190)), 1000.f, 40);
	Porlash->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	Porlash->SetRenderOpacity(0.f);
	Qosh(Ov, Porlash, HAlign_Center, VAlign_Center);

	// 1-logo: belgi (pastdan yuqoriga ochiladi) + matn (chapdan o'ngga ochiladi) + sariq chiziq
	LogoGuruh = Yasa<UVerticalBox>(WidgetTree);
	LogoGuruh->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	UHorizontalBox* Hb = Yasa<UHorizontalBox>(WidgetTree);

	USizeBox* BelgiQuti = Yasa<USizeBox>(WidgetTree);
	BelgiQuti->SetWidthOverride(BelgiBalandligi);
	BelgiQuti->SetHeightOverride(BelgiBalandligi);
	Belgi = Rasm(WidgetTree, TxBelgi);
	BelgiQuti->AddChild(Belgi);
	if (UPanelSlot* S = Belgi->Slot) if (USizeBoxSlot* Ss = Cast<USizeBoxSlot>(S)) { Ss->SetVerticalAlignment(VAlign_Bottom); Ss->SetHorizontalAlignment(HAlign_Fill); }
	Qosh(Hb, BelgiQuti, FMargin(0.f), VAlign_Bottom);

	const float MatnKenglik = BelgiBalandligi * MatnNisbati;
	USizeBox* MatnQuti = Yasa<USizeBox>(WidgetTree);
	MatnQuti->SetWidthOverride(MatnKenglik);
	MatnQuti->SetHeightOverride(BelgiBalandligi);
	Matn = Rasm(WidgetTree, TxMatn);
	MatnQuti->AddChild(Matn);
	if (USizeBoxSlot* Ss = Cast<USizeBoxSlot>(Matn->Slot)) { Ss->SetHorizontalAlignment(HAlign_Left); Ss->SetVerticalAlignment(VAlign_Fill); }
	Qosh(Hb, MatnQuti, FMargin(46.f, 0.f, 0.f, 0.f), VAlign_Bottom);
	Qosh(LogoGuruh, Hb);

	Chiziq = Rang(WidgetTree, BozorUI::Sariq(), FVector2D(BelgiBalandligi + 46.f + MatnKenglik, 5.f));
	Chiziq->SetRenderTransformPivot(FVector2D(0.f, 0.5f));
	Chiziq->SetRenderScale(FVector2D(0.f, 1.f));
	Qosh(LogoGuruh, Chiziq, FMargin(0.f, 34.f, 0.f, 0.f), HAlign_Left);
	Qosh(Ov, LogoGuruh, HAlign_Center, VAlign_Center);

	// 2-logo: o'yin nomi
	UVerticalBox* NomGuruh = Yasa<UVerticalBox>(WidgetTree);
	Nom = Yozuv(WidgetTree, LOCTEXT("OyinNomi", "SAVDOGAR"), 96, FLinearColor::White, TEXT("Bold"), 900);
	Nom->SetJustification(ETextJustify::Center);
	Nom->SetRenderOpacity(0.f);
	Qosh(NomGuruh, Nom, FMargin(0.f), HAlign_Center);
	NomChiziq = Rang(WidgetTree, BozorUI::Sariq(), FVector2D(420.f, 5.f));
	NomChiziq->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	NomChiziq->SetRenderScale(FVector2D(0.f, 1.f));
	Qosh(NomGuruh, NomChiziq, FMargin(0.f, 18.f, 0.f, 22.f), HAlign_Center);
	Shior = Yozuv(WidgetTree, LOCTEXT("Shior", "BOZORGA XUSH KELIBSIZ!"), 24, BozorUI::Sariq(), TEXT("Bold"), 420);
	Shior->SetRenderOpacity(0.f);
	Qosh(NomGuruh, Shior, FMargin(0.f), HAlign_Center);
	Qosh(Ov, NomGuruh, HAlign_Center, VAlign_Center);

	Maslahat = Yozuv(WidgetTree, LOCTEXT("Otkazish", "O'tkazib yuborish uchun istalgan tugmani bosing"), 15, FLinearColor(1.f, 1.f, 1.f, 0.35f), TEXT("Regular"), 60, false);
	Maslahat->SetRenderOpacity(0.f);
	Qosh(Ov, Maslahat, HAlign_Right, VAlign_Bottom, FMargin(0.f, 0.f, 48.f, 36.f));

	// boshlang'ich holat
	NativeTick(FGeometry(), 0.f);
}

void UBozorSplash::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (bTugadi || !Belgi) return;
	T += FMath::Min(InDeltaTime, 0.05f);

	// --- 1-logo
	const float NurA = ChiqishSilliq(Oraliq(T, 0.2f, 1.4f)) * (1.f - IkkiTomonSilliq(Oraliq(T, 3.8f, 4.5f)));
	Porlash->SetRenderOpacity(0.55f * NurA);
	Porlash->SetRenderScale(FVector2D(1.25f + 0.35f * Oraliq(T, 0.f, LogoTugashi)));

	// belgi pastdan yuqoriga "o'sadi" (UV bilan kesib - cho'zilmaydi)
	const float Pb = ChiqishSilliq(Oraliq(T, 0.45f, 1.35f));
	if (TxBelgi)
	{
		FSlateBrush B;
		B.SetResourceObject(TxBelgi);
		B.ImageSize = FVector2D(BelgiBalandligi, BelgiBalandligi * Pb);
		B.SetUVRegion(FBox2f(FVector2f(0.f, 1.f - Pb), FVector2f(1.f, 1.f)));
		Belgi->SetBrush(B);
	}
	Belgi->SetRenderOpacity(Oraliq(T, 0.45f, 0.8f));

	// matn chapdan o'ngga ochiladi va biroz suriladi
	const float Pm = IkkiTomonSilliq(Oraliq(T, 1.05f, 2.0f));
	if (TxMatn)
	{
		const float H = BelgiBalandligi;
		const float W = H * MatnNisbati;
		FSlateBrush B;
		B.SetResourceObject(TxMatn);
		B.ImageSize = FVector2D(W * Pm, H);
		B.SetUVRegion(FBox2f(FVector2f(0.f, 0.f), FVector2f(Pm, 1.f)));
		Matn->SetBrush(B);
	}
	Matn->SetRenderTranslation(FVector2D(-24.f * (1.f - Pm), 0.f));
	Matn->SetRenderOpacity(Oraliq(T, 1.05f, 1.4f));

	Chiziq->SetRenderScale(FVector2D(ChiqishSilliq(Oraliq(T, 1.8f, 2.6f)), 1.f));

	const float Ket = IkkiTomonSilliq(Oraliq(T, 3.85f, 4.45f));
	LogoGuruh->SetRenderOpacity(1.f - Ket);
	LogoGuruh->SetRenderScale(FVector2D(1.f + 0.035f * Oraliq(T, 0.4f, 4.45f)));
	LogoGuruh->SetVisibility(T < LogoTugashi ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	// --- 2-logo: o'yin nomi (harflar oralig'i torayib yig'iladi)
	const float N0 = LogoTugashi + 0.2f;
	const float Pn = ChiqishSilliq(Oraliq(T, N0, N0 + 1.4f));
	const float NomKet = IkkiTomonSilliq(Oraliq(T, NomTugashi - 0.9f, NomTugashi - 0.2f));
	Nom->SetFont(BozorUI::Shrift(96, TEXT("Bold"), FMath::RoundToInt(FMath::Lerp(900.f, 180.f, Pn))));
	Nom->SetRenderOpacity(Oraliq(T, N0, N0 + 0.8f) * (1.f - NomKet));
	NomChiziq->SetRenderScale(FVector2D(ChiqishSilliq(Oraliq(T, N0 + 0.9f, N0 + 1.6f)), 1.f));
	NomChiziq->SetRenderOpacity(1.f - NomKet);
	Shior->SetRenderOpacity(Oraliq(T, N0 + 1.3f, N0 + 1.9f) * (1.f - NomKet));
	Shior->SetRenderTranslation(FVector2D(0.f, 10.f * (1.f - ChiqishSilliq(Oraliq(T, N0 + 1.3f, N0 + 1.9f)))));

	Maslahat->SetRenderOpacity(Oraliq(T, 1.5f, 2.2f) * (1.f - NomKet));

	if (T >= NomTugashi)
	{
		bTugadi = true;
		if (Tugadi) Tugadi();
	}
}

void UBozorSplash::OtkazibYubor()
{
	// 1-logo paytida - nomga o'tadi, nom paytida - tugaydi
	if (T < LogoTugashi - 0.6f) T = LogoTugashi - 0.6f;
	else if (T < NomTugashi - 0.9f) T = NomTugashi - 0.9f;
}

FReply UBozorSplash::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	OtkazibYubor();
	return FReply::Handled();
}

FReply UBozorSplash::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	OtkazibYubor();
	return FReply::Handled();
}

// ---------------------------------------------------------------- sozlamalar

namespace
{
	FText SifatNomi(int32 L)
	{
		const FText N[] = { LOCTEXT("Past", "Past"), LOCTEXT("Orta", "O'rta"), LOCTEXT("Yuqori", "Yuqori"), LOCTEXT("EngYuqori", "Eng yuqori"), LOCTEXT("Kino", "Kino") };
		return N[FMath::Clamp(L, 0, 4)];
	}

	FText EkranNomi(EWindowMode::Type M)
	{
		switch (M)
		{
		case EWindowMode::Fullscreen: return LOCTEXT("ToliqEkran", "To'liq ekran");
		case EWindowMode::WindowedFullscreen: return LOCTEXT("OynaliToliq", "Ramkasiz to'liq ekran");
		default: return LOCTEXT("Oyna", "Oyna");
		}
	}
}

void UBozorSozlamalarOyna::Ochildi()
{
	Kutilayotgan = NAME_None;
	OchilishT = 0.f;
	Yangila();
	SetKeyboardFocus();
}

TSharedRef<SWidget> UBozorSozlamalarOyna::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget) Qur();
	return Super::RebuildWidget();
}

void UBozorSozlamalarOyna::Qur()
{
	SetIsFocusable(true);
	UBozorSozlamalar* Sz = UBozorSozlamalar::Get();

	UOverlay* Ov = Yasa<UOverlay>(WidgetTree);
	WidgetTree->RootWidget = Ov;
	Qosh(Ov, Rang(WidgetTree, FLinearColor(0.f, 0.f, 0.f, 0.6f)));

	USizeBox* Quti = Yasa<USizeBox>(WidgetTree);
	Quti->SetWidthOverride(1000.f);
	Quti->SetMaxDesiredHeight(860.f);
	Panel = Yasa<UBorder>(WidgetTree);
	Panel->SetBrush(FSlateRoundedBoxBrush(FLinearColor(0.012f, 0.022f, 0.045f, 0.96f), 10.f, FLinearColor(1.f, 1.f, 1.f, 0.08f), 1.f));
	Panel->SetPadding(FMargin(48.f, 40.f));
	Quti->AddChild(Panel);
	Qosh(Ov, Quti, HAlign_Center, VAlign_Center, FMargin(24.f));

	UVerticalBox* Ic = Yasa<UVerticalBox>(WidgetTree);
	Panel->SetContent(Ic);

	// sarlavha
	Qosh(Ic, Yozuv(WidgetTree, LOCTEXT("Sozlamalar", "SOZLAMALAR"), 40, FLinearColor::White, TEXT("Bold"), 160));
	Qosh(Ic, Rang(WidgetTree, BozorUI::Sariq(), FVector2D(90.f, 5.f)), FMargin(0.f, 10.f, 0.f, 22.f), HAlign_Left);

	UScrollBox* Sb = Yasa<UScrollBox>(WidgetTree);
	Sb->SetScrollBarVisibility(ESlateVisibility::Visible);
	UVerticalBoxSlot* SbSlot = Qosh(Ic, Sb);
	SbSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	UVerticalBox* Ro = Yasa<UVerticalBox>(WidgetTree);
	Sb->AddChild(Ro);

	auto Bolim = [this, Ro](const FText& Nom)
	{
		Qosh(Ro, Yozuv(WidgetTree, Nom, 17, BozorUI::Sariq(), TEXT("Bold"), 300, false), FMargin(0.f, 22.f, 0.f, 8.f));
		Qosh(Ro, Rang(WidgetTree, FLinearColor(1.f, 1.f, 1.f, 0.1f), FVector2D(10.f, 1.f)), FMargin(0.f, 0.f, 0.f, 6.f));
	};
	auto Qator = [this, Ro](const FText& Nom, UWidget* Boshqaruv)
	{
		UHorizontalBox* H = Yasa<UHorizontalBox>(WidgetTree);
		Qosh(H, Yozuv(WidgetTree, Nom, 22, FLinearColor(1.f, 1.f, 1.f, 0.88f), TEXT("Regular"), 0, false), FMargin(0.f), VAlign_Center, true);
		USizeBox* Qt = Yasa<USizeBox>(WidgetTree);
		Qt->SetWidthOverride(380.f);
		Qt->SetHeightOverride(52.f);
		Qt->AddChild(Boshqaruv);
		Qosh(H, Qt, FMargin(16.f, 0.f, 8.f, 0.f));
		Qosh(Ro, H, FMargin(0.f, 5.f));
	};
	auto KichikTugma = [this](const FText& M)
	{
		UBozorTugma* T = CreateWidget<UBozorTugma>(this, UBozorTugma::StaticClass());
		T->Sozla(M, 20, true);
		return T;
	};

	// --- boshqaruv
	Bolim(LOCTEXT("Boshqaruv", "SICHQONCHA"));
	{
		UHorizontalBox* H = Yasa<UHorizontalBox>(WidgetTree);
		Sezgirlik = Yasa<USlider>(WidgetTree);
		Sezgirlik->SetMinValue(0.1f);
		Sezgirlik->SetMaxValue(3.f);
		Sezgirlik->SetStepSize(0.05f);
		Sezgirlik->SetValue(Sz->Sezgirlik);
		Sezgirlik->SetSliderBarColor(FLinearColor(1.f, 1.f, 1.f, 0.25f));
		Sezgirlik->SetSliderHandleColor(BozorUI::Sariq());
		Sezgirlik->OnValueChanged.AddDynamic(this, &UBozorSozlamalarOyna::SezgirlikOzgardi);
		Qosh(H, Sezgirlik, FMargin(0.f), VAlign_Center, true);
		SezgirlikQiymat = Yozuv(WidgetTree, FText::GetEmpty(), 22, FLinearColor::White, TEXT("Bold"), 0, false);
		SezgirlikQiymat->SetMinDesiredWidth(64.f);
		SezgirlikQiymat->SetJustification(ETextJustify::Right);
		Qosh(H, SezgirlikQiymat, FMargin(14.f, 0.f, 0.f, 0.f));
		Qator(LOCTEXT("Sezgirlik", "Sichqoncha sezgirligi"), H);
	}
	TeskariY = KichikTugma(FText::GetEmpty());
	TeskariY->Bosildi = [this]()
	{
		UBozorSozlamalar* S = UBozorSozlamalar::Get();
		S->bTeskariY = !S->bTeskariY;
		Qollash();
	};
	Qator(LOCTEXT("TeskariY", "Yuqori-pastga qarashni teskari qilish"), TeskariY);

	// --- grafika
	Bolim(LOCTEXT("Grafika", "GRAFIKA"));
	Sifat = KichikTugma(FText::GetEmpty());
	Sifat->Bosildi = [this]()
	{
		if (UGameUserSettings* G = GEngine ? GEngine->GetGameUserSettings() : nullptr)
		{
			int32 L = G->GetOverallScalabilityLevel();
			L = (L < 0 || L >= 3) ? 0 : L + 1;    // Past -> O'rta -> Yuqori -> Eng yuqori -> Past
			G->SetOverallScalabilityLevel(L);
			G->ApplySettings(false);
			G->SaveSettings();
		}
		Yangila();
	};
	Qator(LOCTEXT("Sifat", "Grafika sifati"), Sifat);

	EkranRejimi = KichikTugma(FText::GetEmpty());
	EkranRejimi->Bosildi = [this]()
	{
		if (UGameUserSettings* G = GEngine ? GEngine->GetGameUserSettings() : nullptr)
		{
			const EWindowMode::Type M = G->GetFullscreenMode();
			const EWindowMode::Type Yangi = M == EWindowMode::WindowedFullscreen ? EWindowMode::Windowed
				: M == EWindowMode::Windowed ? EWindowMode::Fullscreen : EWindowMode::WindowedFullscreen;
			G->SetFullscreenMode(Yangi);
			if (Yangi != EWindowMode::Windowed) G->SetScreenResolution(G->GetDesktopResolution());
			G->ApplyResolutionSettings(false);
			G->SaveSettings();
		}
		Yangila();
	};
	Qator(LOCTEXT("Ekran", "Ekran rejimi"), EkranRejimi);

	// --- tugmalar
	Bolim(LOCTEXT("Tugmalar", "KLAVIATURA TUGMALARI"));
	for (const FBozorAmal& A : UBozorSozlamalar::Amallar())
	{
		UBozorTugma* T = KichikTugma(FText::GetEmpty());
		const FName Id = A.Id;
		T->Bosildi = [this, Id]()
		{
			Kutilayotgan = (Kutilayotgan == Id) ? NAME_None : Id;
			SetKeyboardFocus();
			Yangila();
		};
		TugmaQatorlar.Add(T);
		Qator(A.Nomi, T);
	}

	// pastki qator
	Izoh = Yozuv(WidgetTree, FText::GetEmpty(), 17, FLinearColor(1.f, 1.f, 1.f, 0.5f), TEXT("Regular"), 0, false);
	Qosh(Ic, Izoh, FMargin(0.f, 16.f, 0.f, 0.f));
	UHorizontalBox* Past = Yasa<UHorizontalBox>(WidgetTree);
	UBozorTugma* Standart = KichikTugma(LOCTEXT("Standart", "Standart sozlamalar"));
	Standart->Bosildi = [this]()
	{
		Kutilayotgan = NAME_None;
		UBozorSozlamalar::Get()->StandartgaQaytar();
		if (Sezgirlik) Sezgirlik->SetValue(UBozorSozlamalar::Get()->Sezgirlik);
		Qollash();
	};
	Qosh(Past, Standart, FMargin(0.f), VAlign_Center);
	Qosh(Past, Yasa<USpacer>(WidgetTree), FMargin(0.f), VAlign_Center, true);
	UBozorTugma* Orqaga = KichikTugma(LOCTEXT("Orqaga", "Orqaga  (Esc)"));
	Orqaga->Bosildi = [this]() { Yop(); };
	Qosh(Past, Orqaga, FMargin(0.f), VAlign_Center);
	Qosh(Ic, Past, FMargin(0.f, 14.f, 0.f, 0.f));

	Yangila();
}

void UBozorSozlamalarOyna::Yangila()
{
	UBozorSozlamalar* Sz = UBozorSozlamalar::Get();
	FNumberFormattingOptions Nf;
	Nf.SetMinimumFractionalDigits(2).SetMaximumFractionalDigits(2);
	if (SezgirlikQiymat) SezgirlikQiymat->SetText(FText::AsNumber(Sz->Sezgirlik, &Nf));
	if (TeskariY) TeskariY->SetMatn(Sz->bTeskariY ? LOCTEXT("Ha", "Ha") : LOCTEXT("Yoq", "Yo'q"));
	if (UGameUserSettings* G = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		const int32 L = G->GetOverallScalabilityLevel();
		if (Sifat) Sifat->SetMatn(L >= 0 && L <= 4 ? SifatNomi(L) : LOCTEXT("Maxsus", "Maxsus"));
		if (EkranRejimi) EkranRejimi->SetMatn(EkranNomi(G->GetFullscreenMode()));
	}
	const TArray<FBozorAmal>& Am = UBozorSozlamalar::Amallar();
	for (int32 I = 0; I < TugmaQatorlar.Num() && I < Am.Num(); ++I)
	{
		if (Kutilayotgan == Am[I].Id)
			TugmaQatorlar[I]->SetMatn(LOCTEXT("Kutish", "Yangi tugmani bosing..."), BozorUI::Sariq());
		else
		{
			const FKey K = Sz->Tugma(Am[I].Id);
			const bool bOzgargan = K != Am[I].Standart;
			TugmaQatorlar[I]->SetMatn(UBozorSozlamalar::TugmaNomi(K), bOzgargan ? FLinearColor(0.75f, 0.9f, 1.f) : FLinearColor::White);
		}
	}
	if (Izoh)
		Izoh->SetText(Kutilayotgan.IsNone()
			? LOCTEXT("IzohOddiy", "Tugmani o'zgartirish uchun ustiga bosing. Sozlamalar avtomatik saqlanadi.")
			: LOCTEXT("IzohKutish", "Klaviaturadagi tugmani bosing. Bekor qilish - Esc."));
}

void UBozorSozlamalarOyna::SezgirlikOzgardi(float V)
{
	UBozorSozlamalar::Get()->Sezgirlik = FMath::RoundToFloat(V * 20.f) / 20.f;
	Qollash();
}

void UBozorSozlamalarOyna::Qollash()
{
	UBozorSozlamalar::Get()->Saqla();
	UBozorSozlamalar::OyinchigaQollash(GetOwningPlayer());
	Yangila();
}

void UBozorSozlamalarOyna::TugmaUshla(const FKey& K)
{
	if (K == EKeys::Escape)
	{
		Kutilayotgan = NAME_None;
		Yangila();
		return;
	}
	if (!UBozorSozlamalar::TayinlashMumkin(K))
	{
		if (Izoh) Izoh->SetText(FText::Format(LOCTEXT("Band", "\"{0}\" tugmasi band - boshqasini tanlang."), K.GetDisplayName()));
		return;
	}
	UBozorSozlamalar::Get()->TugmaQoy(Kutilayotgan, K);
	Kutilayotgan = NAME_None;
	Qollash();
}

FReply UBozorSozlamalarOyna::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (!Kutilayotgan.IsNone())
	{
		TugmaUshla(InKeyEvent.GetKey());
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

FReply UBozorSozlamalarOyna::NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!Kutilayotgan.IsNone())
	{
		const FKey K = InMouseEvent.GetEffectingButton();
		if (K == EKeys::LeftMouseButton)
		{
			// chap tugma - bekor qilish (boshqa qatorni bosish ham shu bilan ishlaydi)
			Kutilayotgan = NAME_None;
			Yangila();
			return FReply::Unhandled();
		}
		TugmaUshla(K);
		return FReply::Handled();
	}
	return Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UBozorSozlamalarOyna::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape || InKeyEvent.GetKey() == EKeys::BackSpace)
	{
		Yop();
		return FReply::Handled();
	}
	return FReply::Handled();
}

void UBozorSozlamalarOyna::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	OchilishT += InDeltaTime;
	const float P = ChiqishSilliq(Oraliq(OchilishT, 0.f, 0.3f));
	if (Panel)
	{
		Panel->SetRenderOpacity(P);
		Panel->SetRenderTranslation(FVector2D(0.f, 24.f * (1.f - P)));
	}
}

void UBozorSozlamalarOyna::Yop()
{
	Kutilayotgan = NAME_None;
	UBozorSozlamalar::Get()->Saqla();
	if (Yopildi) Yopildi();
}

// ---------------------------------------------------------------- bosh menyu

TSharedRef<SWidget> UBozorBoshMenyu::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget) Qur();
	return Super::RebuildWidget();
}

void UBozorBoshMenyu::Qur()
{
	SetIsFocusable(true);
	UOverlay* Ov = Yasa<UOverlay>(WidgetTree);
	WidgetTree->RootWidget = Ov;
	Qosh(Ov, Rang(WidgetTree, FLinearColor(0.01f, 0.02f, 0.04f)));

	// fon: bozor darvozasining kino kadri, ekranni to'liq qoplaydi va juda sekin yaqinlashadi
	UScaleBox* Sb = Yasa<UScaleBox>(WidgetTree);
	Sb->SetStretch(EStretch::ScaleToFill);
	FonRasm = Rasm(WidgetTree, Tekstura(TEXT("/Game/UI/Fon/T_Menyu_Fon.T_Menyu_Fon")));
	{ FSlateBrush B = FonRasm->GetBrush(); B.ImageSize = FVector2D(1920.f, 1080.f); FonRasm->SetBrush(B); }   // 16:9 - tekstura hali yuklanmagan bo'lsa ham
	FonRasm->SetRenderTransformPivot(FVector2D(0.5f, 0.45f));
	Sb->AddChild(FonRasm);
	Qosh(Ov, Sb);

	// jonli fon: video bo'lsa, rasm ustida ochiladi (birinchi kadr kelgach sekin paydo bo'ladi)
	const FString VideoYol = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("Movies/MenyuFon.mp4"));
	if (FPaths::FileExists(VideoYol) && !FParse::Param(FCommandLine::Get(), TEXT("VideoYoq")))
	{
		Pleer = NewObject<UMediaPlayer>(this);
		Pleer->SetLooping(true);
		Pleer->PlayOnOpen = true;
		Pleer->SetDesiredPlayerName(TEXT("ElectraPlayer"));   // WMF bilan ijro qotib qolardi
		VideoTekstura = NewObject<UMediaTexture>(this);
		VideoTekstura->AutoClear = true;
		VideoTekstura->ClearColor = FLinearColor::Black;
		VideoTekstura->SetMediaPlayer(Pleer);
		VideoTekstura->UpdateResource();
		VideoManba = NewObject<UFileMediaSource>(this);
		VideoManba->SetFilePath(VideoYol);
		Pleer->OpenSource(VideoManba);

		UScaleBox* Vb = Yasa<UScaleBox>(WidgetTree);
		Vb->SetStretch(EStretch::ScaleToFill);
		FonVideo = Yasa<UImage>(WidgetTree);
		FSlateBrush B;
		B.SetResourceObject(VideoTekstura);
		B.ImageSize = FVector2D(1920.f, 1080.f);
		FonVideo->SetBrush(B);
		FonVideo->SetRenderTransformPivot(FVector2D(0.5f, 0.45f));
		FonVideo->SetRenderOpacity(0.f);
		Vb->AddChild(FonVideo);
		Qosh(Ov, Vb);
	}

	// o'qilishi uchun: chapdan qorong'i gradient va chetlar
	USizeBox* GrQuti = Yasa<USizeBox>(WidgetTree);
	GrQuti->SetWidthOverride(1300.f);
	GrQuti->AddChild(Gradient(WidgetTree, FLinearColor(0.f, 0.012f, 0.035f, 0.94f), true, false, 72, 1.5f));
	if (USizeBoxSlot* S = Cast<USizeBoxSlot>(GrQuti->GetContent()->Slot)) { S->SetHorizontalAlignment(HAlign_Fill); S->SetVerticalAlignment(VAlign_Fill); }
	Qosh(Ov, GrQuti, HAlign_Left, VAlign_Fill);
	// pastdan va tepadan yengil qorong'ilik
	USizeBox* Past = Yasa<USizeBox>(WidgetTree);
	Past->SetHeightOverride(380.f);
	Past->AddChild(Gradient(WidgetTree, FLinearColor(0.f, 0.f, 0.f, 0.7f), false, true, 48, 1.8f));
	if (USizeBoxSlot* S = Cast<USizeBoxSlot>(Past->GetContent()->Slot)) { S->SetHorizontalAlignment(HAlign_Fill); S->SetVerticalAlignment(VAlign_Fill); }
	Qosh(Ov, Past, HAlign_Fill, VAlign_Bottom);
	USizeBox* Tepa = Yasa<USizeBox>(WidgetTree);
	Tepa->SetHeightOverride(220.f);
	Tepa->AddChild(Gradient(WidgetTree, FLinearColor(0.f, 0.f, 0.f, 0.4f), false, false, 32, 1.8f));
	if (USizeBoxSlot* S = Cast<USizeBoxSlot>(Tepa->GetContent()->Slot)) { S->SetHorizontalAlignment(HAlign_Fill); S->SetVerticalAlignment(VAlign_Fill); }
	Qosh(Ov, Tepa, HAlign_Fill, VAlign_Top);

	// chap ustun: shior, nom plitasi, tugmalar
	Chap = Yasa<UVerticalBox>(WidgetTree);
	Qosh(Chap, Yozuv(WidgetTree, LOCTEXT("MenyuShior", "BOZORGA XUSH KELIBSIZ!"), 20, BozorUI::Sariq(), TEXT("Bold"), 380), FMargin(4.f, 0.f, 0.f, 14.f), HAlign_Left);

	UVerticalBox* Plita = Yasa<UVerticalBox>(WidgetTree);
	UBorder* PlitaFon = Yasa<UBorder>(WidgetTree);
	PlitaFon->SetBrush(FSlateRoundedBoxBrush(BozorUI::Kok(), 4.f));
	PlitaFon->SetPadding(FMargin(34.f, 14.f, 34.f, 10.f));
	PlitaFon->SetContent(Yozuv(WidgetTree, LOCTEXT("MenyuNom", "SAVDOGAR"), 74, FLinearColor::White, TEXT("Bold"), 120));
	Qosh(Plita, PlitaFon);
	Qosh(Plita, Rang(WidgetTree, BozorUI::Sariq(), FVector2D(10.f, 7.f)));
	Qosh(Chap, Plita, FMargin(0.f), HAlign_Left);

	Qosh(Chap, Yasa<USpacer>(WidgetTree), FMargin(0.f, 34.f));

	const FText Nomlar[] = { LOCTEXT("Boshlash", "O'YINNI BOSHLASH"), LOCTEXT("SozlamalarT", "SOZLAMALAR"), LOCTEXT("Chiqish", "CHIQISH") };
	for (int32 I = 0; I < UE_ARRAY_COUNT(Nomlar); ++I)
	{
		UBozorTugma* Tg = CreateWidget<UBozorTugma>(this, UBozorTugma::StaticClass());
		Tg->Sozla(Nomlar[I], 30);
		Tg->Bosildi = [this, I]() { Bajar(I); };
		Tg->UstigaKeldi = [this, I]() { Tanla(I); };
		USizeBox* Kenglik = Yasa<USizeBox>(WidgetTree);
		Kenglik->SetWidthOverride(520.f);
		Kenglik->AddChild(Tg);
		Qosh(Chap, Kenglik, FMargin(0.f, 4.f), HAlign_Left);
		Tugmalar.Add(Tg);
	}
	Qosh(Ov, Chap, HAlign_Left, VAlign_Center, FMargin(130.f, 0.f, 0.f, 40.f));

	// pastki satr: boshqaruv izohi va kompaniya logosi
	Qosh(Ov, Yozuv(WidgetTree, LOCTEXT("MenyuIzoh", "↑ ↓  tanlash     Enter  tasdiqlash"), 16, FLinearColor(1.f, 1.f, 1.f, 0.45f), TEXT("Regular"), 80, false),
		HAlign_Left, VAlign_Bottom, FMargin(134.f, 0.f, 0.f, 46.f));
	UHorizontalBox* Ong = Yasa<UHorizontalBox>(WidgetTree);
	UImage* Logo = Rasm(WidgetTree, Tekstura(TEXT("/Game/UI/Logo/T_Logo_Toliq.T_Logo_Toliq")), FLinearColor(1.f, 1.f, 1.f, 0.85f));
	USizeBox* LogoQuti = Yasa<USizeBox>(WidgetTree);
	LogoQuti->SetWidthOverride(1540.f / 480.f * 56.f);
	LogoQuti->SetHeightOverride(56.f);
	LogoQuti->AddChild(Logo);
	Qosh(Ong, LogoQuti);
	Qosh(Ov, Ong, HAlign_Right, VAlign_Bottom, FMargin(0.f, 0.f, 56.f, 40.f));

	// sozlamalar oynasi (yopiq)
	Sozlamalar = CreateWidget<UBozorSozlamalarOyna>(this, UBozorSozlamalarOyna::StaticClass());
	Sozlamalar->Yopildi = [this]() { SozlamalarniYop(); };
	Sozlamalar->SetVisibility(ESlateVisibility::Collapsed);
	Qosh(Ov, Sozlamalar);

	// kirish: qora pardadan ochiladi; yuklanayotganda yana qoraga
	Qora = Rang(WidgetTree, FLinearColor::Black);
	Qora->SetVisibility(ESlateVisibility::HitTestInvisible);
	Qosh(Ov, Qora);
	Yuklanmoqda = Yozuv(WidgetTree, LOCTEXT("Yuklanmoqda", "YUKLANMOQDA..."), 22, FLinearColor::White, TEXT("Bold"), 400, false);
	Yuklanmoqda->SetRenderOpacity(0.f);
	Yuklanmoqda->SetVisibility(ESlateVisibility::HitTestInvisible);
	Qosh(Ov, Yuklanmoqda, HAlign_Right, VAlign_Bottom, FMargin(0.f, 0.f, 64.f, 52.f));

	Tanla(0);
}

void UBozorBoshMenyu::Tanla(int32 I)
{
	Tanlangan = (I + Tugmalar.Num()) % FMath::Max(1, Tugmalar.Num());
	for (int32 J = 0; J < Tugmalar.Num(); ++J) Tugmalar[J]->SetTanlangan(J == Tanlangan);
}

void UBozorBoshMenyu::Bajar(int32 I)
{
	if (YuklashT >= 0.f) return;
	switch (I)
	{
	case 0:
		YuklashT = 0.f;     // qora parda tushadi, keyin xarita ochiladi
		break;
	case 1:
		SozlamalarniOch();
		break;
	default:
		UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
		break;
	}
}

void UBozorBoshMenyu::SozlamalarniOch()
{
	Sozlamalar->SetVisibility(ESlateVisibility::Visible);
	Sozlamalar->Ochildi();
}

void UBozorBoshMenyu::SozlamalarniYop()
{
	Sozlamalar->SetVisibility(ESlateVisibility::Collapsed);
	SetKeyboardFocus();
}

FReply UBozorBoshMenyu::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey K = InKeyEvent.GetKey();
	if (K == EKeys::Up || K == EKeys::W) { Tanla(Tanlangan - 1); return FReply::Handled(); }
	if (K == EKeys::Down || K == EKeys::S) { Tanla(Tanlangan + 1); return FReply::Handled(); }
	if (K == EKeys::Enter || K == EKeys::SpaceBar) { Bajar(Tanlangan); return FReply::Handled(); }
	return FReply::Handled();
}

void UBozorBoshMenyu::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const float Dt = FMath::Min(InDeltaTime, 0.05f);
	T += Dt;

	// fon juda sekin yaqinlashib-uzoqlashadi (Ken Burns)
	const FVector2D Kb(1.02f + 0.03f * (0.5f - 0.5f * FMath::Cos(T * 2.f * PI / 40.f)));
	if (FonRasm) FonRasm->SetRenderScale(Kb);
	if (FonVideo && Pleer)
	{
		// video o'ynay boshlagach (birinchi kadrlar tayyor) rasm o'rnini egallaydi
		if (Pleer->IsPlaying() && Pleer->GetTime() > FTimespan::FromSeconds(0.2)) VideoAlfa = FMath::FInterpConstantTo(VideoAlfa, 1.f, Dt, 1.5f);
		FonVideo->SetRenderOpacity(VideoAlfa);
		FonVideo->SetRenderScale(Kb);
		static double OxirgiLog = 0.0;
		if (FParse::Param(FCommandLine::Get(), TEXT("UISinov")) && FPlatformTime::Seconds() - OxirgiLog > 1.0)
		{
			OxirgiLog = FPlatformTime::Seconds();
			UE_LOG(LogTemp, Display, TEXT("MenyuVideo ijro: %s vaqt=%.2f playing=%d rate=%.2f"), *Pleer->GetPlayerName().ToString(), Pleer->GetTime().GetTotalSeconds(), Pleer->IsPlaying(), Pleer->GetRate());
		}
	}

	// chap ustun pastdan suzib chiqadi
	const float Kir = ChiqishSilliq(Oraliq(T, 0.3f, 1.3f));
	if (Chap)
	{
		Chap->SetRenderOpacity(Kir);
		Chap->SetRenderTranslation(FVector2D(-30.f * (1.f - Kir), 0.f));
	}

	float QoraA = 1.f - IkkiTomonSilliq(Oraliq(T, 0.f, 1.1f));
	if (YuklashT >= 0.f)
	{
		YuklashT += Dt;
		QoraA = FMath::Max(QoraA, IkkiTomonSilliq(Oraliq(YuklashT, 0.f, 0.6f)));
		if (Yuklanmoqda) Yuklanmoqda->SetRenderOpacity(Oraliq(YuklashT, 0.4f, 0.7f));
		// parda to'liq tushgach va "Yuklanmoqda" bir kadr chizilgach - xarita ochiladi (ochilish paytida kadr qotadi)
		if (YuklashT > 0.8f)
		{
			YuklashT = -100.f;
			OyinUchunKirish(GetOwningPlayer());   // UIOnly rejimi (IgnoreInput) keyingi xaritaga o'tib qolmasin
			UGameplayStatics::OpenLevel(this, FName(BozorUI::BozorXaritasi));
			return;
		}
	}
	if (Qora) Qora->SetRenderOpacity(QoraA);
}

// ---------------------------------------------------------------- pauza

TSharedRef<SWidget> UBozorPauza::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget) Qur();
	return Super::RebuildWidget();
}

void UBozorPauza::Qur()
{
	SetIsFocusable(true);
	UOverlay* Ov = Yasa<UOverlay>(WidgetTree);
	WidgetTree->RootWidget = Ov;
	Qosh(Ov, Rang(WidgetTree, FLinearColor(0.f, 0.01f, 0.03f, 0.62f)));
	USizeBox* GrQuti = Yasa<USizeBox>(WidgetTree);
	GrQuti->SetWidthOverride(1000.f);
	UWidget* Gr = Gradient(WidgetTree, FLinearColor(0.f, 0.01f, 0.03f, 0.8f), true);
	GrQuti->AddChild(Gr);
	if (USizeBoxSlot* S = Cast<USizeBoxSlot>(Gr->Slot)) { S->SetHorizontalAlignment(HAlign_Fill); S->SetVerticalAlignment(VAlign_Fill); }
	Qosh(Ov, GrQuti, HAlign_Left, VAlign_Fill);

	Markaz = Yasa<UVerticalBox>(WidgetTree);
	Qosh(Markaz, Yozuv(WidgetTree, LOCTEXT("Pauza", "PAUZA"), 56, FLinearColor::White, TEXT("Bold"), 200), FMargin(0.f), HAlign_Left);
	Qosh(Markaz, Rang(WidgetTree, BozorUI::Sariq(), FVector2D(110.f, 6.f)), FMargin(0.f, 10.f, 0.f, 40.f), HAlign_Left);
	const FText Nomlar[] = { LOCTEXT("Davom", "DAVOM ETISH"), LOCTEXT("PSozlamalar", "SOZLAMALAR"), LOCTEXT("BoshMenyu", "BOSH MENYU"), LOCTEXT("PChiqish", "O'YINDAN CHIQISH") };
	for (int32 I = 0; I < UE_ARRAY_COUNT(Nomlar); ++I)
	{
		UBozorTugma* Tg = CreateWidget<UBozorTugma>(this, UBozorTugma::StaticClass());
		Tg->Sozla(Nomlar[I], 28);
		Tg->Bosildi = [this, I]() { Bajar(I); };
		Tg->UstigaKeldi = [this, I]() { Tanla(I); };
		USizeBox* Kenglik = Yasa<USizeBox>(WidgetTree);
		Kenglik->SetWidthOverride(480.f);
		Kenglik->AddChild(Tg);
		Qosh(Markaz, Kenglik, FMargin(0.f, 4.f), HAlign_Left);
		Tugmalar.Add(Tg);
	}
	Qosh(Ov, Markaz, HAlign_Left, VAlign_Center, FMargin(130.f, 0.f, 0.f, 0.f));

	Sozlamalar = CreateWidget<UBozorSozlamalarOyna>(this, UBozorSozlamalarOyna::StaticClass());
	Sozlamalar->Yopildi = [this]() { Sozlamalar->SetVisibility(ESlateVisibility::Collapsed); SetKeyboardFocus(); };
	Sozlamalar->SetVisibility(ESlateVisibility::Collapsed);
	Qosh(Ov, Sozlamalar);
	Tanla(0);
}

void UBozorPauza::Tanla(int32 I)
{
	Tanlangan = (I + Tugmalar.Num()) % FMath::Max(1, Tugmalar.Num());
	for (int32 J = 0; J < Tugmalar.Num(); ++J) Tugmalar[J]->SetTanlangan(J == Tanlangan);
}

void UBozorPauza::Bajar(int32 I)
{
	switch (I)
	{
	case 0:
		if (Davom) Davom();
		break;
	case 1:
		Sozlamalar->SetVisibility(ESlateVisibility::Visible);
		Sozlamalar->Ochildi();
		break;
	case 2:
		if (UBozorUITizimi* Ui = UBozorUITizimi::Get(this)) Ui->PauzadanChiq();
		UGameplayStatics::OpenLevel(this, FName(BozorUI::BoshMenyuXaritasi));
		break;
	default:
		UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
		break;
	}
}

FReply UBozorPauza::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey K = InKeyEvent.GetKey();
	if (K == EKeys::Escape) { if (Davom) Davom(); return FReply::Handled(); }
	if (K == EKeys::Up || K == EKeys::W) { Tanla(Tanlangan - 1); return FReply::Handled(); }
	if (K == EKeys::Down || K == EKeys::S) { Tanla(Tanlangan + 1); return FReply::Handled(); }
	if (K == EKeys::Enter || K == EKeys::SpaceBar) { Bajar(Tanlangan); return FReply::Handled(); }
	return FReply::Handled();
}

void UBozorPauza::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	T += FMath::Min(InDeltaTime, 0.05f);
	const float P = ChiqishSilliq(Oraliq(T, 0.f, 0.35f));
	SetRenderOpacity(P);
	if (Markaz) Markaz->SetRenderTranslation(FVector2D(-24.f * (1.f - P), 0.f));
}

// ---------------------------------------------------------------- menyu xaritasi

ABozorMenyuGameMode::ABozorMenyuGameMode()
{
	PlayerControllerClass = ABozorMenyuPC::StaticClass();
	DefaultPawnClass = ASpectatorPawn::StaticClass();
	HUDClass = AHUD::StaticClass();
}

void ABozorMenyuPC::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalController()) return;
	if (MenyusizMi())
	{
		UGameplayStatics::OpenLevel(this, FName(BozorUI::BozorXaritasi));
		return;
	}
	UBozorUITizimi* Ui = UBozorUITizimi::Get(this);
	if (Ui && !Ui->bSplashKorsatildi && !FParse::Param(FCommandLine::Get(), TEXT("SplashYoq")))
	{
		Ui->bSplashKorsatildi = true;
		UBozorSplash* S = CreateWidget<UBozorSplash>(this, UBozorSplash::StaticClass());
		S->Tugadi = [this]() { MenyuniOch(); };
		S->AddToViewport(20);
		Joriy = S;
		UIUchunKirish(this, S);
		SetShowMouseCursor(false);
		return;
	}
	MenyuniOch();
}

void ABozorMenyuPC::MenyuniOch()
{
	if (Joriy) Joriy->RemoveFromParent();
	UBozorBoshMenyu* M = CreateWidget<UBozorBoshMenyu>(this, UBozorBoshMenyu::StaticClass());
	M->AddToViewport(10);
	Joriy = M;
	UIUchunKirish(this, M);
}

// ---------------------------------------------------------------- UI tizimi

UBozorUITizimi* UBozorUITizimi::Get(const UObject* Ctx)
{
	const UWorld* W = Ctx ? Ctx->GetWorld() : nullptr;
	const UGameInstance* Gi = W ? W->GetGameInstance() : nullptr;
	return Gi ? Gi->GetSubsystem<UBozorUITizimi>() : nullptr;
}

void UBozorUITizimi::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UBozorUITizimi::Tick));
}

void UBozorUITizimi::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	Super::Deinitialize();
}

void UBozorUITizimi::PauzaniAlmashtir(APlayerController* Pc)
{
	if (!Pc || !Pc->IsLocalController()) return;
	if (Pauza)
	{
		PauzadanChiq();
		return;
	}
	PauzaPc = Pc;
	bOldinKursor = Pc->ShouldShowMouseCursor();
	Pauza = CreateWidget<UBozorPauza>(Pc, UBozorPauza::StaticClass());
	Pauza->Davom = [this]() { PauzadanChiq(); };
	Pauza->AddToViewport(50);
	Pc->SetPause(true);
	UIUchunKirish(Pc, Pauza);
}

void UBozorUITizimi::PauzadanChiq()
{
	if (Pauza) Pauza->RemoveFromParent();
	Pauza = nullptr;
	APlayerController* Pc = PauzaPc.Get();
	if (!Pc) return;
	Pc->SetPause(false);
	if (bOldinKursor)
	{
		// suhbat paytida pauza qilingan bo'lsa - kursorli holatga qaytamiz
		FInputModeGameAndUI M;
		M.SetHideCursorDuringCapture(false);
		M.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways);
		Pc->SetInputMode(M);
		Pc->SetShowMouseCursor(true);
	}
	else OyinUchunKirish(Pc);
}

bool UBozorUITizimi::Tick(float Dt)
{
	UGameInstance* Gi = GetGameInstance();
	UWorld* W = Gi ? Gi->GetWorld() : nullptr;
	// yangi o'yin xaritasi (menyudan tashqari) boshlanganda o'yin kirish rejimi: menyuning UIOnly'i viewport'da IgnoreInput qoldiradi
	if (W && W->IsGameWorld() && W != OxirgiDunyo.Get())
	{
		if (APlayerController* Pc = W->GetFirstPlayerController())
		{
			OxirgiDunyo = W;
			if (!W->GetMapName().Contains(TEXT("BoshMenyu"))) OyinUchunKirish(Pc);
		}
	}
	if (W && FParse::Param(FCommandLine::Get(), TEXT("MenyuFonOl"))) FonQadami(W, Dt);
	if (W && FParse::Param(FCommandLine::Get(), TEXT("UISinov"))) SinovQadami(W, Dt);
	return true;
}

void UBozorUITizimi::SinovQadami(UWorld* W, float Dt)
{
	if (!W->IsGameWorld() || !W->GetFirstPlayerController()) return;
	if (SinovT < 0.f) SinovT = 0.f;

	// -FpsSinov: bozor xaritasida har grafika sifatida (Past..Eng yuqori) 6 s qizish + 15 s o'lchov; kamera 360° aylanadi.
	// Natija: Saved/FpsSinov.txt (o'rtacha FPS, 1% past, GPU / o'yin oqimi / render oqimi ms)
	if (FParse::Param(FCommandLine::Get(), TEXT("FpsSinov")))
	{
		if (W->GetMapName().Contains(TEXT("BoshMenyu"))) return;
		struct FOlchov { TArray<double> Kadr; double Gpu = 0, Oyin = 0, Render = 0; int32 N = 0; };
		static FOlchov O;
		static double Oldingi = 0.0, Boshi = 0.0;
		static int32 Daraja = -1;
		static FString Natija;
		static const TCHAR* Nomlar[] = { TEXT("Past"), TEXT("O'rta"), TEXT("Yuqori"), TEXT("Eng yuqori") };
		const double Hozir = FPlatformTime::Seconds();
		APlayerController* Pc = W->GetFirstPlayerController();
		if (Daraja < 0)
		{
			if (Boshi == 0.0) { Boshi = Hozir; GEngine->Exec(W, TEXT("t.MaxFPS 0")); GEngine->Exec(W, TEXT("r.VSync 0")); }
			if (Hozir - Boshi < 8.0) return;   // xarita va NPC'lar joylashsin
			Daraja = 0;
			Boshi = Hozir;
			GEngine->Exec(W, TEXT("scalability 0"));
			Natija = FString::Printf(TEXT("GPU: %s\nCPU: %s, %d yadro / %d oqim\nOlcham: %dx%d\n\n"), *GRHIAdapterName, *FPlatformMisc::GetCPUBrand(),
				FPlatformMisc::NumberOfCores(), FPlatformMisc::NumberOfCoresIncludingHyperthreads(),
				GEngine->GameViewport ? (int32)GEngine->GameViewport->Viewport->GetSizeXY().X : 0, GEngine->GameViewport ? (int32)GEngine->GameViewport->Viewport->GetSizeXY().Y : 0);
			Oldingi = Hozir;
			return;
		}
		const double Utgan = Hozir - Boshi;
		const double Dtk = Hozir - Oldingi;
		Oldingi = Hozir;
		// kamera sekin aylanadi: bozor, ko'cha, binolar - hammasi kadrga tushadi
		Pc->SetControlRotation(FRotator(-2.f, (float)(Utgan - 6.0) * 24.f, 0.f));
		if (Utgan > 6.0 && Utgan <= 21.0)
		{
			O.Kadr.Add(Dtk * 1000.0);
			O.Gpu += FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles());
			O.Oyin += FPlatformTime::ToMilliseconds(GGameThreadTime);
			O.Render += FPlatformTime::ToMilliseconds(GRenderThreadTime);
			++O.N;
		}
		else if (Utgan > 21.0)
		{
			TArray<double> S = O.Kadr;
			S.Sort();
			double Jami = 0; for (double K : S) Jami += K;
			const double Ortacha = S.Num() ? Jami / S.Num() : 0;
			const double Past1 = S.Num() ? S[FMath::Clamp((int32)(S.Num() * 0.99), 0, S.Num() - 1)] : 0;
			const int32 N = FMath::Max(1, O.N);
			const FString Q = FString::Printf(TEXT("%-11s  FPS %5.1f   1%% past %5.1f   GPU %5.1f ms   o'yin %5.1f ms   render %5.1f ms   (%d kadr)\n"),
				Nomlar[Daraja], 1000.0 / FMath::Max(0.001, Ortacha), 1000.0 / FMath::Max(0.001, Past1), O.Gpu / N, O.Oyin / N, O.Render / N, S.Num());
			UE_LOG(LogTemp, Display, TEXT("FpsSinov: %s"), *Q);
			Natija += Q;
			O = FOlchov();
			if (++Daraja >= UE_ARRAY_COUNT(Nomlar))
			{
				FFileHelper::SaveStringToFile(Natija, *(FPaths::ProjectSavedDir() / TEXT("FpsSinov.txt")), FFileHelper::EEncodingOptions::ForceUTF8);
				FPlatformMisc::RequestExit(false);
				return;
			}
			Boshi = Hozir;
			GEngine->Exec(W, *FString::Printf(TEXT("scalability %d"), Daraja));
		}
		return;
	}

	// -BoshlashSinov: menyu -> "O'yinni boshlash" -> bozor -> intro o'tkaziladi -> W haqiqiy Slate hodisasi bilan bosiladi
	if (FParse::Param(FCommandLine::Get(), TEXT("BoshlashSinov")))
	{
		SinovT += FMath::Min(Dt, 0.1f);
		auto Bos = [](const FKey& K, bool bPast)
		{
			FKeyEvent E(K, FModifierKeysState(), 0, false, 0, 0);
			if (bPast) FSlateApplication::Get().ProcessKeyDownEvent(E); else FSlateApplication::Get().ProcessKeyUpEvent(E);
		};
		if (W->GetMapName().Contains(TEXT("BoshMenyu")))
		{
			if (SinovQadam == 0 && SinovT > 2.f)
				for (TObjectIterator<UBozorBoshMenyu> It; It; ++It) if (It->GetWorld() == W && It->IsInViewport()) { It->BoshlashSinov(); ++SinovQadam; break; }
			return;
		}
		APawn* Pw = W->GetFirstPlayerController()->GetPawn();
		if (SinovQadam <= 1 && SinovT > 2.f) { SinovT = 0.f; SinovQadam = 2; }
		if (SinovQadam == 2 && SinovT > 4.f) { Bos(EKeys::SpaceBar, true); Bos(EKeys::SpaceBar, false); ++SinovQadam; UE_LOG(LogTemp, Display, TEXT("BoshlashSinov: intro o'tkazildi")); }
		if (SinovQadam == 3 && SinovT > 10.f && Pw) { SinovJoy = Pw->GetActorLocation(); Bos(EKeys::W, true); ++SinovQadam; }
		if (SinovQadam == 4 && SinovT > 13.f && Pw)
		{
			Bos(EKeys::W, false);
			UE_LOG(LogTemp, Display, TEXT("BoshlashSinov: W bilan yurilgan masofa = %.0f sm"), FVector::Dist2D(SinovJoy, Pw->GetActorLocation()));
			FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots/UISinov/13_boshqaruv.png"), true, false);
			++SinovQadam;
		}
		if (SinovQadam == 5 && SinovT > 14.5f) FPlatformMisc::RequestExit(false);
		return;
	}
	// bozor xaritasida: pauza menyusi sinovi
	if (!W->GetMapName().Contains(TEXT("BoshMenyu")))
	{
		SinovT += FMath::Min(Dt, 0.1f);
		const FString Papka = FPaths::ProjectSavedDir() / TEXT("Screenshots/UISinov");
		if (SinovQadam == 0 && SinovT > 8.f) { PauzaniAlmashtir(W->GetFirstPlayerController()); ++SinovQadam; }
		if (SinovQadam == 1 && SinovT > 9.5f) { FScreenshotRequest::RequestScreenshot(Papka / TEXT("11_pauza.png"), true, false); ++SinovQadam; }
		if (SinovQadam == 2 && SinovT > 10.5f) { PauzadanChiq(); ++SinovQadam; }
		if (SinovQadam == 3 && SinovT > 12.f) { FScreenshotRequest::RequestScreenshot(Papka / TEXT("12_oyin.png"), true, false); ++SinovQadam; }
		if (SinovQadam == 4 && SinovT > 13.f) FPlatformMisc::RequestExit(false);
		return;
	}
	SinovT += FMath::Min(Dt, 0.1f);
	struct FQadam { float T; const TCHAR* Nom; int32 Amal; };   // Amal: 0 surat, 1 sozlamalarni och, 2 pastga, 3 yop, 9 chiqish
	static const FQadam Jadval[] = {
		{ 0.9f, TEXT("01_logo_boshi"), 0 }, { 1.6f, TEXT("02_logo_belgi"), 0 }, { 2.4f, TEXT("03_logo_toliq"), 0 },
		{ 4.2f, TEXT("04_logo_ketish"), 0 }, { 5.6f, TEXT("05_nom_boshi"), 0 }, { 7.0f, TEXT("06_nom_toliq"), 0 },
		{ 10.f, TEXT("07_menyu"), 0 }, { 10.5f, nullptr, 2 }, { 11.2f, TEXT("08_menyu_tanlov"), 0 },
		{ 11.5f, nullptr, 1 }, { 12.6f, TEXT("09_sozlamalar"), 0 }, { 13.f, nullptr, 3 }, { 14.f, TEXT("10_qaytdi"), 0 }, { 15.f, nullptr, 9 } };
	while (SinovQadam < UE_ARRAY_COUNT(Jadval) && SinovT >= Jadval[SinovQadam].T)
	{
		const FQadam& Q = Jadval[SinovQadam++];
		UBozorBoshMenyu* M = nullptr;
		for (TObjectIterator<UBozorBoshMenyu> It; It; ++It) if (It->GetWorld() == W && It->IsInViewport()) M = *It;
		switch (Q.Amal)
		{
		case 0:
		{
			const FString Papka = FPaths::ProjectSavedDir() / TEXT("Screenshots/UISinov");
			IFileManager::Get().MakeDirectory(*Papka, true);
			FScreenshotRequest::RequestScreenshot(Papka / FString(Q.Nom) + TEXT(".png"), true, false);
			break;
		}
		case 1: if (M) M->SozlamalarniOch(); break;
		case 2: if (M) M->Tanla(1); break;
		case 3: if (M) M->SozlamalarniYop(); break;
		default: FPlatformMisc::RequestExit(false); break;
		}
	}
}

// Menyu foni uchun kino kadr: bozor xaritasida yo'l ustidan darvozaga qaragan CineCamera, HUD va o'yinchi yashiriladi.
// Ishga tushirish: UnrealEditor.exe BozorUnreal.uproject /Game/Bozor/Maps/Bozor -game -AvtobusYoq -MenyuFonOl -ResX=2560 -ResY=1440 -windowed
// Parametrlar: -FonKamera=x,y,z,pitch,yaw  -FonFokus=mm  -FonDiafragma=f  -FonQuyosh=pitch,yaw  -FonKutish=s  -FonSoni=n
void UBozorUITizimi::FonQadami(UWorld* W, float Dt)
{
	if (!W->IsGameWorld() || !W->GetMapName().Contains(TEXT("Bozor")) || W->GetMapName().Contains(TEXT("BoshMenyu"))) return;
	APlayerController* Pc = W->GetFirstPlayerController();
	if (!Pc) return;
	if (FonDunyo.Get() != W)
	{
		FonDunyo = W;
		FonT = 0.f;
		FonOlindi = 0;
		FonKamera = nullptr;
	}
	FonT += Dt;
	const TCHAR* Cmd = FCommandLine::Get();

	if (!FonKamera.IsValid())
	{
		FString KamS = TEXT("0,2100,165,1.5,-90");
		FParse::Value(Cmd, TEXT("FonKamera="), KamS, false);   // false - vergulda to'xtamasin
		TArray<FString> P;
		KamS.ParseIntoArray(P, TEXT(","));
		while (P.Num() < 5) P.Add(TEXT("0"));
		FVector Joy(FCString::Atof(*P[0]), FCString::Atof(*P[1]), FCString::Atof(*P[2]));
		const FRotator Bur(FCString::Atof(*P[3]), FCString::Atof(*P[4]), 0.f);
		// z - yerdan balandlik: pastga nur otib yer sathini topamiz
		{
			FHitResult Hit;
			const FVector Bosh(Joy.X, Joy.Y, 20000.f);
			if (W->LineTraceSingleByChannel(Hit, Bosh, FVector(Joy.X, Joy.Y, -20000.f), ECC_Visibility))
			{
				UE_LOG(LogTemp, Display, TEXT("MenyuFon: yer z=%.1f (%s)"), Hit.ImpactPoint.Z, *GetNameSafe(Hit.GetActor()));
				Joy.Z += Hit.ImpactPoint.Z;
			}
		}
		FActorSpawnParameters Sp;
		Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ACineCameraActor* K = W->SpawnActor<ACineCameraActor>(Joy, Bur, Sp);
		if (!K) return;
		UCineCameraComponent* C = K->GetCineCameraComponent();
		float Fokus = 32.f, Diafragma = 4.f;
		FParse::Value(Cmd, TEXT("FonFokus="), Fokus);
		FParse::Value(Cmd, TEXT("FonDiafragma="), Diafragma);
		C->Filmback.SensorWidth = 36.f;
		C->Filmback.SensorHeight = 20.25f;
		C->SetCurrentFocalLength(Fokus);
		C->CurrentAperture = Diafragma;
		C->FocusSettings.FocusMethod = ECameraFocusMethod::Manual;
		// fokus - darvozaga (xarita markazi); -FonMasofa bilan o'zgartiriladi
		float Masofa = FMath::Max(300.f, (float)Joy.Size2D() - 50.f);
		FParse::Value(Cmd, TEXT("FonMasofa="), Masofa);
		C->FocusSettings.ManualFocusDistance = Masofa;
		// kino ko'rinish: vinyetka, yengil don, iliqroq ranglar
		FPostProcessSettings& Pp = C->PostProcessSettings;
		Pp.bOverride_VignetteIntensity = true;   Pp.VignetteIntensity = 0.55f;
		Pp.bOverride_FilmGrainIntensity = true;  Pp.FilmGrainIntensity = 0.12f;
		Pp.bOverride_WhiteTemp = true;           Pp.WhiteTemp = 7000.f;
		Pp.bOverride_ColorSaturation = true;     Pp.ColorSaturation = FVector4(1.06f, 1.06f, 1.06f, 1.f);
		Pp.bOverride_ColorContrast = true;       Pp.ColorContrast = FVector4(1.08f, 1.08f, 1.08f, 1.f);
		Pp.bOverride_BloomIntensity = true;      Pp.BloomIntensity = 0.9f;
		Pp.bOverride_AutoExposureBias = true;    Pp.AutoExposureBias = 0.15f;
		Pp.bOverride_SceneFringeIntensity = true; Pp.SceneFringeIntensity = 0.6f;
		FonKamera = K;

		FString QuyoshS;
		if (FParse::Value(Cmd, TEXT("FonQuyosh="), QuyoshS, false))
		{
			TArray<FString> Q;
			QuyoshS.ParseIntoArray(Q, TEXT(","));
			if (Q.Num() >= 2)
				for (TActorIterator<ADirectionalLight> It(W); It; ++It)
				{
					It->GetLightComponent()->SetMobility(EComponentMobility::Movable);
					It->SetActorRotation(FRotator(FCString::Atof(*Q[0]), FCString::Atof(*Q[1]), 0.f));
				}
		}
	}

	// intro yoki o'yinchi kamerasi ustidan har kadr qayta o'rnatamiz
	// o'yinchi o'z kamerasini har kadr qaytarib olmasligi uchun pawn olib tashlanadi
	if (APawn* Pw = Pc->GetPawn())
	{
		Pc->UnPossess();
		Pw->Destroy();
	}
	Pc->bAutoManageActiveCameraTarget = false;
	if (Pc->GetViewTarget() != FonKamera.Get()) Pc->SetViewTarget(FonKamera.Get());
	if (Pc->MyHUD) Pc->MyHUD->bShowHUD = false;

	float Kutish = 10.f;
	int32 Soni = 3;
	FParse::Value(Cmd, TEXT("FonKutish="), Kutish);

	// -FonVideo=N: Kutish'dan keyin har kadr surat (N kadr). -benchmark -fps=30 bilan qat'iy 1/30 s qadam -> silliq video
	int32 VideoKadr = 0;
	if (FParse::Value(Cmd, TEXT("FonVideo="), VideoKadr) && VideoKadr > 0)
	{
		if (FonT < Kutish) return;
		if (FonOlindi < VideoKadr)
		{
			static const FString Papka = FPaths::ProjectSavedDir() / TEXT("Screenshots/MenyuVideo");
			if (FonOlindi == 0) IFileManager::Get().MakeDirectory(*Papka, true);
			FScreenshotRequest::RequestScreenshot(Papka / FString::Printf(TEXT("k_%05d.png"), FonOlindi), false, false);
			if (FonOlindi % 300 == 0) UE_LOG(LogTemp, Display, TEXT("MenyuVideo: %d / %d"), FonOlindi, VideoKadr);
			++FonOlindi;
		}
		else FPlatformMisc::RequestExit(false);
		return;
	}
	FParse::Value(Cmd, TEXT("FonSoni="), Soni);
	const float Keyingi = Kutish + 2.5f * FonOlindi;
	if (FonOlindi < Soni && FonT >= Keyingi)
	{
		const FString Papka = FPaths::ProjectSavedDir() / TEXT("Screenshots/MenyuFon");
		IFileManager::Get().MakeDirectory(*Papka, true);
		FScreenshotRequest::RequestScreenshot(Papka / FString::Printf(TEXT("fon_%d.png"), FonOlindi), false, false);
		UE_LOG(LogTemp, Display, TEXT("MenyuFon: surat %d olindi"), FonOlindi);
		++FonOlindi;
	}
	if (FonOlindi >= Soni && FonT >= Keyingi + 1.5f) FPlatformMisc::RequestExit(false);
}

#undef LOCTEXT_NAMESPACE
