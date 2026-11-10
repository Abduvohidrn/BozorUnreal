#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BozorOvoz.generated.h"

class ABozorNpc;
class UAudioComponent;
class USoundBase;

// Rastadagi ovoz nuqtasi: rekvizit (tarozi, qop, idish) yoki sotuvchining kassasi. Kinds - shu nuqtada chalinadigan turlar
USTRUCT(BlueprintType)
struct FBozorRastaNuqta
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	TArray<FName> Kinds;

	// ikki ovoz orasidagi vaqt, s
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	FVector2D Gap = FVector2D(10.f, 30.f);

	double NextTime = 0.0;
};

// Rastalardagi nuqtaviy ovozlar (BozorKit/Audio/tools/generate_sfx.py, import_audio.py): sfx_<tur>_<nn> -
// tarozi_pip, tarozi_tosh, qop, idish, tanga, pul, paket.
// Ikki manba:
//   - nuqtalar (Points, Scripts/bozor_ovoz.py sahnadagi rekvizitlardan qo'yadi): o'yinchidan ActiveRadius ichidagilari
//     o'z vaqtida tasodifiy chalinadi - elektron tarozi "pip"lari, posangi, qopdan cho'mich, piyola jaranglashi, kassa;
//   - savdo (ABozorNpc::DealWith): tortish -> eng yaqin tarozi, uzatish -> paket, to'lash -> tanga/pul.
// Ovoz bir martalik (PlaySoundAtLocation): so'nish - Att_SFX, bir vaqtdagi soni - Con_SFX (kliplarning o'zida).
UCLASS()
class BOZORUNREAL_API ABozorRastaOvozlari : public AActor
{
	GENERATED_BODY()

public:
	ABozorRastaOvozlari();

	static void Play(const UObject* WorldContext, FName Kind, const FVector& Location, AActor* Source = nullptr);
	// Radius ichidagi Kinds turli eng yaqin nuqtada chaladi; topilmasa Fallback'ni Location'da
	static void PlayNear(const UObject* WorldContext, const TArray<FName>& Kinds, const FVector& Location, float Radius, FName Fallback, AActor* Source = nullptr);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	TArray<TObjectPtr<USoundBase>> Clips;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	TArray<FBozorRastaNuqta> Points;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor", meta = (ClampMin = "0", ClampMax = "2"))
	float Volume = 1.f;

	// nuqtalar faqat o'yinchidan shu masofa ichida o'z-o'zidan chalinadi (uzoqdagisi baribir eshitilmaydi), sm
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	float ActiveRadius = 1800.f;

	// hamma nuqtalar Gap'ining umumiy ko'paytuvchisi: bozor juda shovqinli bo'lsa oshiring
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	float GapScale = 1.f;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;

private:
	void PlayKind(FName Kind, const FVector& Location, AActor* Source);

	static TWeakObjectPtr<ABozorRastaOvozlari> Instance;
	TMap<FName, TArray<USoundBase*>> ByKind;
	TMap<FName, USoundBase*> LastClip;
	double NextScan = 0.0;
};

// O'yinchi qadamlari sirt bo'yicha (generate_sfx.py -> qadam_<sirt>_<nn>: beton, asfalt, maysa, tuproq, yogoch, metall).
// Qadam oyoq yerga tekkan paytda chalinadi: FootBones suyagining qahramon tagidan balandligi pastga tushib, eng past nuqtadan
// qayta ko'tarila boshlagan payt - shu past nuqta PlantHeight'dan past va oldingi cho'qqidan kamida MinLift pastda bo'lsa
// (qat'iy chegara emas: sekin yurishda tovon baland, yugurishda past tushadi). Sirt - oyoq ostiga nur (complex trace) tushirib, tekkan materialning
// Physical Material -> Surface Type -> PhysicsSettings'dagi nomi (import_audio.py SIRTLAR). Suyak topilmasa - har StrideLength sm da;
// suyakdan qadam chiqmay qolsa (juda sekin yurish) - tezlikka qarab qisqaradigan masofa bo'yicha (to'liq tezlikda 1.5 x StrideLength).
// Tezroq yurganda balandroq, sakrab tushganda ikki oyoq birga. Har qadam: oxirgi ikki klipdan boshqasi, balandlik ±10%, pitch 0.92-1.08,
// HPF/LPF (bola yengil qadami, chastotasi har safar biroz boshqa); oyoq ostidan chalinadi, yaqinda Att_Qadam uni qisman 2D'ga
// aralashtiradi (oyoq ostidagi manba chap-o'ng sakramasin). Blueprint'ga tegmaydi: o'yinchi qahramonini o'zi topadi.
// -BozorQadamTest: o'yinchini 25 s oldinga yurgizadi (avval sekin, keyin yugurib), har qadamni loglaydi va o'yindan chiqadi.
UCLASS()
class BOZORUNREAL_API ABozorQadamOvozlari : public AActor
{
	GENERATED_BODY()

public:
	ABozorQadamOvozlari();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	TArray<TObjectPtr<USoundBase>> Clips;

