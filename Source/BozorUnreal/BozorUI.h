// O'yin UI'si: kirish logolari (splash), bosh menyu, sozlamalar oynasi, pauza menyusi.
// Hammasi C++ da quriladi (Widget Blueprint yo'q) - shuning uchun muharrirda alohida asset kerak emas.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "BozorUI.generated.h"

class UButton;
class UTextBlock;
class UImage;
class UBorder;
class USizeBox;
class UOverlay;
class UVerticalBox;
class UScrollBox;
class USlider;
class UTexture2D;
class UBozorSozlamalarOyna;
class ACineCameraActor;

namespace BozorUI
{
	// bozor yozuvlari bilan bir xil ranglar: ko'k plita, oq yozuv, sariq chiziq
	FLinearColor Kok();
	FLinearColor Sariq();
	FSlateFontInfo Shrift(int32 Olcham, const TCHAR* Turi = TEXT("Bold"), int32 Oraliq = 0);
	extern const TCHAR* BoshMenyuXaritasi;
	extern const TCHAR* BozorXaritasi;
}

// Menyu tugmasi: sichqoncha ustiga kelganda chapda sariq chiziq o'sadi, yozuv biroz suriladi
UCLASS()
class BOZORUNREAL_API UBozorTugma : public UUserWidget
{
	GENERATED_BODY()

public:
	// bKichik - sozlamalardagi ixcham tugma (markazda yozuv, doim ko'rinadigan fon)
	void Sozla(const FText& Matn, int32 Olcham, bool bKichik = false);
	void SetMatn(const FText& Matn, const FLinearColor& Rang = FLinearColor::White);
	void SetTanlangan(bool bHa) { bTanlangan = bHa; }
	bool IsUstida() const { return bUstida; }

	TFunction<void()> Bosildi;
	TFunction<void()> UstigaKeldi;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION() void OnBos();
	UFUNCTION() void OnUst();
	UFUNCTION() void OnKet();

	UPROPERTY() TObjectPtr<UButton> Btn;
	UPROPERTY() TObjectPtr<UTextBlock> Yozuv;
	UPROPERTY() TObjectPtr<UImage> Chiziq;
	UPROPERTY() TObjectPtr<UImage> Fon;

	FText Matn0;
	int32 Olcham0 = 30;
	bool bKichik0 = false;
	bool bTanlangan = false;
	bool bUstida = false;
	float Holat = 0.f;
};

// Kirishdagi logolar: 1) Tadbirkorlikni Rivojlantirish Kompaniyasi, 2) o'yin nomi
UCLASS()
class BOZORUNREAL_API UBozorSplash : public UUserWidget
{
	GENERATED_BODY()

public:
	TFunction<void()> Tugadi;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	void OtkazibYubor();
	void Qur();

	UPROPERTY() TObjectPtr<UWidget> Porlash;
	UPROPERTY() TObjectPtr<UImage> Belgi;
	UPROPERTY() TObjectPtr<UImage> Matn;
	UPROPERTY() TObjectPtr<UImage> Chiziq;
	UPROPERTY() TObjectPtr<UVerticalBox> LogoGuruh;
	UPROPERTY() TObjectPtr<UTextBlock> Nom;
	UPROPERTY() TObjectPtr<UTextBlock> Shior;
	UPROPERTY() TObjectPtr<UImage> NomChiziq;
	UPROPERTY() TObjectPtr<UTextBlock> Maslahat;
	UPROPERTY() TObjectPtr<UTexture2D> TxBelgi;
	UPROPERTY() TObjectPtr<UTexture2D> TxMatn;

	float T = 0.f;
	bool bTugadi = false;
};

// Sozlamalar: sichqoncha sezgirligi, Y o'qi, grafika, tugmalarni qayta tayinlash
UCLASS()
class BOZORUNREAL_API UBozorSozlamalarOyna : public UUserWidget
{
	GENERATED_BODY()

public:
	TFunction<void()> Yopildi;
	void Ochildi();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	void Qur();
	void Yangila();
	void TugmaUshla(const FKey& K);
	void Qollash();
	void Yop();

	UFUNCTION() void SezgirlikOzgardi(float V);

	UPROPERTY() TObjectPtr<UBorder> Panel;
	UPROPERTY() TObjectPtr<USlider> Sezgirlik;
	UPROPERTY() TObjectPtr<UTextBlock> SezgirlikQiymat;
	UPROPERTY() TObjectPtr<UBozorTugma> TeskariY;
	UPROPERTY() TObjectPtr<UBozorTugma> Sifat;
	UPROPERTY() TObjectPtr<UBozorTugma> EkranRejimi;
	UPROPERTY() TArray<TObjectPtr<UBozorTugma>> TugmaQatorlar;
	UPROPERTY() TObjectPtr<UTextBlock> Izoh;

