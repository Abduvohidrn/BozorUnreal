#include "BozorOvoz.h"

#include "BozorNpc.h"
#include "AudioDevice.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/CommandLine.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "Sound/SoundBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogBozorOvoz, Log, All);

TWeakObjectPtr<ABozorSotuvchiOvozi> ABozorSotuvchiOvozi::Instance;

namespace
{
	const TCHAR* Female[] = { TEXT("a1"), TEXT("a2") };
	const TCHAR* Male[] = { TEXT("e1"), TEXT("e2") };
	const FName Chaqiriq(TEXT("chaqiriq"));
}

ABozorSotuvchiOvozi::ABozorSotuvchiOvozi()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ABozorSotuvchiOvozi::BeginPlay()
{
	Super::BeginPlay();
	Instance = this;
	for (USoundBase* C : Clips)
	{
		if (!C) continue;
		// sot_<tur>_<bolim>_<persona>_<nn>
		TArray<FString> P;
		C->GetName().ParseIntoArray(P, TEXT("_"));
		if (P.Num() < 5 || P[0] != TEXT("sot")) continue;
		ByKey.FindOrAdd(P[1] + TEXT("_") + P[2] + TEXT("_") + P[3]).Add(C);
	}
	if (ByKey.Num() == 0)
	{
		UE_LOG(LogBozorOvoz, Warning, TEXT("[BozorOvoz] sotuvchi kliplari yo'q - ovozlar o'chirildi"));
		SetActorTickEnabled(false);
		return;
	}
	NextAmbient = GetWorld()->GetTimeSeconds() + FMath::FRandRange(1.f, 3.f);
	UE_LOG(LogBozorOvoz, Display, TEXT("[BozorOvoz] klip=%d to'plam=%d"), Clips.Num(), ByKey.Num());
}

void ABozorSotuvchiOvozi::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Instance.Get() == this) Instance = nullptr;
	Super::EndPlay(Reason);
}

// ABozorNpc chaqiradi; Chance - gapirish ehtimoli (har savdo gapida ham gapirsa, ko'p bo'lib ketadi)
void ABozorSotuvchiOvozi::Say(ABozorNpc* Vendor, FName Kind, float Chance)
{
	ABozorSotuvchiOvozi* O = Instance.Get();
	if (!O || !O->IsActorTickEnabled() || !Vendor) return;
	if (Kind == TEXT("savdo")) Chance *= O->SavdoChance;
	else if (Kind == TEXT("rahmat")) Chance *= O->RahmatChance;
	if (FMath::FRand() < Chance) O->TrySay(Vendor, Kind);
}

void ABozorSotuvchiOvozi::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const double Now = GetWorld()->GetTimeSeconds();
	FVector L;
	if (!ListenerLocation(L)) return;
	if (Now >= NextScan)
	{
		NextScan = Now + 5.0;
		Vendors.Reset();
		for (TActorIterator<ABozorNpc> It(GetWorld()); It; ++It)
		{
			if (It->NpcRole == EBozorRole::Sotuvchi) Vendors.Add(*It);
		}
	}
	if (Now < NextAmbient) return;
	NextAmbient = Now + FMath::FRandRange(AmbientGap.X, AmbientGap.Y);
	// o'yinchi atrofidagi bo'sh sotuvchilardan biri baqiradi
	TArray<ABozorNpc*> Near;
	for (const TWeakObjectPtr<ABozorNpc>& V : Vendors)
	{
		if (V.IsValid() && !V->Deal.IsValid() && FVector::DistSquared(V->GetActorLocation(), L) < AmbientRadius * AmbientRadius) Near.Add(V.Get());
	}
	if (Near.Num() > 0) TrySay(Near[FMath::RandRange(0, Near.Num() - 1)], FMath::FRand() < 0.85f ? Chaqiriq : FName(TEXT("salom")));
}

bool ABozorSotuvchiOvozi::ListenerLocation(FVector& Out) const
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC) return false;
	FVector Front, Right;
	PC->GetAudioListenerPosition(Out, Front, Right);
	return true;
}

