#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BozorFon.generated.h"

class UAudioComponent;
class USoundBase;

// Fon zonasi: bozor rejasidagi to'rtburchak (sm, XY) va undagi qatlamlar balandligi
USTRUCT(BlueprintType)
struct FBozorFonZona
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	FVector2D Min = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	FVector2D Max = FVector2D::ZeroVector;

	// zona umumiy balandligi (asl mix RMS'iga keltirilgan), dB
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	float MasterDb = 0.f;

	// ABozorFonZonalar::Stems tartibida, dB
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	TArray<float> GainsDb;

	float Distance(const FVector2D& P) const
	{
		const double Dx = FMath::Max3(Min.X - P.X, 0.0, P.X - Max.X);
		const double Dy = FMath::Max3(Min.Y - P.Y, 0.0, P.Y - Max.Y);
		return float(FMath::Sqrt(Dx * Dx + Dy * Dy));
	}
};

// Bozorning fon ovozi zonalar bo'yicha: Unity BozorFonZonalar.cs ning Unreal ko'chirmasi.
// Ovozning o'zi - MS_Bozor_Fon (Scripts/bozor_fon.py): 7 ta qatlam (olomon, sotuvchi, qadamlar, shahar, bozor, qushlar, savdo)
// bitta MetaSound'da sinxron aylanadi, har birining balandligi - grafning float kirishi. Bu aktor har kadr o'yinchi qaysi zonada
// ekaniga qarab o'sha kirishlarni qo'yadi: kirishda shahar va qadamlar, sabzavot qatorida savdo va sotuvchilar, ziravor
// qatorida sokinroq, ochiq maydonda qushlar, go'sht pavilyonida tarozi, orqa darvozada arava.
// Zona presetlari BozorKit/Audio/tools/generate_zonalar.py dagi ZONES bilan bir xil (Unity metr -> Unreal sm: X = z, Y = x).
// Har qatlam yana sekin, tasodifiy tebranadi (Perlin, 15-40 s): qatlamlar yig'indisi 60 s da qaytarilib eshitilmaydi.
//
// Bozor maydonidan (Area) tashqarida maydonning eng yaqin chetigacha masofa d bo'yicha:
//   balandlik 1/(1 + d/HalfDistance), FadeStart..MaxDistance oralig'ida nolga tushadi;
//   baland chastotalar so'nadi (low-pass) - uzoqdagi shovqin bo'g'iq eshitiladi;
//   manba maydonning eng yaqin nuqtasiga ko'chadi va asta 3D'ga o'tadi - ovoz bozor tomondan keladi.
UCLASS()
class BOZORUNREAL_API ABozorFonZonalar : public AActor
{
	GENERATED_BODY()

public:
	ABozorFonZonalar();

	static const FName Stems[7];

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	TObjectPtr<USoundBase> Sound;

	// Berilgan bo'lsa zonalar o'rniga shu bitta tayyor mix (looping SoundWave) chalinadi: hamma joyda bir xil, faqat maydondan
	// uzoqlashganda so'nadi/bo'g'iqlashadi va tom ostida MixIndoorDb ga pasayadi. Bo'sh bo'lsa - zonali 7 qatlam (Sound)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	TObjectPtr<USoundBase> Mix;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor|Pavilyon")
	float MixIndoorDb = -3.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor", meta = (ClampMin = "0", ClampMax = "2"))
	float Volume = 0.8f;

	// qatlamlar loop uzunligi (s): o'yin har safar tasodifiy joydan boshlanadi
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	float LoopLength = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	float FadeIn = 2.f;

	// zona chegarasidan shuncha sm gacha qo'shni zona aralashadi
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	float Blend = 400.f;

	// zona almashganda qatlamlar shu vaqt doimiysi bilan o'tadi, s
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	float Smoothing = 1.5f;

	// har qatlamning sekin tasodifiy tebranishi, dB
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	float WobbleDb = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	TArray<FBozorFonZona> Zones;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor|Masofa")
	FBox Area = FBox(FVector(0.f, -1300.f, 0.f), FVector(4400.f, 1300.f, 400.f));

	// shu masofada (sm) balandlik yarmiga tushadi (-6 dB)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor|Masofa")
	float HalfDistance = 1000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor|Masofa")
	float FadeStart = 6000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor|Masofa")
	float MaxDistance = 12000.f;

	// low-pass kesish chastotasi har shu masofada e marta pasayadi
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor|Masofa")
	float MuffleDistance = 4000.f;

	// maydondan shu masofada ovoz to'liq 3D (yo'nalishli); maydon ichida - har tomondan
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor|Masofa")
	float SpatialDistance = 1500.f;

	// tom ostida (o'yinchi pavilyonning AAudioVolume'i ichida): tashqi qatlamlar shuncha dB pasayadi, Stems tartibida.
	// Pavilyonlar (bozor_ovoz.py) gaplar va rasta ovozlarini o'zi bo'g'adi; fon manbai o'yinchi bilan yurgani uchun
	// AudioVolume uni doim "ichkarida" deb biladi - shahar va qushlarni shu yerda pasaytiramiz
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor|Pavilyon")
	TArray<float> IndoorDb = { 0.f, 0.f, 0.f, -8.f, -3.f, -9.f, 0.f };

	// ichkariga kirib-chiqqanda o'tish vaqti doimiysi, s
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor|Pavilyon")
	float IndoorTime = 0.8f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Bozor")
	FString CurrentZone;

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Bozor")
	TObjectPtr<UAudioComponent> Audio;

	TArray<float> Gain, Weight, Seed;
	double StartTime = 0.0;
	bool bPrimed = false;
	float Indoor = 0.f;             // 0 - ochiq osmon ostida, 1 - tom ostida (silliq)
	bool bIndoor = false;

	void UpdateWeights(const FVector2D& P);
	float UpdateDistance(const FVector& Listener);
};
