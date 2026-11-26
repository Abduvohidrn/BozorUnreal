#include "BozorMobil.h"

#include "BozorHud.h"
#include "BozorMashina.h"
#include "BozorOyinchi.h"
#include "BozorUI.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSafeZone.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

bool BozorMobil::Faol()
{
#if PLATFORM_ANDROID || PLATFORM_IOS
	return true;
#elif !UE_BUILD_SHIPPING
	return FParse::Param(FCommandLine::Get(), TEXT("BozorMobil"));
#else
	return false;
#endif
}

class SBozorSensor : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBozorSensor) {}
		SLATE_ARGUMENT(TFunction<void(const FVector2D&)>, Aylantir)
	SLATE_END_ARGS()

	void Construct(const FArguments& Args)
	{
		Aylantir = Args._Aylantir;
		if (!Aylantir)
		{
			static const FSlateRoundedBoxBrush Fon(FLinearColor(0.03f, 0.06f, 0.10f, 0.58f), 66.f,
				FLinearColor(1.f, 1.f, 1.f, 0.25f), 1.f);
			static const FSlateRoundedBoxBrush Tutqich(FLinearColor(0.94f, 0.78f, 0.45f, 0.74f), 21.f);
			ChildSlot
			[
				SNew(SBorder).BorderImage(&Fon).HAlign(HAlign_Center).VAlign(VAlign_Center)
				[
					SNew(SBox).WidthOverride(42.f).HeightOverride(42.f).Visibility(EVisibility::HitTestInvisible)
					.RenderTransform_Lambda([this]() -> TOptional<FSlateRenderTransform>
					{
						const FVector2D V = Qiymat();
						return FSlateRenderTransform(FVector2f(V.X * 38., -V.Y * 38.));
					})
					[
						SNew(SBorder).BorderImage(&Tutqich)
					]
				]
			];
		}
	}

	FVector2D Qiymat() const
	{
		return Barmoq != INDEX_NONE && HasMouseCaptureByUser(Foydalanuvchi, Barmoq) ? Vektor : FVector2D::ZeroVector;
	}

	virtual FReply OnTouchStarted(const FGeometry& G, const FPointerEvent& E) override
	{
		if (Barmoq != INDEX_NONE) return FReply::Unhandled();
		Barmoq = E.GetPointerIndex();
		Foydalanuvchi = E.GetUserIndex();
		Oldingi = G.AbsoluteToLocal(E.GetScreenSpacePosition());
		if (!Aylantir) JoystikniYangila(G, Oldingi);
		return FReply::Handled().CaptureMouse(SharedThis(this));
	}

	virtual FReply OnTouchMoved(const FGeometry& G, const FPointerEvent& E) override
	{
		if (E.GetPointerIndex() != Barmoq || E.GetUserIndex() != Foydalanuvchi) return FReply::Unhandled();
		const FVector2D Joy = G.AbsoluteToLocal(E.GetScreenSpacePosition());
		if (Aylantir) Aylantir(Joy - Oldingi);
		else JoystikniYangila(G, Joy);
		Oldingi = Joy;
		return FReply::Handled();
	}

	virtual FReply OnTouchEnded(const FGeometry&, const FPointerEvent& E) override
	{
		if (E.GetPointerIndex() != Barmoq || E.GetUserIndex() != Foydalanuvchi) return FReply::Unhandled();
		Barmoq = INDEX_NONE;
		Vektor = FVector2D::ZeroVector;
		return FReply::Handled().ReleaseMouseCapture();
	}

	virtual void OnMouseCaptureLost(const FCaptureLostEvent& E) override
	{
		SCompoundWidget::OnMouseCaptureLost(E);
		Barmoq = INDEX_NONE;
		Vektor = FVector2D::ZeroVector;
	}