bool ABozorSotuvchiOvozi::TrySay(ABozorNpc* Vendor, FName Kind)
{
	FVector L;
	if (!ListenerLocation(L) || FVector::DistSquared(Vendor->GetActorLocation(), L) > HearDistance * HearDistance) return false;
	const double Now = GetWorld()->GetTimeSeconds();
	FVoice& V = Get(Vendor);
	if (V.Src->IsPlaying() || Now < V.NextTime) return false;
	const bool bCall = Kind == Chaqiriq;
	if (bCall && ActiveCalls() >= MaxCalls) return false;
	const bool bSavdo = Kind == TEXT("savdo");
	if (bSavdo && Now < NextSavdo) return false;
	USoundBase* Clip = Pick(V, Kind, Now);
	if (!Clip) return false;           // hamma gap yaqinda aytilgan - takrorlamay jim turadi
	V.Src->SetSound(Clip);
	V.Src->SetVolumeMultiplier(Volume * FMath::FRandRange(VolumeRange.X, VolumeRange.Y));
	V.Src->SetPitchMultiplier(FMath::FRandRange(PitchRange.X, PitchRange.Y));
	V.Src->Play();
	V.Last = Clip;
	Heard.Add(SourceKey(Clip), Now);
	if (bSavdo) NextSavdo = Now + Clip->GetDuration() + SavdoGap;
	V.bCall = bCall;
	V.NextTime = Now + Clip->GetDuration() + (bCall ? CallCooldown * FMath::FRandRange(0.8f, 1.6f) : 0.3f);
	UE_LOG(LogBozorOvoz, Verbose, TEXT("[BozorOvoz] %s: %s"), *Vendor->GetActorNameOrLabel(), *Clip->GetName());
	return true;
}

int32 ABozorSotuvchiOvozi::ActiveCalls() const
{
	int32 N = 0;
	for (const TPair<TWeakObjectPtr<ABozorNpc>, FVoice>& P : Voices)
	{
		N += P.Value.bCall && P.Value.Src && P.Value.Src->IsPlaying() ? 1 : 0;
	}
	return N;
}

ABozorSotuvchiOvozi::FVoice& ABozorSotuvchiOvozi::Get(ABozorNpc* Vendor)
{
	if (FVoice* Found = Voices.Find(Vendor); Found && Found->Src) return *Found;
	FVoice V;
	V.Section = Bolim(Vendor->GetActorLocation());
	// keksalar (WalkStyle 2/3) - keksa ovoz; qolganlar navbatma-navbat: yonma-yon rastalar bir xil ovozda gapirmasin
	V.Persona = Vendor->WalkStyle == 3 ? TEXT("a3") : Vendor->WalkStyle == 2 ? TEXT("e2")
		: Vendor->bFemale ? Female[FemaleCount++ % UE_ARRAY_COUNT(Female)] : Male[MaleCount++ % UE_ARRAY_COUNT(Male)];
	// og'iz balandligida (oyoqdan ~155 sm), sotuvchi bilan birga buriladi
	UAudioComponent* A = NewObject<UAudioComponent>(Vendor, TEXT("SotuvchiOvozi"));
	A->bAutoActivate = false;
	A->bAutoDestroy = false;
	const float Half = Vendor->GetCapsuleComponent() ? Vendor->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 88.f;
	A->SetupAttachment(Vendor->GetRootComponent());
	A->SetRelativeLocation(FVector(15.f, 0.f, 155.f - Half));
	A->RegisterComponent();
	V.Src = A;
	UE_LOG(LogBozorOvoz, Log, TEXT("[BozorOvoz] %s: %s, %s"), *Vendor->GetActorNameOrLabel(), *V.Persona, *V.Section);
	return Voices.Add(Vendor, MoveTemp(V));
}

// sot_<tur>_<bolim>_<persona>_<nn> -> <tur>_<bolim>_<a|e>_<nn>: bitta haqiqiy yozuv ayol (a1..a3) yoki erkak (e1..e2)
// personalarining hammasiga bir xil raqam bilan nusxalangan (bozor_yozuv_joylash.py) - ular bitta manba
FString ABozorSotuvchiOvozi::SourceKey(const USoundBase* Clip)
{
	TArray<FString> P;
	Clip->GetName().ParseIntoArray(P, TEXT("_"));
	return P.Num() >= 5 ? P[1] + TEXT("_") + P[2] + TEXT("_") + P[3].Left(1) + TEXT("_") + P[4] : Clip->GetName();
}

