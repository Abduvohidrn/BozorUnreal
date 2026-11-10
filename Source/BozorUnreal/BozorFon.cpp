#include "BozorFon.h"

#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Sound/AudioVolume.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogBozorFon, Log, All);

const FName ABozorFonZonalar::Stems[7] = { "olomon", "sotuvchi", "qadamlar", "shahar", "bozor", "qushlar", "savdo" };

namespace
{
	float Db(float InDb) { return FMath::Pow(10.f, InDb / 20.f); }

	// Unity zona (metr): x0..x1, z0..z1 -> Unreal sm: X = z, Y = x
	FBozorFonZona Zona(const TCHAR* Name, float X0, float X1, float Z0, float Z1, float Master, TArray<float> Gains)
	{
		FBozorFonZona Z;
		Z.Name = Name;
		Z.Min = FVector2D(Z0, X0) * 100.f;
		Z.Max = FVector2D(Z1, X1) * 100.f;
		Z.MasterDb = Master;
		Z.GainsDb = MoveTemp(Gains);
		return Z;
	}
}

ABozorFonZonalar::ABozorFonZonalar()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Audio = CreateDefaultSubobject<UAudioComponent>(TEXT("Audio"));
	Audio->SetupAttachment(RootComponent);
	Audio->bAutoActivate = false;

	// generate_zonalar.py ZONES + chiqargan master dB
	//               nom             x0    x1    z0     z1   master  olomon sotuvchi qadamlar shahar bozor qushlar savdo
	Zones = {
		Zona(TEXT("kirish"),       -16.f, 16.f, -8.f,   3.f,   1.6f, { -3, -6,  2,   5, -4,   0, -6 }),
		Zona(TEXT("sabzavot"),     -16.f, 16.f,  3.f,  16.5f, -1.9f, {  2,  4,  0,  -6, -2,  -8,  4 }),
		Zona(TEXT("ziravor"),      -16.f, 16.f, 16.5f, 31.5f, -0.9f, {  0,  0,  0, -10, -6, -10,  1 }),
		Zona(TEXT("ochiq_maydon"), -16.f,  8.f, 31.5f, 42.f,  -1.0f, {  1,  2, -1,  -6,  0,   5,  1 }),
		Zona(TEXT("gosht_sut"),      8.f, 16.f, 33.8f, 44.f,   0.7f, { -5, -6, -3, -14,  2, -14,  5 }),
		Zona(TEXT("orqa_darvoza"), -16.f,  8.f, 42.f,  50.f,  -0.8f, { -6, -10, -3,  0,  6,   2, -8 }),
	};
}

void ABozorFonZonalar::BeginPlay()
{
	Super::BeginPlay();
	const int32 N = UE_ARRAY_COUNT(Stems);
	bool bValid = (Sound != nullptr || Mix != nullptr) && Zones.Num() > 0;
	for (const FBozorFonZona& Z : Zones)
	{
		bValid &= Z.GainsDb.Num() == N;
	}
	if (!bValid)
	{
		UE_LOG(LogBozorFon, Warning, TEXT("[BozorFon] ovoz yoki zona qatlamlari to'liq emas - fon o'chirildi"));
		SetActorTickEnabled(false);
		return;
	}
	Gain.Init(0.f, N);
	Weight.Init(0.f, Zones.Num());
	Seed.SetNum(N);
	for (float& S : Seed) S = FMath::FRandRange(0.f, 100.f);

	// masofani o'zimiz hisoblaymiz (UpdateDistance); attenuation faqat yo'nalish uchun: maydon ichida 2D, SpatialDistance da to'liq 3D
	FSoundAttenuationSettings Att;
	Att.bAttenuate = false;
	Att.bSpatialize = true;
	Att.NonSpatializedRadiusStart = SpatialDistance;
	Att.NonSpatializedRadiusEnd = 0.f;
	Audio->bOverrideAttenuation = true;
	Audio->AttenuationOverrides = Att;
	Audio->SetLowPassFilterEnabled(true);
	Audio->SetLowPassFilterFrequency(20000.f);
	if (Mix)
	{
		// bitta mix: tasodifiy joydan boshlanadi, balandlik Tick'da SetVolumeMultiplier bilan
		Audio->SetSound(Mix);
		Audio->SetVolumeMultiplier(0.f);
		Audio->Play(FMath::FRandRange(0.f, FMath::Max(Mix->GetDuration() - 1.f, 0.f)));
		UE_LOG(LogBozorFon, Log, TEXT("[BozorFon] bitta mix: %s"), *Mix->GetName());
	}
	else
	{
		Audio->SetSound(Sound);
		// hamma qatlam bitta loop'ning bo'laklari - bir xil tasodifiy joydan boshlanadi (grafda bitta "Boshlanish" kirishi)
		Audio->SetFloatParameter(TEXT("Boshlanish"), FMath::FRandRange(0.f, LoopLength));
		for (const FName& S : Stems) Audio->SetFloatParameter(S, 0.f);
		Audio->Play();
	}
	StartTime = GetWorld()->GetTimeSeconds();
	for (TActorIterator<AAudioVolume> It(GetWorld()); It; ++It)
	{
		// hajm collision'i to'g'ri qurilganini tekshirish (chetga yaqin nuqta ham ichida bo'lishi kerak)
		const FBox B = It->GetComponentsBoundingBox(true);
		const bool bOk = It->EncompassesPoint(FMath::Lerp(B.Min, B.Max, 0.1));
		UE_LOG(LogBozorFon, Log, TEXT("[BozorFon] pavilyon %s: %s%s"), *It->GetActorNameOrLabel(), *B.ToString(),
			bOk ? TEXT("") : TEXT(" - XATO: collision bo'sh, hajm ishlamaydi (BozorLevelTools.MakeBoxBrush)"));
	}
}