	// to'liq yurish tezligidagi balandlik (har qadamda ±10%, sekin yurganda ~60%); 0.8 o'z qadami uchun juda baland edi
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor", meta = (ClampMin = "0", ClampMax = "2"))
	float Volume = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	TArray<FName> FootBones = { TEXT("foot_l"), TEXT("foot_r") };

	// suyakning qahramon tagidan balandligi, sm: oyoqning eng past nuqtasi shundan past bo'lsa - qadam
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	float PlantHeight = 28.f;

	// oyoq oldingi qadamdan beri kamida shuncha ko'tarilib tushgan bo'lishi kerak (joyida chayqalish qadam emas), sm;
	// to'liq yurish tezligida - shu, sekinroq yurganda tezlikka mutanosib kamayadi (eng kami 30%)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	float MinLift = 5.f;

	// bitta oyoqning ikki qadami orasida kamida shuncha s (tovondan panjaga o'tishdagi ikkinchi "cho'kish" qadam emas)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	float MinInterval = 0.3f;

	// bundan sekin (sm/s) - qadam yo'q (joyida burilish)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	float MinSpeed = 20.f;

	// yugurish tezligi: sakrab tushish qadami shu tezlikda deb olinadi, test logida yurish/yugurish chegarasi (x0.6).
	// Qadam balandligi esa CharacterMovement MaxWalkSpeed'ga nisbatan (u bo'lmasa - shunga)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	float RunSpeed = 500.f;

	// oyoq suyagi topilmasa: har shuncha sm da bitta qadam
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	float StrideLength = 75.f;

	// Physical Material'i yo'q sirt
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	FName FallbackSurface = TEXT("beton");

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	void Step(class ACharacter* C, const FVector& Foot, float Speed, float Gain);
	FName SurfaceAt(class ACharacter* C, const FVector& Foot, FVector& OutPoint) const;
	void UpdateTest(class ACharacter* C, double Now);

	TMap<FName, TArray<USoundBase*>> ByKind;
	TMap<FName, USoundBase*> LastClip;
	struct FFoot
	{
		float Prev = 0.f;           // oldingi kadrdagi balandlik
		float Peak = 0.f;           // oxirgi qadamdan beri eng baland
		bool bDown = false;         // hozir pastga tushyapti
		double LastStep = 0.0;
		FVector2D Walk = FVector2D(1e6, -1e6), Run = FVector2D(1e6, -1e6);   // test logi: min/max
	};
	TArray<FFoot> Feet;
	float Stride = 0.f;
	bool bWasFalling = false;
	bool bTest = false;
	double TestStart = 0.0;
	TMap<FName, int32> TestSteps;
};

