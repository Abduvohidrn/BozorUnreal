#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BozorAvtobus.generated.h"

class ACameraActor;
class ACharacter;
class APlayerController;
class UMaterialInterface;
class USplineComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UTextRenderComponent;

// Avtobus bekati: Toshkentdagi yangi bekatlar kabi - shisha devorli, o'rindiqli soyabon, ko'k "Avtobus bekati" belgisi (yo'nalish
// raqami va bekat nomi bilan) va soyabon tagidagi elektron tablo (qaysi avtobus necha daqiqada keladi). Soyabon va ustun -
// Fab "Bus Stop Shelters & Signs Pack" (Scripts/bozor_bekat.py import qiladi). Aktor oldi (+X) - yo'l tomoni
UCLASS()
class BOZORUNREAL_API ABozorBekat : public AActor
{
	GENERATED_BODY()

public:
	ABozorBekat();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	FString BekatNomi = TEXT("Dehqon bozori");

	// bekatdan o'tadigan yo'nalishlar
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	FString Yonalishlar = TEXT("4  31  67");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	FString Tablo = TEXT("4   Chorsu      hozir\n31  Yunusobod   5 daq\n67  Sergeli    12 daq");

	// soyabon modelining o'z burilishi: ochiq tomoni aktorning +X'iga qarasin
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	float SoyabonBurilish = 0.f;

	// belgi ustuni soyabonning qaysi yonida: +1 - aktorning +Y tomoni
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	float UstunTomon = 1.f;

	// true - soyabon, ustun, belgi va tablo soyabon o'lchamiga qarab avtomatik joylanadi (bir marta), keyin o'zi false bo'ladi:
	// qismlarni muharrirda qo'lda surish mumkin va ular joyida qoladi. Qayta avtomatik joylash uchun belgini qo'ying
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	bool bAvtoJoylash = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	TObjectPtr<UMaterialInterface> KokMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	TObjectPtr<UMaterialInterface> QoraMaterial;

	// belgi ustuni (metall)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	TObjectPtr<UMaterialInterface> UstunMaterial;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bozor")
	TObjectPtr<UStaticMeshComponent> Soyabon;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bozor")
	TObjectPtr<UStaticMeshComponent> Oyna;

	// ustun kubdan yasaladi (ko'k belgi uchun oddiy metall quvur)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bozor")
	TObjectPtr<UStaticMeshComponent> Ustun;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bozor")
	TObjectPtr<UStaticMeshComponent> Belgi;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bozor")
	TObjectPtr<UStaticMeshComponent> TabloQuti;

	virtual void OnConstruction(const FTransform& Transform) override;

private:
	UPROPERTY() TObjectPtr<UTextRenderComponent> BelgiMatn;
	UPROPERTY() TObjectPtr<UTextRenderComponent> BelgiOrqa;
	UPROPERTY() TObjectPtr<UTextRenderComponent> NomMatn;
	UPROPERTY() TObjectPtr<UTextRenderComponent> TabloMatn;
};

// O'yin boshlanishi: o'yinchi avtobusda keladi. Avtobus (sahnadagi BP_City_Bus) Kelish yo'li bo'ylab yuradi, bekatda sekinlab
// to'xtaydi, eshiklarini ochadi; o'yinchi o'rta eshikdan bekatga tushadi va boshqaruv unga o'tadi. Avtobus eshiklarini yopib,
// Ketish yo'li bilan jo'nab ketadi va yo'qoladi.
// Kino kadr (asosiy): hammasi tashqi kameralardan ko'rinadi - 1) yo'l chetidan kelayotgan avtobus, 2) yo'lakdan o'rta eshik va
// tushayotgan bola, 3) bola orqasidan: avtobus ketib bozor ochiladi; keyin kamera bola ko'ziga o'tadi. Bola tanasi o'z ko'zidan
// yashirin, tashqi kamerada ko'rinadi. Bo'sh joy yoki Enter - intro o'tkazib yuboriladi.
// -Avtobus1Shaxs: eski variant - butun intro bola ko'zidan (Arxiv/Avtobus_1shaxs_2026-11-04).
// Avtobus fizikasiz (kinematik) yuradi - to'xtash joyi aniq bo'ladi; g'ildiraklar bosib o'tilgan masofaga qarab aylantiriladi.
// -AvtobusYoq (va boshqa avto-sinovlar: -SotuvTest, -BozorAutoTest, -BozorQadamTest) - intro bo'lmaydi, avtobus yashiriladi.
// -AvtobusTest: intro bosqichlarida skrinshot oladi (Saved/Screenshots/AvtobusTest) va oxirida o'yindan chiqadi.
UCLASS()
class BOZORUNREAL_API ABozorAvtobusIntro : public AActor
{
	GENERATED_BODY()

public:
	ABozorAvtobusIntro();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	TObjectPtr<AActor> Avtobus;

	// dunyo nuqtalari: avtobus markazi (yer sathida) shu yo'ldan yuradi; oxirgisi - bekatdagi to'xtash joyi
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	TArray<FVector> KelishNuqtalari;

	// bekatdan keyin; birinchisi to'xtash joyidan keyingi nuqta
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	TArray<FVector> KetishNuqtalari;

	// sm/s (~32 km/soat)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	float Tezlik = 900.f;

