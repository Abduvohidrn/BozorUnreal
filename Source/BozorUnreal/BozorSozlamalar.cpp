#include "BozorSozlamalar.h"

#include "BozorOyinchi.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "GameFramework/PlayerController.h"

#define LOCTEXT_NAMESPACE "BozorSozlamalar"

UBozorSozlamalar* UBozorSozlamalar::Get()
{
	return GetMutableDefault<UBozorSozlamalar>();
}

const TArray<FBozorAmal>& UBozorSozlamalar::Amallar()
{
	static const TArray<FBozorAmal> Royxat = {
		{ TEXT("Oldinga"),    LOCTEXT("Oldinga", "Oldinga yurish"),      EKeys::W },
		{ TEXT("Orqaga"),     LOCTEXT("Orqaga", "Orqaga yurish"),        EKeys::S },
		{ TEXT("Chapga"),     LOCTEXT("Chapga", "Chapga yurish"),        EKeys::A },
		{ TEXT("Onga"),       LOCTEXT("Onga", "O'ngga yurish"),          EKeys::D },
		{ TEXT("Sakrash"),    LOCTEXT("Sakrash", "Sakrash"),             EKeys::SpaceBar },
		{ TEXT("Gaplashish"), LOCTEXT("Gaplashish", "Gaplashish / xayrlashish"), EKeys::E },
		{ TEXT("Royxat"),     LOCTEXT("Royxat", "Xarid ro'yxati (bosib turish)"), EKeys::Tab },
		{ TEXT("Qaytadan"),   LOCTEXT("Qaytadan", "Darajani qaytadan o'ynash"), EKeys::R },
	};
	return Royxat;
}

FKey UBozorSozlamalar::Tugma(FName Amal) const
{
	for (const FBozorTugmaBog& B : Tugmalar)
		if (B.Amal == Amal && B.Tugma.IsValid()) return B.Tugma;
	for (const FBozorAmal& A : Amallar())
		if (A.Id == Amal) return A.Standart;
	return EKeys::Invalid;
}

void UBozorSozlamalar::TugmaQoy(FName Amal, const FKey& Yangi)
{
	const FKey Eski = Tugma(Amal);
	if (Eski == Yangi) return;
	auto Yoz = [this](FName Id, const FKey& K)
	{
		for (FBozorTugmaBog& B : Tugmalar)
			if (B.Amal == Id) { B.Tugma = K; return; }
		Tugmalar.Add({ Id, K });
	};
	// boshqa amal shu tugmada bo'lsa - unga eski tugmani beramiz (almashuv)
	for (const FBozorAmal& A : Amallar())
		if (A.Id != Amal && Tugma(A.Id) == Yangi) Yoz(A.Id, Eski);
	Yoz(Amal, Yangi);
}

void UBozorSozlamalar::StandartgaQaytar()
{
	Tugmalar.Reset();
	Sezgirlik = 1.f;
	bTeskariY = false;
}

void UBozorSozlamalar::Saqla()
{
	SaveConfig();
}

FText UBozorSozlamalar::TugmaNomi(const FKey& K)
{
	if (K == EKeys::SpaceBar) return LOCTEXT("Probel", "Probel");
	if (K == EKeys::LeftShift || K == EKeys::RightShift) return FText::FromString(TEXT("Shift"));
	if (K == EKeys::LeftControl || K == EKeys::RightControl) return FText::FromString(TEXT("Ctrl"));
	if (K == EKeys::LeftAlt || K == EKeys::RightAlt) return FText::FromString(TEXT("Alt"));
	if (K == EKeys::RightMouseButton) return LOCTEXT("OngSich", "O'ng tugma");
	if (K == EKeys::MiddleMouseButton) return LOCTEXT("OrtaSich", "G'ildirak");
	if (K == EKeys::Up) return FText::FromString(TEXT("↑"));
	if (K == EKeys::Down) return FText::FromString(TEXT("↓"));
	if (K == EKeys::Left) return FText::FromString(TEXT("←"));
	if (K == EKeys::Right) return FText::FromString(TEXT("→"));
	return K.GetDisplayName(false);
}

bool UBozorSozlamalar::TayinlashMumkin(const FKey& K)
{
	if (!K.IsValid() || K.IsAxis1D() || K.IsAxis2D() || K.IsAxis3D() || K.IsTouch() || K.IsGamepadKey()) return false;
	static const FKey Band[] = { EKeys::Escape, EKeys::Enter, EKeys::LeftMouseButton, EKeys::MouseScrollUp, EKeys::MouseScrollDown,
		EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine,
		EKeys::LeftCommand, EKeys::RightCommand, EKeys::CapsLock };
	for (const FKey& B : Band) if (K == B) return false;
	return true;
}

UInputMappingContext* UBozorSozlamalar::MoslashganImc(UInputMappingContext* Asl)
{
	if (!Asl) return nullptr;
	// standart tugma -> amal (faqat IA_Move / IA_Jump xaritalari almashadi; strelkalar va gamepad o'z joyida)
	struct FJuft { FKey Standart; FName Amal; };
	const FJuft Juftlar[] = { { EKeys::W, TEXT("Oldinga") }, { EKeys::S, TEXT("Orqaga") }, { EKeys::A, TEXT("Chapga") },
		{ EKeys::D, TEXT("Onga") }, { EKeys::SpaceBar, TEXT("Sakrash") } };

	UInputMappingContext* Nusxa = DuplicateObject<UInputMappingContext>(Asl, GetTransientPackage());
	const TArray<FEnhancedActionKeyMapping> Eski = Nusxa->GetMappings();
	// avval hamma o'zgaradiganlarni yig'amiz, keyin olib tashlab qayta qo'shamiz (W<->S almashuvi ham to'g'ri bo'lsin)
	TArray<TPair<FEnhancedActionKeyMapping, FKey>> Ozgarish;
	for (const FEnhancedActionKeyMapping& M : Eski)
	{
		const FString ActNomi = M.Action ? M.Action->GetName() : FString();
		if (ActNomi != TEXT("IA_Move") && ActNomi != TEXT("IA_Jump")) continue;
		for (const FJuft& J : Juftlar)
		{
			if (M.Key != J.Standart) continue;
			const FKey Yangi = Tugma(J.Amal);
			if (Yangi != J.Standart) Ozgarish.Emplace(M, Yangi);
			break;
		}
	}
	for (const TPair<FEnhancedActionKeyMapping, FKey>& O : Ozgarish)
		Nusxa->UnmapKey(O.Key.Action, O.Key.Key);
	for (const TPair<FEnhancedActionKeyMapping, FKey>& O : Ozgarish)
	{
		FEnhancedActionKeyMapping& N = Nusxa->MapKey(O.Key.Action, O.Value);
		N.Modifiers = O.Key.Modifiers;
		N.Triggers = O.Key.Triggers;
	}
	ImcNusxalar.Add(Asl, Nusxa);
	return Nusxa;
}

void UBozorSozlamalar::OyinchigaQollash(APlayerController* Pc)
{
	if (ABozorOyinchi* O = Pc ? Cast<ABozorOyinchi>(Pc->GetPawn()) : nullptr)
		O->KirishniYangila();
}

#undef LOCTEXT_NAMESPACE
