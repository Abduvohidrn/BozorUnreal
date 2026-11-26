#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "BozorHud.generated.h"

class USoundBase;
class UBozorHamyon;
class UBozorMobil;

// Xarid ro'yxati qog'ozi. Intro tugagach bola uni cho'ntagidan chiqaradi: qog'oz ekran pastidan o'ng tomonga ko'tariladi
// (ro'yxat, pul, onaning eslatmasi). Birinchi marta - E/Enter yoki yura boshlasa yopiladi; keyin Tab bosib turilganda ochiq.
// Qog'oz yopilgach - boshqaruv maslahati (pastda, o'yinchi yurib ko'rgach yo'qoladi) va bozor darvozasi belgisi (yetib borguncha).
// Ma'lumot o'yinchi hamyonida (UBozorHamyon::Royxat, Pul). Hozircha Canvas bilan chiziladi
UCLASS()
class BOZORUNREAL_API ABozorHud : public AHUD
{
	GENERATED_BODY()

public:
	ABozorHud();

	virtual void Tick(float DeltaSeconds) override;
	virtual void DrawHUD() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void Qogoz(bool bOch);

	// ona bilan telefon suhbati davom etyapti (E/Enter shu paytda telefonga tegishli)
	bool TelefonFaol() const { return Telefon != nullptr; }
	bool BoshqaruvFaol() const { return bFaol; }
	void MobilRoyxat();
	void MobilAmal();


private:
	UBozorHamyon* Hamyon() const;
	void QogozChiz(float Ochiqlik);
	void MaslahatChiz();
	void BelgiChiz();
	// o'rgatish: o'yinchi biror narsani birinchi marta uchratganda tepada "nima qilish kerak" kartasi
	void OrgatishYangila(float Dt);
	void OrgatishChiz();
	// sotuvchi oldida "E — Gaplashish" belgisi va suhbat oynasi (sotuvchi gapi)
	void SotuvChiz();
	// ro'yxat tugagach: chiqishga qaytish belgisi; yetib borganda - natija
	void QaytishniTekshir();
	void NatijaChiz();

	bool bQaytish = false;
	double FaolVaqti = -100.0;   // HUD ishga tushgan payt (daraja nomi 5 s ko'rinadi)
	double QaytishVaqti = 0.0;
	FVector ChiqishJoyi = FVector::ZeroVector;
	FVector BoshJoy = FVector::ZeroVector;   // o'yinchi o'yinni boshlagan joy (intro bo'lmasa chiqish shu)
	bool bBoshJoy = false;

	UPROPERTY() TObjectPtr<USoundBase> Shitir;
	UPROPERTY() TObjectPtr<UBozorMobil> Mobil;
	bool bMobilRoyxat = false;

	// darajada qo'ng'iroq bo'lsa: intro tugagach ona telefon qiladi, qog'oz suhbatdan keyin chiqadi
	UPROPERTY() TObjectPtr<class UBozorTelefon> Telefon;
	double QongiroqTestVaqt = -1.0;   // -QongiroqTest: ro'yxat surati olinib, o'yin yopiladi
	bool bQongiroqSurat = false;


	bool bFaol = false;       // intro tugadi, HUD ishlaydi
	bool bOchiq = false;
	bool bBirinchi = false;   // introdan keyingi birinchi ochilish
	bool bTest = false;
	bool bTestSurat = false;
	float Ochiqlik = 0.f;     // 0 - cho'ntakda, 1 - ko'z oldida
	double OchilganVaqt = 0.0;

	// boshqaruv maslahati va bozor belgisi (faqat intro o'ynalgan bo'lsa)
	bool bIntroBor = false;
	FVector BozorJoyi = FVector::ZeroVector;
	double MaslahatBosh = -1.0;   // ko'rsatila boshlagan vaqt
	FVector MaslahatJoy = FVector::ZeroVector;   // o'sha paytda o'yinchi qayerda edi
	bool bMaslahatTugadi = false;
	float MaslahatAlfa = 0.f;
	bool bBelgi = false;
	float BelgiAlfa = 0.f;
	bool bMaslahatSurat = false;

	// o'rgatish kartasi
	bool bOrgatishOchiq = true;   // sinov/bot rejimlarida o'chiq
	int32 OrgatishJoriy = INDEX_NONE;   // hozir ko'rsatilayotgan maslahat (BozorHud.cpp dagi ro'yxatda)
	double OrgatishBosh = 0.0;
	float OrgatishAlfa = 0.f;
	int32 OrgatishOxirgi = INDEX_NONE;   // so'nib borayotgan karta (alfa 0 ga tushguncha chiziladi)
	FVector OrgatishJoy = FVector::ZeroVector;
};