USoundBase* ABozorSotuvchiOvozi::Pick(const FVoice& V, FName Kind, double Now) const
{
	const FString K = Kind.ToString();
	// yaqinda (RepeatWindow) bozorda aytilmagan gaplar
	auto Fresh = [&](const TArray<USoundBase*>* List)
	{
		TArray<USoundBase*> Out;
		if (!List) return Out;
		for (USoundBase* C : *List)
		{
			const double* T = Heard.Find(SourceKey(C));
			if (!T || Now - *T >= RepeatWindow) Out.Add(C);
		}
		return Out;
	};
	TArray<USoundBase*> Cand;
	// chaqiriq: ko'pincha o'z mahsulotini aytadi, ba'zan umumiy "keling, keling"
	if (Kind == Chaqiriq && V.Section != TEXT("umumiy") && FMath::FRand() < 0.65f)
	{
		Cand = Fresh(ByKey.Find(K + TEXT("_") + V.Section + TEXT("_") + V.Persona));
	}
	if (Cand.Num() == 0) Cand = Fresh(ByKey.Find(K + TEXT("_umumiy_") + V.Persona));
	return Cand.Num() > 0 ? Cand[FMath::RandRange(0, Cand.Num() - 1)] : nullptr;
}

// Sotuvchi turgan joydan bo'lim (Unity BozorBuilder.BuildScene tartibi, Unity metrlarida: x = Y/100, z = X/100):
// yopiq segmentlar z 3..15 va 18..30, ichki qator markaziy yo'lak tomonda (|x| < 6), tashqi qator yon yo'lak tomonda, 4 m modullar;
// z > 31.5 ochiq maydon; x > 8, z > 33.8 go'sht-sut pavilyoni; |x| > 10.5 kiosklar
FString ABozorSotuvchiOvozi::Bolim(const FVector& Location)
{
	const float X = Location.Y / 100.f, Ax = FMath::Abs(X), Z = Location.X / 100.f;
	if (X > 8.f && Z > 33.8f) return Z > 42.5f && X < 13.f ? TEXT("sut") : TEXT("gosht");
	if (Z >= 31.5f)
	{
		if (Ax < 5.5f) return X < 0 ? TEXT("guruch") : TEXT("meva");   // yo'lak chetidagi qoplar | meva yashiklari
		if (X > 0) return TEXT("meva");
		return X < -11.f ? TEXT("sabzavot") : TEXT("meva");            // fermerlar soyabonlari
	}
	if (Ax > 10.5f) return TEXT("sabzavot");
	const bool bFront = Z < 16.5f;
	const int32 M = FMath::Clamp(FMath::FloorToInt((Z - (bFront ? 3.f : 18.f)) / 4.f), 0, 2);
	const bool bInner = Ax < 6.f;
	if (bFront) return bInner ? TEXT("sabzavot") : M == 0 ? TEXT("non") : M == 1 ? TEXT("kokat") : TEXT("meva");
	return bInner ? (M < 2 ? TEXT("ziravor") : TEXT("quruq")) : M == 0 ? TEXT("guruch") : M == 1 ? TEXT("atlas") : TEXT("meva");
}

// ============================================================ rasta ovozlari

TWeakObjectPtr<ABozorRastaOvozlari> ABozorRastaOvozlari::Instance;

