#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "BozorTelefon.generated.h"

class APlayerController;
class UAudioComponent;
class UCanvas;
class USoundBase;
class USoundClass;
class UTexture2D;
struct FBozorDaraja;

// Avtobusdan tushgach bolaga keladigan telefon qo'ng'irog'i (Advanced Phone System asseti: /Game/PhoneSystem - fon, ikonkalar,
// qo'ng'iroq tovushlari). Telefon ekranning o'ng tomonidan chiqib titraydi; E (Gaplashish) yoki Enter - javob berish
// (15 s javob berilmasa o'zi ulanadi). Suhbat gaplari darajadan (sotuv.json "qongiroq") - ekranda subtitr, onaning gapi
// Navoiy ishlayotgan bo'lsa ayol ovozida o'qiladi. Suhbatda E/Enter - keyingi gap. Tugagach telefon cho'ntakka qaytadi.
// HUD chaqiradi: Boshla -> har kadr Tick va Chiz -> Tugadimi.
UCLASS()
class BOZORUNREAL_API UBozorTelefon : public UObject
{
	GENERATED_BODY()

public:
	UBozorTelefon();

	void Boshla(APlayerController* P, const FBozorDaraja& D);
	void Tick(float Dt);
	void MobilDavom() { bMobilDavom = true; }
	void Chiz(UCanvas* C);
	bool Tugadimi() const { return Holat == EHolat::Tugadi; }

private:
	enum class EHolat : uint8 { Jiringlash, Gaplashish, Tugatish, Tugadi };

	void JavobBer();
	void GapBoshla(int32 N);
	void Tugat();

	UPROPERTY() TObjectPtr<UTexture2D> Fon;
	UPROPERTY() TObjectPtr<UTexture2D> QongiroqBelgi;
	UPROPERTY() TObjectPtr<UTexture2D> TugatishBelgi;
	UPROPERTY() TObjectPtr<USoundBase> Jiring;
	UPROPERTY() TObjectPtr<USoundBase> Ulandi;
	UPROPERTY() TObjectPtr<USoundBase> Uzildi;
	UPROPERTY() TObjectPtr<USoundClass> SinfUI;
	UPROPERTY() TObjectPtr<USoundClass> SinfOvoz;
	UPROPERTY() TObjectPtr<UAudioComponent> Ovoz;   // onaning Navoiy ovozi (bAutoDestroy=false, Yoqot bilan tozalanadi)

	TWeakObjectPtr<APlayerController> Pc;
	FString Kim;
	TArray<TPair<bool, FString>> Gaplar;

	EHolat Holat = EHolat::Jiringlash;
	double Vaqt = 0.0;            // o'yin vaqti (pauza bo'lsa to'xtaydi)
	double HolatVaqt = 0.0;
	double KeyingiJiring = 0.0;
	double UlanganVaqt = 0.0;
	int32 Gap = -1;
	double GapVaqt = 0.0;
	float GapDavomi = 0.f;        // matn kamida shuncha s ko'rinadi
	int32 OvozKutilyapti = -1;    // shu gap uchun Navoiy ovozi so'ralgan
	float Chiqish = 0.f;          // 0 - cho'ntakda, 1 - ekranda
	bool bTest = false;
	bool bMobilDavom = false;
};