	FName Kutilayotgan;
	float OchilishT = 0.f;
};

// Bosh menyu: fonda bozor darvozasining kino kadri, chapda nom va tugmalar
UCLASS()
class BOZORUNREAL_API UBozorBoshMenyu : public UUserWidget
{
	GENERATED_BODY()

public:
	// -UISinov uchun
	void Tanla(int32 I);
	void BoshlashSinov() { Bajar(0); }
	void SozlamalarniOch();
	void SozlamalarniYop();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	void Qur();
	void Bajar(int32 I);

	UPROPERTY() TObjectPtr<UImage> FonRasm;
	// jonli fon: Content/Movies/MenyuFon.mp4 (zebra boshidan bola ko'zi, 5 daqiqa, aylanma)
	UPROPERTY() TObjectPtr<UImage> FonVideo;
	UPROPERTY() TObjectPtr<class UMediaPlayer> Pleer;
	UPROPERTY() TObjectPtr<class UMediaTexture> VideoTekstura;
	UPROPERTY() TObjectPtr<class UFileMediaSource> VideoManba;
	float VideoAlfa = 0.f;
	UPROPERTY() TObjectPtr<UVerticalBox> Chap;
	UPROPERTY() TArray<TObjectPtr<UBozorTugma>> Tugmalar;
	UPROPERTY() TObjectPtr<UBozorSozlamalarOyna> Sozlamalar;
	UPROPERTY() TObjectPtr<UImage> Qora;
	UPROPERTY() TObjectPtr<UTextBlock> Yuklanmoqda;

	float T = 0.f;
	int32 Tanlangan = 0;
	float YuklashT = -1.f;   // >= 0 - bozor xaritasi ochilmoqda
};

// O'yin ichida Esc: pauza
UCLASS()
class BOZORUNREAL_API UBozorPauza : public UUserWidget
{
	GENERATED_BODY()

public:
	TFunction<void()> Davom;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	void Qur();
	void Tanla(int32 I);
	void Bajar(int32 I);

	UPROPERTY() TArray<TObjectPtr<UBozorTugma>> Tugmalar;
	UPROPERTY() TObjectPtr<UBozorSozlamalarOyna> Sozlamalar;
	UPROPERTY() TObjectPtr<UVerticalBox> Markaz;
	int32 Tanlangan = 0;
	float T = 0.f;
};

UCLASS()
class BOZORUNREAL_API ABozorMenyuPC : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;
	void MenyuniOch();

	UPROPERTY() TObjectPtr<UUserWidget> Joriy;
};

// /Game/UI/Maps/BoshMenyu xaritasi uchun (DefaultEngine.ini: GameModeMapPrefixes)
UCLASS()
class BOZORUNREAL_API ABozorMenyuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ABozorMenyuGameMode();
};

// Pauza menyusi va menyu foni uchun kino kadr olish (-MenyuFonOl)
UCLASS()
class BOZORUNREAL_API UBozorUITizimi : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UBozorUITizimi* Get(const UObject* Ctx);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void PauzaniAlmashtir(APlayerController* Pc);
	bool PauzadaMi() const { return Pauza != nullptr; }
	void PauzadanChiq();

	// splash bir sessiyada bir marta (bosh menyuga qaytilganda takrorlanmaydi)
	bool bSplashKorsatildi = false;

private:
	bool Tick(float Dt);
	void FonQadami(UWorld* W, float Dt);

	FTSTicker::FDelegateHandle TickHandle;
	UPROPERTY() TObjectPtr<UBozorPauza> Pauza;
	TWeakObjectPtr<APlayerController> PauzaPc;
	bool bOldinKursor = false;

	// -MenyuFonOl
	TWeakObjectPtr<ACineCameraActor> FonKamera;
	TWeakObjectPtr<UWorld> FonDunyo;
	TWeakObjectPtr<UWorld> OxirgiDunyo;
	float FonT = 0.f;
	int32 FonOlindi = 0;

	// -UISinov: splash/menyu/sozlamalarni o'zi suratga oladi (Saved/Screenshots/UISinov)
	void SinovQadami(UWorld* W, float Dt);
	float SinovT = -1.f;
	int32 SinovQadam = 0;
	FVector SinovJoy = FVector::ZeroVector;
};