ABozorRastaOvozlari::ABozorRastaOvozlari()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ABozorRastaOvozlari::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer)
	{
		SetActorTickEnabled(false);
		return;
	}
	Instance = this;
	for (USoundBase* C : Clips)
	{
		// sfx_<tur>_<nn>: tur o'zi ham "_" li bo'lishi mumkin (tarozi_pip)
		FString Name = C ? C->GetName() : FString();
		int32 Last;
		if (!Name.StartsWith(TEXT("sfx_")) || !Name.FindLastChar(TEXT('_'), Last) || Last <= 4) continue;
		ByKind.FindOrAdd(FName(*Name.Mid(4, Last - 4))).Add(C);
	}
	if (ByKind.Num() == 0)
	{
		UE_LOG(LogBozorOvoz, Warning, TEXT("[BozorOvoz] rasta SFX kliplari yo'q - o'chirildi"));
		SetActorTickEnabled(false);
		return;
	}
	// hamma nuqta birdaniga chalinmasin
	const double Now = GetWorld()->GetTimeSeconds();
	for (FBozorRastaNuqta& P : Points) P.NextTime = Now + FMath::FRandRange(1.f, P.Gap.Y * GapScale);
	UE_LOG(LogBozorOvoz, Display, TEXT("[BozorOvoz] rasta SFX: klip=%d tur=%d nuqta=%d"), Clips.Num(), ByKind.Num(), Points.Num());
}

void ABozorRastaOvozlari::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Instance.Get() == this) Instance = nullptr;
	Super::EndPlay(Reason);
}

void ABozorRastaOvozlari::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now < NextScan) return;
	NextScan = Now + 0.2;
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC) return;
	FVector L, Front, Right;
	PC->GetAudioListenerPosition(L, Front, Right);
	for (FBozorRastaNuqta& P : Points)
	{
		if (Now < P.NextTime || P.Kinds.Num() == 0) continue;
		P.NextTime = Now + FMath::FRandRange(P.Gap.X, P.Gap.Y) * GapScale;
		if (FVector::DistSquared(P.Location, L) > ActiveRadius * ActiveRadius) continue;
		PlayKind(P.Kinds[FMath::RandRange(0, P.Kinds.Num() - 1)], P.Location, nullptr);
	}
}

void ABozorRastaOvozlari::Play(const UObject* WorldContext, FName Kind, const FVector& Location, AActor* Source)
{
	if (ABozorRastaOvozlari* R = Instance.Get(); R && R->IsActorTickEnabled()) R->PlayKind(Kind, Location, Source);
}

void ABozorRastaOvozlari::PlayNear(const UObject* WorldContext, const TArray<FName>& Kinds, const FVector& Location, float Radius, FName Fallback, AActor* Source)
{
	ABozorRastaOvozlari* R = Instance.Get();
	if (!R || !R->IsActorTickEnabled()) return;
	const FBozorRastaNuqta* Best = nullptr;
	FName BestKind;
	float BestD = Radius * Radius;
	for (const FBozorRastaNuqta& P : R->Points)
	{
		const float D = FVector::DistSquared(P.Location, Location);
		if (D >= BestD) continue;
		for (const FName& K : Kinds)
		{
			if (P.Kinds.Contains(K)) { Best = &P; BestKind = K; BestD = D; break; }
		}
	}
	if (Best) R->PlayKind(BestKind, Best->Location, Source);
	else R->PlayKind(Fallback, Location, Source);
}

void ABozorRastaOvozlari::PlayKind(FName Kind, const FVector& Location, AActor* Source)
{
	const TArray<USoundBase*>* List = ByKind.Find(Kind);
	if (!List || List->Num() == 0) return;
	// ketma-ket bir xil variant chiqmasin; balandlik va pitch har safar biroz boshqa
	int32 I = FMath::RandRange(0, List->Num() - 1);
	USoundBase*& Last = LastClip.FindOrAdd(Kind);
	if ((*List)[I] == Last && List->Num() > 1) I = (I + 1) % List->Num();
	Last = (*List)[I];
	UGameplayStatics::PlaySoundAtLocation(this, Last, Location, FRotator::ZeroRotator,
		Volume * FMath::FRandRange(0.75f, 1.f), FMath::FRandRange(0.94f, 1.06f), 0.f, nullptr, nullptr, Source);
	UE_LOG(LogBozorOvoz, Verbose, TEXT("[BozorOvoz] rasta: %s (%s)"), *Last->GetName(), Source ? *Source->GetActorNameOrLabel() : TEXT("nuqta"));
}

// ============================================================ o'yinchi qadamlari