void ABozorFonZonalar::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	FVector Listener = GetActorLocation(), Front, Right;
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		PC->GetAudioListenerPosition(Listener, Front, Right);
	}
	UpdateWeights(FVector2D(Listener));
	const double Now = GetWorld()->GetTimeSeconds();
	const float Fade = FadeIn > 0.f ? FMath::Clamp(float(Now - StartTime) / FadeIn, 0.f, 1.f) : 1.f;
	const float Dist = UpdateDistance(Listener);
	const float K = bPrimed ? 1.f - FMath::Exp(-DeltaSeconds / FMath::Max(Smoothing, 0.01f)) : 1.f;
	// tom ostidami: tinglovchi turgan joydagi AAudioVolume (dunyo sozlamasi emas)
	FInteriorSettings Interior;
	const bool bNowIndoor = GetWorld()->GetAudioSettings(Listener, nullptr, &Interior) != nullptr && !Interior.bIsWorldSettings;
	if (bNowIndoor != bIndoor)
	{
		bIndoor = bNowIndoor;
		UE_LOG(LogBozorFon, Log, TEXT("[BozorFon] %s (tinglovchi %.0f, %.0f, %.0f)"), bIndoor ? TEXT("tom ostiga kirdi") : TEXT("ochiq joyga chiqdi"),
			Listener.X, Listener.Y, Listener.Z);
	}
	if (FMath::FloorToInt(Now / 2.0) != FMath::FloorToInt((Now - DeltaSeconds) / 2.0))
	{
		UE_LOG(LogBozorFon, Verbose, TEXT("[BozorFon] tinglovchi (%.0f, %.0f, %.0f) tom ostida=%d"), Listener.X, Listener.Y, Listener.Z, bIndoor ? 1 : 0);
	}
	const float KIn = bPrimed ? 1.f - FMath::Exp(-DeltaSeconds / FMath::Max(IndoorTime, 0.01f)) : 1.f;
	Indoor = FMath::Lerp(Indoor, bIndoor ? 1.f : 0.f, KIn);
	bPrimed = true;
	if (Mix)
	{
		Audio->SetVolumeMultiplier(Volume * Fade * Dist * Db(Indoor * MixIndoorDb));
		return;
	}
	for (int32 i = 0; i < Gain.Num(); ++i)
	{
		float Target = 0.f;
		for (int32 z = 0; z < Zones.Num(); ++z)
		{
			if (Weight[z] > 0.f) Target += Weight[z] * Db(Zones[z].GainsDb[i] + Zones[z].MasterDb);
		}
		Gain[i] = FMath::Lerp(Gain[i], Target, K);
		// 15-40 s davrli sekin tebranish, har qatlam o'z fazasida
		const float Rate = 1.f / FMath::Lerp(15.f, 40.f, 0.5f + 0.5f * FMath::PerlinNoise1D(Seed[i] + 0.37f));
		const float Wob = WobbleDb * FMath::Clamp(FMath::PerlinNoise1D(Seed[i] * 7.f + float(Now) * Rate), -1.f, 1.f);
		const float In = IndoorDb.IsValidIndex(i) ? Indoor * IndoorDb[i] : 0.f;
		Audio->SetFloatParameter(Stems[i], Volume * Fade * Dist * Gain[i] * Db(Wob + In));
	}
}

// zona ichida 1, chegaradan Blend sm da 0 (smoothstep), so'ng normallash; hech bir zonada emas - eng yaqini
void ABozorFonZonalar::UpdateWeights(const FVector2D& P)
{
	float Sum = 0.f, Best = TNumericLimits<float>::Max();
	int32 Nearest = 0, Top = 0;
	for (int32 z = 0; z < Zones.Num(); ++z)
	{
		const float D = Zones[z].Distance(P);
		Weight[z] = 1.f - FMath::SmoothStep(0.f, 1.f, D / FMath::Max(Blend, 1.f));
		Sum += Weight[z];
		if (D < Best) { Best = D; Nearest = z; }
		if (Weight[z] > Weight[Top]) Top = z;
	}
	if (Sum < 1e-4f)
	{
		Weight[Nearest] = 1.f;
		Sum = 1.f;
		Top = Nearest;
	}
	for (float& W : Weight) W /= Sum;
	if (CurrentZone != Zones[Top].Name)
	{
		CurrentZone = Zones[Top].Name;
		UE_LOG(LogBozorFon, Log, TEXT("[BozorFon] zona: %s (o'ynayapti: %d)"), *CurrentZone, Audio->IsPlaying() ? 1 : 0);
	}
}

// maydonning eng yaqin chetigacha masofa bo'yicha balandlik ko'paytuvchisi; manba shu nuqtaga ko'chadi, low-pass yopiladi
float ABozorFonZonalar::UpdateDistance(const FVector& Listener)
{
	const FVector C = Area.GetClosestPointTo(Listener);
	const float D = FVector2D::Distance(FVector2D(Listener), FVector2D(C));
	Audio->SetWorldLocation(FVector(C.X, C.Y, Listener.Z));
	Audio->SetLowPassFilterFrequency(FMath::Max(2500.f, 22000.f * FMath::Exp(-D / FMath::Max(MuffleDistance, 1.f))));
	const float Fall = 1.f / (1.f + D / FMath::Max(HalfDistance, 1.f));
	const float Tail = 1.f - FMath::SmoothStep(0.f, 1.f, FMath::GetRangePct(FadeStart, MaxDistance, D));
	return Fall * FMath::Clamp(Tail, 0.f, 1.f);
}
