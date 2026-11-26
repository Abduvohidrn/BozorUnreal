#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BozorQatnov.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class USplineComponent;
class UAudioComponent;
class USoundBase;
class USoundAttenuation;

// Qatnovdagi mashina turi: kuzov (Nanite) va oyna (shaffof, alohida mesh). Mesh pivoti - tagining markazi.
USTRUCT(BlueprintType)
struct FBozorMashinaTuri
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	TObjectPtr<UStaticMesh> Kuzov;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	TObjectPtr<UStaticMesh> Oyna;

	// mesh old tomonining lokal yo'nalishi (gradus): +X = 0, +Y = 90, -X = 180, -Y = -90
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	float OldYaw = 0.f;

	// tanlanish ulushi (Toshkentda Cobalt/Nexia/Spark ko'p, Malibu kam)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	float Ulush = 1.f;

	// aylanadigan g'ildiraklar (pivoti - o'q markazi); Kuzov bu holda g'ildiraksiz mesh bo'ladi
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	TArray<TObjectPtr<UStaticMesh>> Gildiraklar;

	// g'ildiraklar markazi kuzov mesh'ining lokal fazosida, sm (Gildiraklar bilan bir tartibda)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	TArray<FVector> GildirakJoylari;

	// g'ildirak radiusi, sm (aylanish burchagi = yo'l / radius)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	float GildirakRadius = 30.f;
};

// Bo'lak: mashinalar shu nuqtalar orqali o'tgan silliq yo'ldan yuradi (dunyo, yer sathida).
// Birinchi nuqtada paydo bo'ladi, oxirgisida yo'qoladi (ikkalasida ham o'yinchi ko'rmasa).
USTRUCT(BlueprintType)
struct FBozorYolak
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	TArray<FVector> Nuqtalar;

	// bo'lakdagi eng ko'p mashina
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	int32 MaxMashina = 4;

	// avtobus (intro) shu bo'lakdan yuradi: u ko'chada ekan, bo'lak boshida yangi mashina paydo bo'lmaydi
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	bool bAvtobusYoli = false;
};

// Tartibga solinmagan piyodalar o'tish joyi ("zebra"): dunyo o'qlariga parallel quti (markaz va yarim o'lcham, sm)
USTRUCT(BlueprintType)
struct FBozorOtishJoyi
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	FVector Markaz = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	FVector Yarim = FVector(500.f, 200.f, 150.f);
};

// Bozor ko'chasidagi mashinalar qatnovi (O'zbekiston YHQ asosida):
//  - o'ng tomonlama harakat, aholi punktida 60-70 km/soat, bozor yonida amalda 35-45 km/soat;
//  - tartibga solinmagan piyodalar o'tish joyida piyodaga yo'l beradi: piyoda o'tish joyida yoki unga chiqmoqchi bo'lib
//    chetida tursa - o'tish joyi oldida to'xtaydi, piyoda o'tib bo'lgach yo'lga tushadi (YHQ 109-110);
//  - yo'lda (o'tish joyidan tashqarida) piyoda bo'lsa ham urib ketmaydi - oldida to'xtaydi;
//  - oldidagi mashina/avtobus bilan xavfsiz masofa (IDM - aqlli haydovchi modeli).
// Mashinalar kinematik (fizikasiz) - mesh komponentlari shu aktorga biriktirilgan; o'yinchi va NPC ular ichidan o'tmaydi.
// -QatnovYoq: qatnov o'chadi. -QatnovTest: o'yinchi o'tish joyiga qo'yiladi, suratlar (Saved/Screenshots/QatnovTest), keyin chiqish.
UCLASS()
class BOZORUNREAL_API ABozorQatnov : public AActor
{
	GENERATED_BODY()

public:
	ABozorQatnov();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	TArray<FBozorMashinaTuri> Turlar;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	TArray<FBozorYolak> Yolaklar;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	TArray<FBozorOtishJoyi> OtishJoylari;

	// istalgan tezlik oralig'i, km/soat (har mashinaga tasodifiy)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	FVector2D TezlikKmSoat = FVector2D(35.f, 45.f);

	// piyoda o'tish joyiga yaqinlashganda (yonida piyoda bo'lsa) tezlik, km/soat
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	float OtishOldiTezlik = 20.f;

	// burilishda yon tezlanish chegarasi, sm/s2 (shahar haydovchisi ~2 m/s2: 6 m radiusli burilish ~12 km/soat)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	float YonTezlanish = 180.f;

	// kuzov og'ishi: gradus / (sm/s2) - tormozda old tomoni cho'kadi, burilishda tashqariga og'adi
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	float PitchKoef = 0.0028f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	float RollKoef = 0.004f;

	// prujina (osma) chastotasi, Hz va so'nish nisbati: kuzov bir oz chayqalib to'xtaydi
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	float OsmaChastota = 1.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	float OsmaSonish = 0.35f;

	// old g'ildiraklarning eng katta burilish burchagi, gradus
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	float MaxRul = 35.f;

	// tezlanish va qulay tormoz, sm/s2
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	float Tezlanish = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	float Tormoz = 350.f;

	// favqulodda tormozning chegarasi, sm/s2
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	float MaxTormoz = 900.f;