ABozorQadamOvozlari::ABozorQadamOvozlari()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;   // suyaklar shu kadrda yangilangandan keyin o'qiladi
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ABozorQadamOvozlari::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer)
	{
		SetActorTickEnabled(false);
		return;
	}
	for (USoundBase* C : Clips)
	{
		// qadam_<sirt>_<nn>
		FString Name = C ? C->GetName() : FString();
		int32 Last;
		if (!Name.StartsWith(TEXT("qadam_")) || !Name.FindLastChar(TEXT('_'), Last) || Last <= 6) continue;
		ByKind.FindOrAdd(FName(*Name.Mid(6, Last - 6))).Add(C);
	}
	if (ByKind.Num() == 0)
	{
		UE_LOG(LogBozorOvoz, Warning, TEXT("[BozorOvoz] qadam kliplari yo'q - o'chirildi"));
		SetActorTickEnabled(false);
		return;
	}
	Feet.SetNum(FootBones.Num());
	bTest = FParse::Param(FCommandLine::Get(), TEXT("BozorQadamTest"));
	TestStart = GetWorld()->GetTimeSeconds();
	UE_LOG(LogBozorOvoz, Display, TEXT("[BozorOvoz] qadamlar: klip=%d sirt=%d"), Clips.Num(), ByKind.Num());
}

void ABozorQadamOvozlari::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ACharacter* C = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	UCharacterMovementComponent* Move = C ? C->GetCharacterMovement() : nullptr;
	if (!Move) return;
	if (bTest) UpdateTest(C, GetWorld()->GetTimeSeconds());

	const float Half = C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Base = C->GetActorLocation() - FVector(0.f, 0.f, Half);
	const bool bGround = Move->IsMovingOnGround();
	const float Speed = C->GetVelocity().Size2D();
	USkeletalMeshComponent* Mesh = C->GetMesh();
	// sakrab tushdi: ikki oyoq birga, balandroq
	if (bGround && bWasFalling)
	{
		Step(C, Base, RunSpeed, 1.2f);
		for (FFoot& F : Feet) F.Peak = F.Prev;   // tushgan oyoqlar yana ko'tarilgunicha qadam yo'q
	}
	bWasFalling = Move->IsFalling();
	if (!bGround || Speed < MinSpeed) return;

	// to'liq yurish tezligiga nisbatan ulush. BS_Idle_Walk_Run'da yurish namunasi 300 sm/s da, o'yinchi esa ko'pi bilan 220:
	// sekin yurganda blendspace'da idle ulushi katta - oyoq atigi ~3 sm ko'tariladi (0.35 kirishda qat'iy MinLift=5 bilan qadam umuman yo'q edi)
	const float Frac = FMath::Clamp(Speed / FMath::Max(Move->MaxWalkSpeed, 1.f), 0.3f, 1.f);
	Stride += Speed * DeltaSeconds;   // oxirgi qadamdan beri yurilgan masofa, Step() nolga qaytaradi

	bool bBones = Mesh != nullptr;
	for (const FName& B : FootBones) bBones &= Mesh && Mesh->GetBoneIndex(B) != INDEX_NONE;
	if (!bBones)
	{
		if (Stride >= StrideLength) Step(C, Base, Speed, 1.f);
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	const float Lift = MinLift * Frac;
	// tushish tezligi chegarasi kadr chastotasiga bog'liq emas (avval kadrga 0.2 sm edi - 40 fps da 8 sm/s, 120 fps da sezilmay qolardi)
	const float Down = 8.f * Frac * DeltaSeconds;
	// masofa bo'yicha zaxira qadam oralig'i: tezlikka qarab qisqaradi (220 sm/s da ~112 sm - suyak qadamlari ~82 sm da chiqadi,
	// zaxira ishlamaydi; 77 sm/s da ~49 sm, ~0.63 s). Suyak qadami esa uning yarmidan oldin chiqmaydi - ikkalasi aralashsa ritm buzilmasin
	const float Gap = 1.5f * StrideLength * FMath::Pow(Frac, 0.8f);
	for (int32 i = 0; i < FootBones.Num(); ++i)
	{
		FFoot& F = Feet[i];
		const FVector Foot = Mesh->GetBoneLocation(FootBones[i]);
		const float Z = Foot.Z - Base.Z;
		FVector2D& R = Speed < Move->MaxWalkSpeed * 0.6f ? F.Walk : F.Run;   // test logi: sekin / to'liq tezlik
		R.X = FMath::Min(R.X, Z);
		R.Y = FMath::Max(R.Y, Z);
		F.Peak = FMath::Max(F.Peak, Z);
		if (Z < F.Prev - Down)
		{
			F.bDown = true;
		}
		// pastga tushish tugadi (oyoq to'xtadi yoki ko'tarila boshladi): eng past nuqta - F.Prev
		else if (F.bDown && Z > F.Prev - Down * 0.25f)
		{
			F.bDown = false;
			if (F.Prev < PlantHeight && F.Peak - F.Prev >= Lift && Now - F.LastStep >= MinInterval && Stride >= 0.5f * Gap)
			{
				F.Peak = F.Prev;
				F.LastStep = Now;
				Step(C, FVector(Foot.X, Foot.Y, Base.Z), Speed, 1.f);
			}
		}
		F.Prev = Z;
	}
	// suyakdan qadam chiqmay qoldi (juda sekin, oyoq deyarli ko'tarilmaydi) - jim yurmasin: masofa bo'yicha,
	// oxirgi qadam bosgan oyoqning teskarisidan (chap-o'ng navbati saqlansin)
	if (Stride >= Gap)
	{
		int32 Next = 0;
		for (int32 i = 1; i < Feet.Num(); ++i) if (Feet[i].LastStep < Feet[Next].LastStep) Next = i;
		FFoot& F = Feet[Next];
		const FVector Foot = Mesh->GetBoneLocation(FootBones[Next]);
		F.Peak = F.Prev;
		F.LastStep = Now;
		Step(C, FVector(Foot.X, Foot.Y, Base.Z), Speed, 1.f);
	}
}