// Sotuvchilarning o'zbekcha gaplari: Unity BozorSotuvchiOvozi.cs ning Unreal ko'chirmasi.
// Kliplar (BozorKit/Audio/tools/generate_sotuvchi.py, import_audio.py): sot_<tur>_<bolim>_<persona>_<nn> - chaqiriq, salom, savdo, rahmat.
// Har sotuvchiga bitta persona biriktiriladi (ayolga a1..a2, erkakka e1..e2; keksalarga a3/e2) - u hamma gapini shu ovozda aytadi;
// qo'shni sotuvchilar navbatma-navbat boshqa personani oladi. Bo'lim (sabzavot, non, ziravor...) sotuvchining turgan joyidan.
//
// ABozorNpc o'zi chaqiradi: V_Call -> chaqiriq, Greet -> salom, V_Talk -> savdo, V_Thanks -> rahmat.
// Bundan tashqari o'yinchi atrofidagi bo'sh sotuvchilar vaqti-vaqti bilan o'zi ham baqirib qo'yadi: bozor jim turmasin
// (V_Call faqat oldidan kimdir o'tganda o'ynaydi).
// Ovoz sotuvchining og'zida (3D): so'nish va uzoqdagi gapning bo'g'iqligi - kliplardagi Att_Sotuvchi, bir vaqtdagi gaplar
// soni - Con_Sotuvchi; fon gap paytida biroz pasayadi - SC_Ovoz'ning passiv Mix_Ovoz_Ducking'i.
UCLASS()
class BOZORUNREAL_API ABozorSotuvchiOvozi : public AActor
{
	GENERATED_BODY()

public:
	ABozorSotuvchiOvozi();

	static void Say(ABozorNpc* Vendor, FName Kind, float Chance = 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	TArray<TObjectPtr<USoundBase>> Clips;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor", meta = (ClampMin = "0", ClampMax = "2"))
	float Volume = 1.f;

	// bundan uzoqdagi sotuvchi gapirmaydi (baribir eshitilmaydi), sm
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	float HearDistance = 3000.f;

	// o'yinchi atrofida o'z-o'zidan chaqiriqlar orasidagi vaqt, s
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	FVector2D AmbientGap = FVector2D(4.f, 9.f);

	// Takrorlanmaslik: bitta haqiqiy yozuv (persona nusxalari bitta manba) butun bozor bo'ylab shuncha soniya ichida qayta
	// aytilmaydi; hamma gap yaqinda aytilgan bo'lsa sotuvchi jim turadi - takrordan ko'ra sukut tabiiyroq
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor|Takror")
	float RepeatWindow = 60.f;

	// savdolashuvda har navbatda gapirish ehtimoli ko'paytuvchisi (ABozorNpc 0.6 beradi) va rahmat ehtimoli:
	// haqiqiy bozorda har gap eshitilavermaydi
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor|Takror")
	float SavdoChance = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor|Takror")
	float RahmatChance = 0.6f;

	// eshitiladigan joydagi ikki savdo gapi orasida kamida shuncha s (hamma bir vaqtda savdolashib ketmasin)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor|Takror")
	float SavdoGap = 2.5f;

	// har aytilishda ohang va balandlik biroz boshqacha - bir yozuv qaytsa ham aynan bir xil eshitilmaydi
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor|Takror")
	FVector2D PitchRange = FVector2D(0.95f, 1.05f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor|Takror")
	FVector2D VolumeRange = FVector2D(0.8f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	float AmbientRadius = 2200.f;

	// bitta sotuvchi ikki chaqiriq orasida kamida shuncha jim turadi, s
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	float CallCooldown = 7.f;

	// bir vaqtda eng ko'pi bilan shuncha chaqiriq
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	int32 MaxCalls = 3;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;

	// sotuvchi turgan joydan bo'lim (Unity BozorBuilder rastalari tartibi)
	static FString Bolim(const FVector& Location);

private:
	struct FVoice
	{
		TObjectPtr<UAudioComponent> Src;
		FString Persona, Section;
		double NextTime = 0.0;
		USoundBase* Last = nullptr;
		bool bCall = false;
	};

	bool TrySay(ABozorNpc* Vendor, FName Kind);
	FVoice& Get(ABozorNpc* Vendor);
	USoundBase* Pick(const FVoice& V, FName Kind, double Now) const;
	static FString SourceKey(const USoundBase* Clip);
	TMap<FString, double> Heard;    // manba -> oxirgi aytilgan vaqt
	double NextSavdo = 0.0;
	int32 ActiveCalls() const;
	bool ListenerLocation(FVector& Out) const;

	static TWeakObjectPtr<ABozorSotuvchiOvozi> Instance;
	TMap<FString, TArray<USoundBase*>> ByKey;
	TMap<TWeakObjectPtr<ABozorNpc>, FVoice> Voices;
	TArray<TWeakObjectPtr<ABozorNpc>> Vendors;
	int32 FemaleCount = 0, MaleCount = 0;
	double NextAmbient = 0.0, NextScan = 0.0;
};