private:
	void JoystikniYangila(const FGeometry& G, const FVector2D& Joy)
	{
		const FVector2D Markaz = G.GetLocalSize() * 0.5;
		const FVector2D Farq = Joy - Markaz;
		const double Kuch = FMath::GetMappedRangeValueClamped(FVector2D(8., 54.), FVector2D(0., 1.), Farq.Size());
		Vektor = FVector2D(Farq.X, -Farq.Y).GetSafeNormal() * Kuch;
	}

	int32 Barmoq = INDEX_NONE;
	int32 Foydalanuvchi = 0;
	FVector2D Oldingi = FVector2D::ZeroVector;
	FVector2D Vektor = FVector2D::ZeroVector;
	TFunction<void(const FVector2D&)> Aylantir;
};

namespace
{
	TSharedRef<SWidget> Tugma(TAttribute<FText> Nom, FOnClicked Amal)
	{
		static const FButtonStyle Uslub = FButtonStyle()
			.SetNormal(FSlateRoundedBoxBrush(FLinearColor(0.03f, 0.06f, 0.10f, 0.80f), 11.f, FLinearColor(1.f, 1.f, 1.f, 0.24f), 1.f))
			.SetHovered(FSlateRoundedBoxBrush(FLinearColor(0.08f, 0.12f, 0.19f, 0.91f), 11.f))
			.SetPressed(FSlateRoundedBoxBrush(FLinearColor(0.34f, 0.23f, 0.10f, 0.96f), 11.f));
		return SNew(SBox).MinDesiredHeight(52.f).MinDesiredWidth(76.f)
		[
			SNew(SButton).ButtonStyle(&Uslub).IsFocusable(false).HAlign(HAlign_Center).VAlign(VAlign_Center)
			.ContentPadding(FMargin(14.f, 4.f)).OnClicked(Amal)
			[
				SNew(STextBlock).Text(Nom).ColorAndOpacity(FLinearColor::White).Font(FCoreStyle::GetDefaultFontStyle("Bold", 17))
			]
		];
	}
}

ABozorHud* UBozorMobil::Hud() const
{
	const APlayerController* Pc = GetOwningPlayer();
	return Pc ? Cast<ABozorHud>(Pc->GetHUD()) : nullptr;
}