FName ABozorQadamOvozlari::SurfaceAt(ACharacter* C, const FVector& Foot, FVector& OutPoint) const
{
	FCollisionQueryParams Q(SCENE_QUERY_STAT(BozorQadam), /*bTraceComplex=*/true, C);
	Q.bReturnPhysicalMaterial = true;
	FHitResult Hit;
	OutPoint = Foot;
	if (!GetWorld()->LineTraceSingleByChannel(Hit, Foot + FVector(0.f, 0.f, 40.f), Foot - FVector(0.f, 0.f, 60.f), ECC_Visibility, Q)) return FallbackSurface;
	OutPoint = Hit.ImpactPoint;
	const UPhysicalMaterial* PM = Hit.PhysMaterial.Get();
	if (!PM || PM->SurfaceType == SurfaceType_Default) return FallbackSurface;
	for (const FPhysicalSurfaceName& S : UPhysicsSettings::Get()->PhysicalSurfaces)
	{
		if (S.Type == PM->SurfaceType) return FName(*S.Name.ToString().ToLower());
	}
	return FallbackSurface;
}

void ABozorQadamOvozlari::Step(ACharacter* C, const FVector& Foot, float Speed, float Gain)
{
	Stride = 0.f;
	FVector Point;
	FName Surface = SurfaceAt(C, Foot, Point);
	const TArray<USoundBase*>* List = ByKind.Find(Surface);
	if (!List || List->Num() == 0)
	{
		Surface = FallbackSurface;
		List = ByKind.Find(Surface);
	}
	if (!List || List->Num() == 0) return;
	// oxirgi ikki variant qaytarilmaydi (A-B-A-B naqshi ham sezilmasin), qolganlardan teng ehtimol bilan.
	// Oldingisi shu xaritada sirt nomining 1-raqamli nusxasi kaliti bilan saqlanadi (beton -> beton_0)
	const FName PrevKey(Surface, 1);
	LastClip.FindOrAdd(Surface);
	LastClip.FindOrAdd(PrevKey);
	USoundBase*& Last = LastClip[Surface];
	USoundBase*& Prev = LastClip[PrevKey];
	TArray<USoundBase*, TInlineAllocator<8>> Pool;
	for (USoundBase* S : *List) if (S != Last && S != Prev) Pool.Add(S);
	if (Pool.IsEmpty()) for (USoundBase* S : *List) if (S != Last) Pool.Add(S);
	if (Pool.IsEmpty()) Pool.Add((*List)[0]);
	USoundBase* Clip = Pool[FMath::RandRange(0, Pool.Num() - 1)];
	Prev = Last;
	Last = Clip;

	// sekin yurganda ~60%, o'yinchining to'liq yurish tezligida 100% (MaxWalkSpeed 220 - RunSpeed'ga hech yetmaydi)
	const UCharacterMovementComponent* Move = C->GetCharacterMovement();
	const float Full = FMath::Max(Move ? Move->MaxWalkSpeed : RunSpeed, 1.f);
	const float Loud = FMath::Lerp(0.6f, 1.f, FMath::Clamp(Speed / Full, 0.f, 1.f));
	const float Vol = Volume * Loud * Gain * FMath::FRandRange(0.9f, 1.1f);   // har qadam ±10%
	const float Pitch = FMath::FRandRange(0.92f, 1.08f);

	// PlaySoundAtLocation o'rniga (u ichida shuni qiladi): filtrlar Play'dan oldin qo'yilsin - birinchi zarb ham filtrlangan bo'lsin
	FAudioDevice::FCreateComponentParams Params(GetWorld(), C);
	Params.SetLocation(Point);
	if (UAudioComponent* A = FAudioDevice::CreateComponent(Clip, Params))
	{
		A->SetWorldLocation(Point);
		A->SetVolumeMultiplier(Vol);
		A->SetPitchMultiplier(Pitch);
		A->bAllowSpatialization = Params.ShouldUseAttenuation();
		A->bAutoDestroy = true;
		A->bStopWhenOwnerDestroyed = false;
		// bola yengil, krossovkada: kliplardagi og'ir 80-95 Hz "dup" kesiladi, yuqori shitir biroz yumshaydi;
		// chastotalar har qadamda ozgina boshqacha - bir xil sintez rangi takrorlanmasin
		A->SetHighPassFilterFrequency(FMath::FRandRange(150.f, 210.f));
		A->SetHighPassFilterEnabled(true);
		A->SetLowPassFilterFrequency(FMath::FRandRange(6500.f, 11000.f));
		A->SetLowPassFilterEnabled(true);
		A->Play();
	}
	if (bTest) TestSteps.FindOrAdd(Surface)++;
	UE_LOG(LogBozorOvoz, Verbose, TEXT("[BozorOvoz] qadam: %s klip=%s tezlik=%.0f vol=%.2f pitch=%.2f joy=(%.0f, %.0f, %.0f)"),
		*Surface.ToString(), *Clip->GetName(), Speed, Vol, Pitch, Point.X, Point.Y, Point.Z);
}

