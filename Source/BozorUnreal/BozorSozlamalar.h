// O'yinchi sozlamalari: sichqoncha sezgirligi, Y o'qi, tugmalar. Saved/Config/<Platforma>/GameUserSettings.ini ga saqlanadi.
#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "UObject/Object.h"
#include "BozorSozlamalar.generated.h"

class UInputMappingContext;
class APlayerController;

// Bitta qayta tayinlanadigan amal (Oldinga, Gaplashish ...)
USTRUCT()
struct FBozorTugmaBog
{
	GENERATED_BODY()

	UPROPERTY(Config) FName Amal;
	UPROPERTY(Config) FKey Tugma;
};

struct FBozorAmal
{
	FName Id;
	FText Nomi;
	FKey Standart;
};

UCLASS(Config = GameUserSettings)
class BOZORUNREAL_API UBozorSozlamalar : public UObject
{
	GENERATED_BODY()

public:
	// CDO - butun o'yin uchun bitta nusxa
	static UBozorSozlamalar* Get();

	// barcha qayta tayinlanadigan amallar (tartibi - sozlamalar ro'yxatidagi tartib)
	static const TArray<FBozorAmal>& Amallar();

	// amalning hozirgi tugmasi (o'yinchi o'zgartirmagan bo'lsa - standart)
	FKey Tugma(FName Amal) const;
	// tugmani tayinlaydi; boshqa amal shu tugmada bo'lsa - ular almashadi
	void TugmaQoy(FName Amal, const FKey& Yangi);
	void StandartgaQaytar();
	void Saqla();

	// tugma ekranda qanday yoziladi (qisqa, o'zbekcha)
	static FText TugmaNomi(const FKey& K);
	// shu tugmani tayinlab bo'ladimi (Esc, raqamlar va Enter band)
	static bool TayinlashMumkin(const FKey& K);

	// IMC nusxasi: standart W/A/S/D/Probel o'rniga o'yinchi tanlagan tugmalar (modifikatorlar saqlanadi)
	UInputMappingContext* MoslashganImc(UInputMappingContext* Asl);
	// o'yindagi o'yinchiga darhol qo'llash (pauza menyusidan o'zgartirilganda)
	static void OyinchigaQollash(APlayerController* Pc);

	UPROPERTY(Config) float Sezgirlik = 1.f;      // sichqoncha qarash tezligi ko'paytuvchisi
	UPROPERTY(Config) bool bTeskariY = false;
	UPROPERTY(Config) TArray<FBozorTugmaBog> Tugmalar;

private:
	UPROPERTY(Transient) TMap<TObjectPtr<UInputMappingContext>, TObjectPtr<UInputMappingContext>> ImcNusxalar;
};