TSharedRef<SWidget> UBozorMobil::RebuildWidget()
{
	const auto Oyinchi = [this]() { return Cast<ABozorOyinchi>(GetOwningPlayerPawn()); };
	const auto Haydov = [this]() { return Cast<ABozorMashina>(GetOwningPlayerPawn()); };
	const auto Holat = [this, Oyinchi, Haydov]()
	{
		const ABozorHud* H = Hud();
		if (!H || !GetWorld() || GetWorld()->IsPaused() || !H->BoshqaruvFaol() || H->TelefonFaol())
			return EVisibility::Collapsed;
		const ABozorOyinchi* O = Oyinchi();
		return Haydov() || (O && !O->NatijaKorinadi() && !O->Suhbatda()) ? EVisibility::Visible : EVisibility::Collapsed;
	};
	const auto Yurish = Holat;
	const auto Piyoda = [Yurish, Oyinchi]()
	{
		return Yurish() == EVisibility::Visible && Oyinchi() ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed;
	};
	const auto Natija = [Oyinchi]()
	{
		const ABozorOyinchi* O = Oyinchi();
		return O && O->NatijaKorinadi() ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed;
	};
	const auto Suhbat = [this, Oyinchi]()
	{
		const ABozorOyinchi* O = Oyinchi();
		const ABozorHud* H = Hud();
		return H && H->BoshqaruvFaol() && !H->TelefonFaol() && O && !O->NatijaKorinadi() && O->Suhbatda();
	};
	const auto Amal = [this, Natija]()
	{
		const ABozorHud* H = Hud();
		return H && Natija() == EVisibility::Collapsed ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed;
	};
	const auto AsosiyNom = [this, Oyinchi]()
	{
		const ABozorHud* H = Hud();
		const ABozorOyinchi* O = Oyinchi();
		if (!H || !H->BoshqaruvFaol()) return FText::FromString(TEXT("O'tkazish"));
		if (H->TelefonFaol()) return FText::FromString(TEXT("Davom"));
		if (O && O->Suhbatda()) return FText::FromString(TEXT("Yopish"));
		return FText::FromString(TEXT("Gaplashish"));
	};

	return SNew(SSafeZone).Visibility(EVisibility::SelfHitTestInvisible).IsTitleSafe(true)
	[
		SNew(SScaleBox).Visibility(EVisibility::SelfHitTestInvisible).Stretch(EStretch::ScaleToFit)
		[
			SNew(SBox).Visibility(EVisibility::SelfHitTestInvisible).WidthOverride(960.f).HeightOverride(420.f)
			[
				SNew(SOverlay).Visibility_Lambda([this]()
				{
					return GetWorld() && !GetWorld()->IsPaused() ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed;
				})
				+ SOverlay::Slot().Padding(400.f, 65.f, 12.f, 78.f)
				[
					SNew(SBozorSensor).Visibility_Lambda(Yurish)
					.Aylantir([this](const FVector2D& Delta) { Kamera(Delta); })
				]
				+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(16.f)
				[
					SNew(SBox).WidthOverride(132.f).HeightOverride(132.f).Visibility_Lambda(Yurish)
					[
						SAssignNew(Joystik, SBozorSensor)
					]
				]
				+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(12.f)
				[
					Tugma(FText::FromString(TEXT("Pauza")), FOnClicked::CreateWeakLambda(this, [this]()
					{
						if (UBozorUITizimi* Ui = UBozorUITizimi::Get(this)) Ui->PauzaniAlmashtir(GetOwningPlayer());
						return FReply::Handled();
					}))
				]
				+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(12.f)
				[
					SNew(SHorizontalBox).Visibility_Lambda(Amal)
					+ SHorizontalBox::Slot().AutoWidth().Padding(4.f)
					[
						SNew(SBox).Visibility_Lambda([Yurish]() { return Yurish() == EVisibility::Visible ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed; })
						[
							Tugma(TAttribute<FText>::CreateLambda([Haydov]()
							{
								return FText::FromString(Haydov() ? TEXT("Tushish") : TEXT("Mashina"));
							}), FOnClicked::CreateWeakLambda(this, [this]() { Mashina(); return FReply::Handled(); }))
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(4.f)
					[
						SNew(SBox).Visibility_Lambda(Piyoda)
						[
							Tugma(FText::FromString(TEXT("Sakrash")), FOnClicked::CreateWeakLambda(this, [Oyinchi]()
							{
								if (ABozorOyinchi* O = Oyinchi()) O->Jump();
								return FReply::Handled();
							}))
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(4.f)
					[
						SNew(SBox).Visibility_Lambda(Piyoda)
						[
							Tugma(FText::FromString(TEXT("Ro'yxat")), FOnClicked::CreateWeakLambda(this, [this]()
							{
								if (ABozorHud* H = Hud()) H->MobilRoyxat();
								return FReply::Handled();
							}))
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(4.f)
					[
						SNew(SBox).Visibility_Lambda([Haydov]() { return Haydov() ? EVisibility::Collapsed : EVisibility::SelfHitTestInvisible; })
						[
							Tugma(TAttribute<FText>::CreateLambda(AsosiyNom), FOnClicked::CreateWeakLambda(this, [this]()
							{
								if (ABozorHud* H = Hud()) H->MobilAmal();
								return FReply::Handled();
							}))
						]
					]
				]
				+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0.f, 0.f, 0.f, 93.f)
				[
					SNew(SHorizontalBox).Visibility_Lambda([Suhbat]() { return Suhbat() ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed; })
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Fill).Padding(4.f)
					[
						SNew(SBox).WidthOverride(440.f)
						[
							SAssignNew(Matn, SEditableTextBox)
							.HintText(FText::FromString(TEXT("Sotuvchiga yozing")))
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", 19))
							.VirtualKeyboardDismissAction(EVirtualKeyboardDismissAction::TextChangeOnDismiss)
							.OnTextCommitted_Lambda([this](const FText&, ETextCommit::Type Tur)
							{
								if (Tur == ETextCommit::OnEnter) MatnYubor();
							})
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(4.f)
					[
						Tugma(FText::FromString(TEXT("Yuborish")), FOnClicked::CreateWeakLambda(this, [this]()
						{
							MatnYubor();
							return FReply::Handled();
						}))
					]
				]
				+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(12.f)
				[
					SNew(SHorizontalBox).Visibility_Lambda(Natija)
					+ SHorizontalBox::Slot().AutoWidth().Padding(4.f)
					[
						Tugma(FText::FromString(TEXT("Davom")), FOnClicked::CreateWeakLambda(this, [Oyinchi]()
						{
							if (ABozorOyinchi* O = Oyinchi()) O->EnterBosildi();
							return FReply::Handled();
						}))
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(4.f)
					[
						Tugma(FText::FromString(TEXT("Qaytadan")), FOnClicked::CreateWeakLambda(this, [Oyinchi]()
						{
							if (ABozorOyinchi* O = Oyinchi()) O->RBosildi();
							return FReply::Handled();
						}))
					]
				]
			]
		]
	];
}

void UBozorMobil::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	Joystik.Reset();
	Matn.Reset();
}

void UBozorMobil::NativeTick(const FGeometry& Geometry, float Dt)
{
	Super::NativeTick(Geometry, Dt);
	const ABozorHud* H = Hud();
	const FVector2D V = H && H->BoshqaruvFaol() && !H->TelefonFaol() && !GetWorld()->IsPaused() && Joystik.IsValid()
		? Joystik->Qiymat() : FVector2D::ZeroVector;
	if (ABozorMashina* M = Cast<ABozorMashina>(GetOwningPlayerPawn())) M->MobilHarakat(V);
	if (ABozorOyinchi* O = Cast<ABozorOyinchi>(GetOwningPlayerPawn()); O && !O->NatijaKorinadi() && !O->Suhbatda() && !V.IsNearlyZero())
		O->Move(FInputActionValue(V));
}

void UBozorMobil::Kamera(const FVector2D& Delta)
{
	if (ABozorMashina* M = Cast<ABozorMashina>(GetOwningPlayerPawn())) M->MobilKamera(Delta * 0.12);
	if (ABozorOyinchi* O = Cast<ABozorOyinchi>(GetOwningPlayerPawn()); O && !O->NatijaKorinadi() && !O->Suhbatda())
		O->Look(FInputActionValue(FVector2D(Delta.X, -Delta.Y) * 0.30));
}

void UBozorMobil::Mashina()
{
	if (ABozorMashina* M = Cast<ABozorMashina>(GetOwningPlayerPawn()))
	{
		M->MobilOtirTush();
		return;
	}
	ABozorMashina* Yaqin = nullptr;
	double EngYaqin = TNumericLimits<double>::Max();
	const APawn* O = GetOwningPlayerPawn();
	if (!O) return;
	for (TActorIterator<ABozorMashina> It(GetWorld()); It; ++It)
	{
		const double Masofa = FVector::DistSquared2D(O->GetActorLocation(), It->GetActorLocation());
		if (It->Haydalyapti() || Masofa >= EngYaqin || Masofa >= FMath::Square(It->OtirishMasofasi)) continue;
		EngYaqin = Masofa;
		Yaqin = *It;
	}
	if (Yaqin) Yaqin->MobilOtirTush();
}

void UBozorMobil::MatnYubor()
{
	ABozorOyinchi* O = Cast<ABozorOyinchi>(GetOwningPlayerPawn());
	if (!O || !O->Suhbatda() || !Matn.IsValid()) return;
	const FString Gap = Matn->GetText().ToString().TrimStartAndEnd();
	if (Gap.IsEmpty()) return;
	Matn->SetText(FText::GetEmpty());
	O->Gapir(Gap);
}
