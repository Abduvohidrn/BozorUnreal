#pragma once

#include "CoreMinimal.h"
#include "ChaosVehicleWheel.h"
#include "WheeledVehiclePawn.h"
#include "BozorMashina.generated.h"

class UAudioComponent;
class UCameraComponent;
class USoundAttenuation;
class USoundBase;
class USpringArmComponent;
class UStaticMesh;
class UStaticMeshComponent;
class ACharacter;

// Old g'ildirak: rul bilan buriladi, qo'l tormozi ta'sir qilmaydi (BMW M5 - to'liq privod, dvigatel har to'rttasida)
UCLASS()
class BOZORUNREAL_API UBozorGildirakOld : public UChaosVehicleWheel
{
	GENERATED_BODY()
public:
	UBozorGildirakOld();
};

// Orqa g'ildirak: qo'l tormozi shu g'ildiraklarda
UCLASS()
class BOZORUNREAL_API UBozorGildirakOrqa : public UChaosVehicleWheel
{
	GENERATED_BODY()
public:
	UBozorGildirakOrqa();
};

// O'yinchi haydaydigan mashina (Chaos Vehicle fizikasi). Skelet mesh: kuzov + g'ildirak suyaklari (wheel_fl/fr/rl/rr);
// ko'rinadigan g'ildiraklar - alohida mesh'lar, Chaos g'ildirak holatidan (aylanish, rul, osma) har kadr joylanadi.
// Yoniga kelib "Mashina" tugmasi (standart F) - o'tirish; haydash: W gaz, S tormoz/orqaga, A/D rul, Probel qo'l tormozi,
// sichqoncha - kamera; F (sekin yurganda) - tushish. O'yinchi haydovchi o'rindig'ida ko'rinadi (chap tomonda).
UCLASS()
class BOZORUNREAL_API ABozorMashina : public AWheeledVehiclePawn
{
	GENERATED_BODY()

public:
	ABozorMashina(const FObjectInitializer& ObjectInitializer);

	// g'ildirak mesh'lari, tartibi: wheel_fl, wheel_fr, wheel_rl, wheel_rr; pivoti - g'ildirak markazi
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	TArray<TObjectPtr<UStaticMesh>> GildirakMeshlari;

	// g'ildirak/oyna/raqam mesh'lari old tomoni qayerga qaragan (gradus, mesh lokal: +Y = 90) - mashina oldi +X
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	float MeshOldYaw = 90.f;

	// kuzov materiallari shu mesh'ning bir xil nomli slotlaridan olinadi (skelet mesh asseti materialsiz bo'lsa ham)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	TObjectPtr<UStaticMesh> MaterialManbasi;

	// ko'rinadigan kuzov (g'ildiraksiz statik mesh, Nanite - qatnovdagi bilan bir xil). Berilsa skelet mesh faqat fizika va
	// g'ildirak suyaklari uchun qoladi va ko'rinmaydi (skelet mesh kuzovi o'yinda qorong'i chiqardi)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	TObjectPtr<UStaticMesh> KuzovMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	TObjectPtr<UStaticMesh> OynaMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	TObjectPtr<UStaticMesh> RaqamMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	TObjectPtr<USoundBase> DvigatelOvozi;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	TObjectPtr<USoundBase> SignalOvozi;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	TObjectPtr<USoundAttenuation> OvozSonishi;

	// mashinaga o'tirish uchun o'yinchi shu masofadan yaqin bo'lishi kerak, sm
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	float OtirishMasofasi = 380.f;

	// HUD uchun: yonida - "F - o'tirish", haydayotganda - tezlik va uzatma
	FString Maslahat() const;
	bool Haydalyapti() const { return Haydovchi.IsValid(); }
	void MobilHarakat(const FVector2D& V) { MobilVektor = V; }
	void MobilKamera(const FVector2D& V) { MobilKameraDelta += V; }
	void MobilOtirTush();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float Dt) override;

private:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USpringArmComponent> Qol;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Kamera;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Kuzov;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Oyna;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Raqam;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UAudioComponent> Dvigatel;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UAudioComponent> Signal;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Gildiraklar;
	TArray<FVector> GildirakJoylari;   // komponent fazosida, suyaklardan
	TWeakObjectPtr<ACharacter> Haydovchi;
	float KameraYaw = 0.f, KameraPitch = -12.f, SichqonchaVaqt = 0.f;
	float SignalTugash = -1.f;
	bool bYonida = false;
	FVector2D MobilVektor = FVector2D::ZeroVector;
	FVector2D MobilKameraDelta = FVector2D::ZeroVector;

	void Otir();
	void Tush();
	void GildiraklarniJoyla();
	void Boshqaruv(float Dt);

	// -MashinaTest: o'yinchi mashina yoniga qo'yiladi, o'tiradi, gaz/rul/tormoz, tushadi; suratlar va log, keyin chiqish
	bool bTest = false;
	float TestVaqt = 0.f;
	int32 TestQadam = 0;
	void TestTick(float Dt);
};