// Sinov: o'yinchi kirishdan bozor ichiga qarab yuradi (0-12 s sekin, 12-25 s yugurib), keyin natija loglanadi va o'yin yopiladi
void ABozorQadamOvozlari::UpdateTest(ACharacter* C, double Now)
{
	const double T = Now - TestStart;
	if (T < 3.0) return;                       // xarita yuklanib, qahramon yerga tushsin
	if (T < 28.0)
	{
		C->AddMovementInput(FVector::ForwardVector, T < 15.0 ? 0.35f : 1.f);
		return;
	}
	FString S;
	for (const TPair<FName, int32>& P : TestSteps) S += FString::Printf(TEXT(" %s=%d"), *P.Key.ToString(), P.Value);
	FString R;
	for (int32 i = 0; i < Feet.Num(); ++i)
	{
		R += FString::Printf(TEXT(" %s sekin=[%.1f..%.1f] tez=[%.1f..%.1f]"), *FootBones[i].ToString(),
			Feet[i].Walk.X, Feet[i].Walk.Y, Feet[i].Run.X, Feet[i].Run.Y);
	}
	UE_LOG(LogBozorOvoz, Display, TEXT("[BozorOvoz] qadam testi:%s | suyak balandligi:%s | yakun joy=(%.0f, %.0f)"), *S, *R, C->GetActorLocation().X, C->GetActorLocation().Y);
	bTest = false;
	UKismetSystemLibrary::QuitGame(this, nullptr, EQuitPreference::Quit, false);
}