	// old va orqa o'q orasidagi masofa, sm (shahar avtobusi ~6 m): burchak shu ikki nuqtadan hisoblanadi
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	float OqOraligi = 600.f;

	// avtobus burchagi o'zgarishining eng katta tezligi, gradus/s
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	float MaxBurilishTezligi = 25.f;

	// raqam plitasi materiali (Toshkent raqami; Scripts/yozuv_yangila.py yaratadi). Bo'lsa - BP'ning raqam matnlari yashiriladi
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	TObjectPtr<UMaterialInterface> RaqamMaterial;

	// tormoz, sm/s2: bekatga shu sekinlanish bilan yaqinlashadi
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	float Tormoz = 160.f;

	// jo'nab ketishdagi tezlanish, sm/s2
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	float Tezlanish = 120.f;

	// to'xtagach eshik ochilguncha va ochilgach tushishgacha, s
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	FVector2D EshikKutish = FVector2D(0.8f, 1.6f);

	// avtobus poli yerdan, sm (past polli avtobus)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	float PolBalandligi = 36.f;

	// o'yinchi yo'lda o'rta eshikdan ichkarida shuncha sm turadi
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	float IchkariChuqurlik = 110.f;

	// tushgach eshikdan yo'lakka shuncha sm yuradi (soyabon oldida to'xtaydi)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	float YolakkaMasofa = 105.f;

	// tushgach o'yinchi shu nuqtaga (bozor darvozasiga) burilib qaraydi
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	FVector QarashNuqtasi = FVector(0.f, 0.f, 250.f);

	// tushishdagi yurish tezligi, sm/s
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	float TushishTezligi = 150.f;

	// false - butun intro bola ko'zidan (-Avtobus1Shaxs ham shunday qiladi)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	bool bKinoKadr = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	float KinoFOV = 55.f;

	// 3-kadr: avtobus jo'nagandan (Ketish boshidan) shuncha s keyin kamera bola ko'ziga o'ta boshlaydi va qancha davom etadi
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	FVector2D KozgaOtish = FVector2D(5.f, 1.4f);

	UPROPERTY(VisibleAnywhere, Category = "Bozor")
	TObjectPtr<USplineComponent> Kelish;

	UPROPERTY(VisibleAnywhere, Category = "Bozor")
	TObjectPtr<USplineComponent> Ketish;

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// intro o'ynalyapti va boshqaruv hali o'yinchiga berilmagan (HUD shu paytda hech narsa ko'rsatmaydi)
	bool BoshqaruvKutilmoqda() const { return bOynaldi && !bBoshqaruvBerildi; }
	bool Oynaldi() const { return bOynaldi; }

private:
	enum class EBosqich : uint8 { Kutish, Kelish, Toxtadi, EshikOchiq, Tushish, Ketish, Tugadi };

	void YolQur(USplineComponent* S, const TArray<FVector>& P) const;
	void AvtobusniQoy(USplineComponent* S, float Masofa, float Dt);
	void GildirakAylantir(float Yol);
	void Eshiklar(bool bOch);
	bool OyinchiniOl();
	void OyinchiniQoyibYubor(bool bBoshqaruv = true);
	FVector TushishOxiri(ACharacter* C) const;   // yo'lakdagi joy (kapsula markazi)
	void Kadr(int32 N);
	void KinoTick(float Dt);
	void Otkazish();
	void Bosqich(EBosqich B);
	void Surat(const TCHAR* Nom);
	FTransform AvtobusTransform() const;
	void AvtobusniYoqot();

	EBosqich Holat = EBosqich::Kutish;
	double BosqichVaqt = 0.0;
	float Yol = 0.f, V = 0.f;
	float GildirakBurchak = 0.f;
	bool bTest = false;
	int32 TushishQadam = 0;
	FVector EshikMahalliy = FVector::ZeroVector;   // o'rta eshik markazi, avtobus o'qlarida (sm)
	TArray<FVector> TushishYoli;
	UPROPERTY(Transient) TObjectPtr<AActor> TestKamera;
	UPROPERTY(Transient) TObjectPtr<ACameraActor> KinoKamera;
	int32 KinoKadrN = 0;          // 0 - kino yo'q yoki tugagan
	bool bKozgaOtdi = false;
	bool bBoshqaruvBerildi = false;
	bool bOynaldi = false;
	float Yaw = 0.f;
	bool bYawBor = false;
	float Burilish = 0.f;      // old g'ildiraklar, gradus
	float MaxYawQadam = 0.f;   // sinov: bir kadrdagi eng katta burilish

	struct FGildirak
	{
		TWeakObjectPtr<UStaticMeshComponent> C;
		FTransform Asl;      // ota-komponentga nisbatan, aylanishsiz
		FTransform Qoyilgan; // oxirgi qo'yganimiz: BP uni o'zgartirsa - yangi asl holat
		FVector Markaz;      // ota-komponent o'qlarida
		float Radius = 50.f;
		bool bBor = false;
		bool bOld = false;   // old (buriladigan) g'ildirak
	};
	TArray<FGildirak> Gildiraklar;

	TWeakObjectPtr<ACharacter> Oyinchi;
	TWeakObjectPtr<APlayerController> Pc;
};