	// to'xtaganda oldidagidan masofa, sm va vaqt oralig'i, s
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	float MinOraliq = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	float VaqtOraligi = 1.3f;

	// to'xtash chizig'i o'tish joyi chetidan shuncha oldinda, sm (mashina undan yana MinOraliq oldin to'xtaydi - jami ~2.5 m)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	float ToxtashChizigi = 0.f;

	// piyoda o'tish joyi chetidan (yo'lak tomonda) shuncha ichkarida tursa ham "o'tmoqchi" hisoblanadi, sm
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	float PiyodaZahira = 150.f;

	// piyoda o'tib bo'lgach yo'lga tushishdan oldin kutish, s
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	float JonashKutish = 0.8f;

	// bo'lak boshida yangi mashina paydo bo'lish oralig'i, s
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	FVector2D PaydoOraligi = FVector2D(4.f, 11.f);

	// o'yinchi shu masofadan yaqin bo'lsa (yoki nuqtani ko'rib tursa) mashina paydo bo'lmaydi/yo'qolmaydi, sm
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	float KorinmasMasofa = 2500.f;

	// dvigatel ovozi (loop); balandlik (pitch) va baland-pastligi tezlikka qarab o'zgaradi
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	TObjectPtr<USoundBase> DvigatelOvozi;

	// pitch: to'xtaganda X, istalgan tezlikda Y (dizel avtobus namunasidan yengil mashina ovozi)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	FVector2D DvigatelPitch = FVector2D(1.05f, 1.75f);

	// ovoz balandligi: to'xtaganda X, istalgan tezlikda Y
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	FVector2D DvigatelBalandligi = FVector2D(0.25f, 0.55f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	TObjectPtr<USoundAttenuation> OvozSonishi;

	// signal: yo'lda (o'tish joyidan tashqarida) piyoda oldini to'sib tursa, shuncha soniyadan keyin qisqa signal beradi
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	TObjectPtr<USoundBase> SignalOvozi;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	float SignalKutish = 2.5f;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float Dt) override;

private:
	struct FMashina
	{
		int32 Yolak = 0;
		int32 Tur = 0;
		float S = 0.f;          // bo'lak bo'ylab markazi, sm
		float V = 0.f;          // sm/s
		float V0 = 1100.f;      // istalgan tezlik, sm/s
		float Uzunlik = 400.f;
		float Kenglik = 180.f;
		float Kutish = 0.f;     // piyoda o'tgach kutish qoldig'i, s
		bool bToxtagan = false; // o'tish joyi oldida to'xtab turibdi (log uchun)
		float Burchak = 0.f;    // g'ildiraklar aylanish burchagi, gradus
		float YoldaKutish = 0.f;   // yo'ldagi piyoda oldida turgan vaqt, s (signal uchun)
		int32 Signallar = 0;
		float SignalTugash = -1.f; // signal ovozi shu vaqtda to'xtatiladi
		// "fizika": old g'ildiraklar burilishi va kuzovning prujinali og'ishi (tormoz/tezlanishda pitch, burilishda roll)
		float Rul = 0.f;           // old g'ildiraklar burilish burchagi, gradus (+ o'ngga)
		float Pitch = 0.f, PitchV = 0.f;
		float Roll = 0.f, RollV = 0.f;
		float Tezlanish = 0.f;     // oxirgi kadrdagi bo'ylama tezlanish, sm/s2
		float YonTezlanish = 0.f;  // markazga intilma tezlanish, sm/s2 (+ o'ngga burilish)
		TObjectPtr<UStaticMeshComponent> Kuzov;
		TObjectPtr<UStaticMeshComponent> Oyna;
		TObjectPtr<UStaticMeshComponent> Raqam;
		int32 RaqamIndeksi = INDEX_NONE;
		TArray<TObjectPtr<UStaticMeshComponent>> Gildiraklar;
		TObjectPtr<UAudioComponent> Dvigatel;
		TObjectPtr<UAudioComponent> Signal;
	};

	UPROPERTY() TArray<TObjectPtr<USplineComponent>> Yollar;
	UPROPERTY() TArray<TObjectPtr<USceneComponent>> Komponentlar;   // GC ushlab tursin
	TArray<FMashina> Mashinalar;
	TArray<float> PaydoTaymer;
	TArray<FVector> Piyodalar;   // har kadr: o'yinchi va NPC'lar (oyoq sathi)
	TWeakObjectPtr<AActor> Avtobus;
	float TestVaqt = 0.f;
	int32 TestQadam = 0;
	bool bTest = false;
	bool bKadr = false;
	TWeakObjectPtr<UStaticMeshComponent> TomMashina;
	TWeakObjectPtr<class ACameraActor> KadrKamera;
	int32 KadrJoy = -1;
	float KadrOxirgi = -10.f;
	void KadrTick(float Dt);

	bool Qosh(int32 Yolak, float S, bool bBoshlangich);
	void Joyla(FMashina& M) const;
	void Fizika(FMashina& M, float Dt);
	void Ovoz(FMashina& M, float Dt);
	void Olib(FMashina& M);
	bool Korinadi(const FVector& P) const;
	bool OtishJoyidaPiyoda(const FBozorOtishJoyi& O) const;
	float Tezlanishi(const FMashina& M, float Oraliq, float OldV) const;
	void TestTick(float Dt);
};
