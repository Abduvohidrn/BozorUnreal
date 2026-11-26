#include "BozorQatnov.h"

#include "BozorAvtobus.h"
#include "WheeledVehiclePawn.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "Components/CapsuleComponent.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/CollisionProfile.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Camera/CameraActor.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

DEFINE_LOG_CATEGORY_STATIC(LogBozorQatnov, Log, All);

ABozorQatnov::ABozorQatnov()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent->SetMobility(EComponentMobility::Static);
}

void ABozorQatnov::BeginPlay()
{
	Super::BeginPlay();
	const TCHAR* Cmd = FCommandLine::Get();
	bTest = FParse::Param(Cmd, TEXT("QatnovTest"));
	bKadr = FParse::Param(Cmd, TEXT("QatnovKadr"));
	if (FParse::Param(Cmd, TEXT("QatnovYoq")) || Turlar.Num() == 0 || Yolaklar.Num() == 0)
	{
		UE_LOG(LogBozorQatnov, Display, TEXT("[Qatnov] o'chiq (tur %d, bo'lak %d)"), Turlar.Num(), Yolaklar.Num());
		SetActorTickEnabled(false);
		return;
	}

	for (const FBozorYolak& Y : Yolaklar)
	{
		USplineComponent* S = NewObject<USplineComponent>(this);
		S->SetMobility(EComponentMobility::Static);
		S->SetupAttachment(RootComponent);
		S->RegisterComponent();
		S->ClearSplinePoints(false);
		for (const FVector& P : Y.Nuqtalar) S->AddSplinePoint(P, ESplineCoordinateSpace::World, false);
		S->UpdateSpline();
		Yollar.Add(S);
		PaydoTaymer.Add(FMath::FRandRange(PaydoOraligi.X, PaydoOraligi.Y) * 0.5f);
	}
	for (TActorIterator<ABozorAvtobusIntro> It(GetWorld()); It; ++It)
		if (It->Avtobus) Avtobus = It->Avtobus;

	// boshlang'ich holat: har bo'lakda bir nechta mashina (o'tish joyi va o'yinchidan uzoqroqda)
	for (int32 Y = 0; Y < Yollar.Num(); ++Y)
	{
		const float L = Yollar[Y]->GetSplineLength();
		const int32 Soni = FMath::Max(1, Yolaklar[Y].MaxMashina / 2);
		for (int32 I = 0; I < Soni; ++I)
			Qosh(Y, FMath::FRandRange(0.15f, 0.85f) * L, true);
	}
	UE_LOG(LogBozorQatnov, Display, TEXT("[Qatnov] boshlandi: %d bo'lak, %d tur, %d mashina, avtobus %s"),
		Yollar.Num(), Turlar.Num(), Mashinalar.Num(), *GetNameSafe(Avtobus.Get()));
}

bool ABozorQatnov::Qosh(int32 Yolak, float S, bool bBoshlangich)
{
	USplineComponent* Yol = Yollar[Yolak];
	const FVector P = Yol->GetLocationAtDistanceAlongSpline(S, ESplineCoordinateSpace::World);
	for (const FMashina& M : Mashinalar)   // boshqa mashina bilan ustma-ust tushmasin
		if (M.Yolak == Yolak && FMath::Abs(M.S - S) < M.Uzunlik + 900.f) return false;
	if (bBoshlangich)
	{
		for (const FBozorOtishJoyi& O : OtishJoylari)   // o'tish joyi ustida boshlanmasin
			if (FVector::Dist2D(P, O.Markaz) < 1200.f) return false;
		if (Korinadi(P) && Piyodalar.Num() > 0) return false;
		if (Yolaklar[Yolak].bAvtobusYoli && Avtobus.IsValid() && !Avtobus->IsHidden()) return false;
	}

	float Jami = 0.f;
	for (const FBozorMashinaTuri& T : Turlar) Jami += FMath::Max(0.f, T.Ulush);
	float R = FMath::FRand() * Jami;
	int32 Tur = 0;
	for (; Tur < Turlar.Num() - 1; ++Tur)
	{
		R -= FMath::Max(0.f, Turlar[Tur].Ulush);
		if (R <= 0.f) break;
	}
	// -QatnovTur=BMW: sinov uchun faqat shu tur (mesh nomida bor bo'lsa)
	FString Majburiy;
	if (FParse::Value(FCommandLine::Get(), TEXT("QatnovTur="), Majburiy))
		for (int32 I = 0; I < Turlar.Num(); ++I)
			if (Turlar[I].Kuzov && Turlar[I].Kuzov->GetName().Contains(Majburiy)) { Tur = I; break; }
	// BMW (chiroyli raqamli) - qatnovda bir vaqtda faqat bittasi; bor bo'lsa birinchi tur (Cobalt)
	auto BmwMi = [&](int32 I) { return Turlar[I].Kuzov && Turlar[I].Kuzov->GetName().StartsWith(TEXT("SM_BMW")); };
	if (BmwMi(Tur))
		for (const FMashina& Boshqa : Mashinalar)
			if (BmwMi(Boshqa.Tur)) { Tur = 0; break; }
	const FBozorMashinaTuri& T = Turlar[Tur];
	if (!T.Kuzov) return false;

	FMashina M;
	M.Yolak = Yolak;
	M.Tur = Tur;
	M.S = S;
	M.V0 = FMath::FRandRange(TezlikKmSoat.X, TezlikKmSoat.Y) * 100000.f / 3600.f;
	M.V = bBoshlangich ? M.V0 * 0.8f : M.V0 * 0.6f;
	const FBox B = T.Kuzov->GetBoundingBox();
	const bool bOqX = FMath::Abs(FMath::Cos(FMath::DegreesToRadians(T.OldYaw))) > 0.5f;
	M.Uzunlik = bOqX ? B.GetSize().X : B.GetSize().Y;
	M.Kenglik = bOqX ? B.GetSize().Y : B.GetSize().X;

	auto Komp = [&](UStaticMesh* Mesh, bool bKolliziya, bool bSoya = false,
		USceneComponent* Ota = nullptr) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
		C->SetMobility(EComponentMobility::Movable);
		C->SetupAttachment(Ota ? Ota : GetRootComponent());
		C->SetAbsolute(!Ota, !Ota, false);
		C->SetStaticMesh(Mesh);
		C->SetCanEverAffectNavigation(false);
		if (bKolliziya)
		{
			C->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
			C->CanCharacterStepUpOn = ECB_No;
		}
		else
		{
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			C->SetCastShadow(bSoya);
		}
		C->RegisterComponent();
		Komponentlar.Add(C);
		return C;
	};
	M.Kuzov = Komp(T.Kuzov, true);
	M.Oyna = T.Oyna ? Komp(T.Oyna, false) : nullptr;
	FString Model = T.Kuzov->GetName();
	if (Model.RemoveFromStart(TEXT("SM_")))
	{
		Model.RemoveFromEnd(TEXT("_Qatnov"));
		// Ushbu meshlar xaritadagi turgan nusxalarda ham ishlatiladi.
		const FString MeshNomi = TEXT("SM_Raqam_") + Model;
		const FString RaqamYoli = FString::Printf(TEXT("/Game/Bozor/Mashinalar/Raqamlar/%s.%s"), *MeshNomi, *MeshNomi);
		if (UStaticMesh* RaqamMesh = LoadObject<UStaticMesh>(nullptr, *RaqamYoli, nullptr, LOAD_NoWarn))
		{
			M.Raqam = Komp(RaqamMesh, false, false, M.Kuzov);
			// BMW plitasida o'z materiali (M_Raqam_BMW, "01 S 100 SS") - atlas indeksi kerak emas
			if (UMaterialInstanceDynamic* Material = Model == TEXT("BMW") ? nullptr : M.Raqam->CreateDynamicMaterialInstance(0))
			{
				// Atlasdagi avvalgi to'qqizta kombinatsiya turgan mashinalarniki.
				constexpr int32 QatnovBoshlanishi = 9;
				constexpr int32 QatnovSoni = 23;
				TSet<int32> Band;
				for (const FMashina& Boshqa : Mashinalar)
					Band.Add(Boshqa.RaqamIndeksi);
				const int32 Boshi = FMath::RandHelper(QatnovSoni);
				M.RaqamIndeksi = QatnovBoshlanishi + Boshi;
				for (int32 I = 0; I < QatnovSoni; ++I)
				{
					const int32 Nomzod = QatnovBoshlanishi + (Boshi + I) % QatnovSoni;
					if (!Band.Contains(Nomzod))
					{
						M.RaqamIndeksi = Nomzod;
						break;
					}
				}
				Material->SetScalarParameterValue(TEXT("RaqamIndeksi"), M.RaqamIndeksi);
			}
		}
	}
	for (int32 G = 0; G < T.Gildiraklar.Num() && G < T.GildirakJoylari.Num(); ++G)
		M.Gildiraklar.Add(T.Gildiraklar[G] ? Komp(T.Gildiraklar[G], false, true) : nullptr);
	M.Burchak = FMath::FRand() * 360.f;
	if (DvigatelOvozi)
	{
		UAudioComponent* A = NewObject<UAudioComponent>(this);
		A->bAutoActivate = false;
		A->SetSound(DvigatelOvozi);
		A->AttenuationSettings = OvozSonishi;
		A->SetupAttachment(M.Kuzov);
		A->SetRelativeLocation(FVector(0.f, 0.f, 60.f));
		A->RegisterComponent();
		A->SetVolumeMultiplier(DvigatelBalandligi.X);
		A->SetPitchMultiplier(DvigatelPitch.X);
		A->Play(FMath::FRand() * 5.f);   // har mashina namunaning boshqa joyidan - bir xil "xor" bo'lmasin
		Komponentlar.Add(A);
		M.Dvigatel = A;
	}
	Joyla(M);
	Mashinalar.Add(M);
	return true;
}

void ABozorQatnov::Joyla(FMashina& M) const
{
	const USplineComponent* Yol = Yollar[M.Yolak];
	const FBozorMashinaTuri& T = Turlar[M.Tur];
	const FVector P = Yol->GetLocationAtDistanceAlongSpline(M.S, ESplineCoordinateSpace::World);
	const FVector D = Yol->GetDirectionAtDistanceAlongSpline(M.S, ESplineCoordinateSpace::World);
	const FQuat Yonalish = FRotator(0.f, D.Rotation().Yaw, 0.f).Quaternion();
	const FQuat MeshBurchak = FRotator(0.f, -T.OldYaw, 0.f).Quaternion();
	// kuzov: harakat yo'nalishi bo'yicha pitch (+ old tomoni ko'tariladi) va roll, ~55 sm balandlikdagi nuqta atrofida
	const FQuat Ogish = Yonalish * FRotator(M.Pitch, 0.f, M.Roll).Quaternion();
	const FVector Markaz(0.f, 0.f, 55.f);
	const FVector KuzovJoy = P + Markaz - Ogish.RotateVector(Markaz);
	const FQuat KuzovBurchak = Ogish * MeshBurchak;
	M.Kuzov->SetWorldLocationAndRotation(KuzovJoy, KuzovBurchak);
	if (M.Oyna) M.Oyna->SetWorldLocationAndRotation(KuzovJoy, KuzovBurchak);
	if (M.Gildiraklar.Num() == 0) return;
	// g'ildiraklar kuzov og'ishidan mustaqil (yerda): o'z o'qi atrofida dumalaydi, old g'ildiraklar rul burchagiga buriladi
	const FQuat Old = FRotator(0.f, T.OldYaw, 0.f).Quaternion();
	const FQuat Aylanish = Old * FRotator(-M.Burchak, 0.f, 0.f).Quaternion() * Old.Inverse();
	const FQuat Rul(FVector::UpVector, FMath::DegreesToRadians(M.Rul));
	const FVector OldOq = Old.GetForwardVector();
	const FTransform K(Yonalish * MeshBurchak, P);
	for (int32 G = 0; G < M.Gildiraklar.Num(); ++G)
		if (UStaticMeshComponent* Gildirak = M.Gildiraklar[G])
		{
			const bool bOldi = FVector::DotProduct(T.GildirakJoylari[G], OldOq) > 0.f;
			Gildirak->SetWorldLocationAndRotation(K.TransformPosition(T.GildirakJoylari[G]),
				K.GetRotation() * (bOldi ? Rul * Aylanish : Aylanish));
		}
}

// old g'ildiraklar rul burchagi (yo'l egriligidan: tan(rul) = o'qlar orasi * egrilik) va kuzovning prujinali og'ishi
void ABozorQatnov::Fizika(FMashina& M, float Dt)
{
	const USplineComponent* Yol = Yollar[M.Yolak];
	const FBozorMashinaTuri& T = Turlar[M.Tur];
	const float L = Yol->GetSplineLength();
	auto Egrilik = [&](float S)   // ishorali: + o'ngga burilish (UE'da yaw ortadi)
	{
		const float A = FMath::Clamp(S, 0.f, FMath::Max(0.f, L - 210.f));
		const FVector D0 = Yol->GetDirectionAtDistanceAlongSpline(A, ESplineCoordinateSpace::World).GetSafeNormal2D();
		const FVector D1 = Yol->GetDirectionAtDistanceAlongSpline(A + 200.f, ESplineCoordinateSpace::World).GetSafeNormal2D();
		return FMath::Atan2(FVector::CrossProduct(D0, D1).Z, FVector::DotProduct(D0, D1)) / 200.f;
	};
	// o'qlar orasi - g'ildirak joylaridan (bo'lmasa uzunlikning 60%)
	float OldO = -1e9f, OrqaO = 1e9f;
	const FVector OldOq = FRotator(0.f, T.OldYaw, 0.f).Vector();
	for (const FVector& G : T.GildirakJoylari)
	{
		OldO = FMath::Max(OldO, FVector::DotProduct(G, OldOq));
		OrqaO = FMath::Min(OrqaO, FVector::DotProduct(G, OldOq));
	}
	const float Baza = T.GildirakJoylari.Num() >= 4 ? OldO - OrqaO : M.Uzunlik * 0.6f;
	const float Kappa = Egrilik(M.S + Baza * 0.5f);
	const float RulMaqsad = FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan(Baza * Kappa)), -MaxRul, MaxRul);
	M.Rul = FMath::FInterpTo(M.Rul, RulMaqsad, Dt, 6.f);
	M.YonTezlanish = M.V * M.V * Egrilik(M.S);
	// osma: ikkinchi tartibli prujina-amortizator (bir oz chayqalib to'xtaydi)
	const float W = 2.f * PI * OsmaChastota;
	auto Prujina = [&](float& X, float& XV, float Maqsad)
	{
		XV += (W * W * (Maqsad - X) - 2.f * OsmaSonish * W * XV) * Dt;
		X += XV * Dt;
	};
	Prujina(M.Pitch, M.PitchV, FMath::Clamp(M.Tezlanish * PitchKoef, -3.f, 2.f));
	Prujina(M.Roll, M.RollV, FMath::Clamp(-M.YonTezlanish * RollKoef, -3.f, 3.f));
}

// dvigatel ovozi tezlikka qarab; yo'lda piyoda to'sib tursa - qisqa signal (zebradagi piyodaga signal chalinmaydi)
void ABozorQatnov::Ovoz(FMashina& M, float Dt)
{
	const float Nisbat = FMath::Clamp(M.V / FMath::Max(M.V0, 1.f), 0.f, 1.f);
	if (M.Dvigatel)
	{
		M.Dvigatel->SetPitchMultiplier(FMath::Lerp(DvigatelPitch.X, DvigatelPitch.Y, Nisbat));
		M.Dvigatel->SetVolumeMultiplier(FMath::Lerp(DvigatelBalandligi.X, DvigatelBalandligi.Y, Nisbat));
	}
	const float Hozir = GetWorld()->GetTimeSeconds();
	if (M.Signal && M.SignalTugash > 0.f && Hozir > M.SignalTugash)
	{
		M.Signal->Stop();
		M.SignalTugash = -1.f;
	}
	if (!SignalOvozi || M.Signallar >= 2 || M.YoldaKutish < SignalKutish + M.Signallar * 3.f) return;
	if (!M.Signal)
	{
		M.Signal = NewObject<UAudioComponent>(this);
		M.Signal->bAutoActivate = false;
		M.Signal->SetSound(SignalOvozi);
		M.Signal->AttenuationSettings = OvozSonishi;
		M.Signal->SetupAttachment(M.Kuzov);
		M.Signal->RegisterComponent();
		Komponentlar.Add(M.Signal);
	}
	M.Signal->Play();
	M.SignalTugash = Hozir + (M.Signallar == 0 ? 0.25f : 0.45f);
	++M.Signallar;
	UE_LOG(LogBozorQatnov, Display, TEXT("[Qatnov] %s: yo'ldagi piyodaga signal"), *GetNameSafe(Turlar[M.Tur].Kuzov.Get()));
}

void ABozorQatnov::Olib(FMashina& M)
{
	TArray<USceneComponent*> Hammasi = { M.Raqam, M.Kuzov, M.Oyna, M.Dvigatel, M.Signal };
	for (UStaticMeshComponent* G : M.Gildiraklar) Hammasi.Add(G);
	for (USceneComponent* C : Hammasi)
		if (C)
		{
			Komponentlar.Remove(C);
			C->DestroyComponent();
		}
}

// o'yinchi shu nuqtani ko'ra oladimi (yaqin, ko'rish burchagida va oradagi bino/daraxt to'smaydi)
bool ABozorQatnov::Korinadi(const FVector& P) const
{
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC) return false;
	FVector Kamera;
	FRotator Qarash;
	PC->GetPlayerViewPoint(Kamera, Qarash);
	const FVector Nuqta = P + FVector(0.f, 0.f, 120.f);
	const float Masofa = FVector::Dist(Kamera, Nuqta);
	if (Masofa < KorinmasMasofa * 0.4f) return true;
	if (Masofa > 20000.f) return false;
	if (FVector::DotProduct((Nuqta - Kamera) / Masofa, Qarash.Vector()) < FMath::Cos(FMath::DegreesToRadians(65.f))) return false;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(BozorQatnovKorish), false, this);
	if (PC->GetPawn()) Q.AddIgnoredActor(PC->GetPawn());
	FHitResult Hit;
	return !GetWorld()->LineTraceSingleByChannel(Hit, Kamera, Nuqta, ECC_Visibility, Q);
}

// o'tish joyida yoki unga chiqmoqchi bo'lib chetida turgan piyoda bormi
bool ABozorQatnov::OtishJoyidaPiyoda(const FBozorOtishJoyi& O) const
{
	const FBox Q = FBox(O.Markaz - O.Yarim, O.Markaz + O.Yarim).ExpandBy(FVector(PiyodaZahira, PiyodaZahira, 100.f));
	for (const FVector& P : Piyodalar)
		if (Q.IsInsideXY(P) && FMath::Abs(P.Z - O.Markaz.Z) < 250.f) return true;
	return false;
}

// IDM: istalgan tezlikka intilish va oldidagi to'siq (mashina, piyoda, to'xtash chizig'i) bilan xavfsiz masofa
float ABozorQatnov::Tezlanishi(const FMashina& M, float Oraliq, float OldV) const
{
	const float A = Tezlanish, B = Tormoz;
	const float Erkin = 1.f - FMath::Pow(M.V / FMath::Max(M.V0, 1.f), 4.f);
	if (Oraliq >= 1e8f) return A * Erkin;
	const float Kerak = MinOraliq + FMath::Max(0.f, M.V * VaqtOraligi + M.V * (M.V - OldV) / (2.f * FMath::Sqrt(A * B)));
	const float G = FMath::Max(Oraliq, 5.f);
	return A * (Erkin - FMath::Square(Kerak / G));
}

void ABozorQatnov::Tick(float Dt)
{
	Super::Tick(Dt);
	Dt = FMath::Min(Dt, 0.1f);

	Piyodalar.Reset();
	for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
	{
		if (It->IsHidden()) continue;
		const float Yarim = It->GetCapsuleComponent() ? It->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.f;
		Piyodalar.Add(It->GetActorLocation() - FVector(0.f, 0.f, Yarim));
	}
	const bool bAvtobus = Avtobus.IsValid() && !Avtobus->IsHidden();
	TArray<FVector> Transport;   // Chaos mashinalari (o'yinchi haydaydigan BMW)
	for (TActorIterator<AWheeledVehiclePawn> It(GetWorld()); It; ++It) Transport.Add(It->GetActorLocation());
	const FVector AvtobusJoy = bAvtobus ? Avtobus->GetActorLocation() : FVector::ZeroVector;

	for (int32 I = 0; I < Mashinalar.Num(); ++I)
	{
		FMashina& M = Mashinalar[I];
		USplineComponent* Yol = Yollar[M.Yolak];
		const float L = Yol->GetSplineLength();
		const float Old = M.S + M.Uzunlik * 0.5f;
		const FVector Joy = Yol->GetLocationAtDistanceAlongSpline(M.S, ESplineCoordinateSpace::World);
		float V0 = M.V0;
		float Acc = TNumericLimits<float>::Max();
		bool bPiyodaUchun = false;
		bool bYoldaPiyoda = false;
		auto Tosiq = [&](float Oraliq, float OldV) { Acc = FMath::Min(Acc, Tezlanishi(M, Oraliq, OldV)); };
		// yo'l ustidagi nuqtaning bo'lakdagi masofasi va bo'lak o'qidan yon tomonga uzoqligi
		auto Proyeksiya = [&](const FVector& P, float& OutS) -> float
		{
			const float Key = Yol->FindInputKeyClosestToWorldLocation(P);
			OutS = Yol->GetDistanceAlongSplineAtSplineInputKey(Key);
			return FVector::Dist2D(P, Yol->GetLocationAtSplineInputKey(Key, ESplineCoordinateSpace::World));
		};

		// oldindagi burilish: yon tezlanish YonTezlanish dan oshmasin (v = sqrt(a * R)); burilishgacha qulay tormoz bilan
		// sekinlashadi. Egrilik - 2 m oraliqdagi ikki yo'nalish orasidagi burchak / 2 m
		{
			const float Qadam = 200.f;
			FVector D0 = Yol->GetDirectionAtDistanceAlongSpline(FMath::Min(M.S, L), ESplineCoordinateSpace::World);
			for (float D = 0.f; D < 3000.f && M.S + D + Qadam < L; D += Qadam)
			{
				const FVector D1 = Yol->GetDirectionAtDistanceAlongSpline(M.S + D + Qadam, ESplineCoordinateSpace::World);
				const float Burchak = FMath::Acos(FMath::Clamp(FVector::DotProduct(D0.GetSafeNormal2D(), D1.GetSafeNormal2D()), -1.f, 1.f));
				D0 = D1;
				if (Burchak < 0.01f) continue;
				const float VMax = FMath::Sqrt(YonTezlanish * Qadam / Burchak);
				V0 = FMath::Min(V0, FMath::Sqrt(VMax * VMax + 2.f * Tormoz * D));
			}
		}
		// oldidagi mashina
		for (const FMashina& N : Mashinalar)
			if (N.Yolak == M.Yolak && N.S > M.S)
				Tosiq(N.S - N.Uzunlik * 0.5f - Old, N.V);
		// avtobus (intro): shu bo'lakda va oldinda bo'lsa
		if (bAvtobus && FVector::Dist2D(AvtobusJoy, Joy) < 6000.f)
		{
			float SA;
			if (Proyeksiya(AvtobusJoy, SA) < 250.f && SA > M.S) Tosiq(SA - 620.f - Old, 0.f);
		}
		// o'yinchi mashinasi: bo'lakda va oldinda bo'lsa (yonidan ham urilmasin - yarim kenglik + 1.5 m)
		for (const FVector& TJ : Transport)
		{
			if (FVector::Dist2D(TJ, Joy) > 4000.f) continue;
			float ST;
			if (Proyeksiya(TJ, ST) < M.Kenglik * 0.5f + 150.f && ST > M.S - 100.f) Tosiq(ST - 260.f - Old, 0.f);
		}
		// piyoda o'tish joylari (YHQ: piyodaga yo'l berish)
		for (const FBozorOtishJoyi& O : OtishJoylari)
		{
			float SO;
			if (Proyeksiya(O.Markaz, SO) > FMath::Max(O.Yarim.X, O.Yarim.Y)) continue;
			const FVector D = Yol->GetDirectionAtDistanceAlongSpline(SO, ESplineCoordinateSpace::World);
			const float Qalinlik = FMath::Abs(D.X) * O.Yarim.X + FMath::Abs(D.Y) * O.Yarim.Y;
			const float Chet = SO - Qalinlik;   // o'tish joyining yaqin cheti
			if (Old > Chet + 30.f || Chet - Old > 4000.f) continue;
			if (OtishJoyidaPiyoda(O))
			{
				// to'xtash chizig'i oldida; ulgurmasa - o'tish joyi chetida
				const float Chiziq = Old < Chet - ToxtashChizigi - MinOraliq + 20.f ? Chet - ToxtashChizigi : Chet + MinOraliq - 40.f;
				Tosiq(Chiziq - Old, 0.f);
				M.Kutish = JonashKutish;
				bPiyodaUchun = true;
			}
			else if (M.Kutish > 0.f && M.V < 50.f)
			{
				Tosiq(Chet - ToxtashChizigi - Old, 0.f);   // piyoda o'tdi - bir lahza kutib yo'lga tushadi
				M.Kutish -= Dt;
				bPiyodaUchun = true;
			}
			else
			{
				// o'tish joyi yonida piyoda bo'lsa - sekinlashib yaqinlashadi
				const FBox Yaqin = FBox(O.Markaz - O.Yarim, O.Markaz + O.Yarim).ExpandBy(FVector(500.f, 500.f, 200.f));
				for (const FVector& P : Piyodalar)
					if (Yaqin.IsInsideXY(P) && Chet - Old < 2500.f)
					{
						V0 = FMath::Min(V0, OtishOldiTezlik * 100000.f / 3600.f);
						break;
					}
			}
		}
		// yo'ldagi piyoda (o'tish joyidan tashqarida ham): oldida to'xtaydi. Mashina yonida yoki ustida (tomiga chiqqan)
		// bo'lsa ham joyidan qo'zg'almaydi - aks holda kuzov odamni ichiga olib ketadi
		for (const FVector& P : Piyodalar)
		{
			if (FVector::Dist2D(P, Joy) > 3500.f) continue;
			float SP;
			if (Proyeksiya(P, SP) < M.Kenglik * 0.5f + 70.f && SP > M.S - M.Uzunlik * 0.5f - 60.f && SP - Old < 2500.f)
			{
				Tosiq(SP - 150.f - Old, 0.f);
				bPiyodaUchun = true;
				bYoldaPiyoda = true;
			}
		}
		// bo'lak oxiri: o'yinchi ko'rib tursa - yo'qolmaydi, oxirida to'xtab turadi
		const FVector Oxiri = Yol->GetLocationAtDistanceAlongSpline(L, ESplineCoordinateSpace::World);
		const bool bOxiriKorinadi = L - Old < 3000.f && Korinadi(Oxiri);
		if (bOxiriKorinadi) Tosiq(L - Old, 0.f);

		const float Eski = M.V0;
		M.V0 = V0;
		// erkin yo'l hadi (burilish/o'tish joyi cheklagan V0 bilan) ham doim hisobga olinadi - to'siq bo'lsa ham
		const float A = FMath::Clamp(FMath::Min(Acc, Tezlanishi(M, 1e9f, 0.f)), -MaxTormoz, Tezlanish);
		M.V0 = Eski;
		const float EskiV = M.V;
		M.V = FMath::Max(0.f, M.V + A * Dt);
		M.S += M.V * Dt;
		M.Tezlanish = (M.V - EskiV) / FMath::Max(Dt, 1e-3f);
		Fizika(M, Dt);
		M.Burchak = FMath::Fmod(M.Burchak + FMath::RadiansToDegrees(M.V * Dt / FMath::Max(Turlar[M.Tur].GildirakRadius, 5.f)), 360.f);
		if (bYoldaPiyoda && M.V < 10.f) M.YoldaKutish += Dt;
		else M.YoldaKutish = 0.f, M.Signallar = 0;

		const bool bToxtadi = bPiyodaUchun && M.V < 5.f;
		if (bToxtadi != M.bToxtagan)
		{
			M.bToxtagan = bToxtadi;
			UE_LOG(LogBozorQatnov, Display, TEXT("[Qatnov] %s (bo'lak %d, %s): %s"), *GetNameSafe(Turlar[M.Tur].Kuzov.Get()), M.Yolak,
				*Joy.ToCompactString(), bToxtadi ? TEXT("piyodaga yo'l berib to'xtadi") : TEXT("yo'lga tushdi"));
		}
		Joyla(M);
		Ovoz(M, Dt);
	}

	// bo'lak oxiriga yetgan mashinalar yo'qoladi (o'yinchi ko'rmasa)
	for (int32 I = Mashinalar.Num() - 1; I >= 0; --I)
	{
		FMashina& M = Mashinalar[I];
		const float L = Yollar[M.Yolak]->GetSplineLength();
		if (M.S + M.Uzunlik * 0.5f < L - 20.f) continue;
		if (Korinadi(Yollar[M.Yolak]->GetLocationAtDistanceAlongSpline(L, ESplineCoordinateSpace::World))) continue;
		Olib(M);
		Mashinalar.RemoveAt(I);
	}

	// yangi mashinalar bo'lak boshida (o'yinchi ko'rmaydigan bo'lsa)
	for (int32 Y = 0; Y < Yollar.Num(); ++Y)
	{
		PaydoTaymer[Y] -= Dt;
		if (PaydoTaymer[Y] > 0.f) continue;
		int32 Soni = 0;
		for (const FMashina& M : Mashinalar) Soni += M.Yolak == Y;
		const FVector Bosh = Yollar[Y]->GetLocationAtDistanceAlongSpline(0.f, ESplineCoordinateSpace::World);
		const bool bAvtobusKochada = Yolaklar[Y].bAvtobusYoli && bAvtobus;
		if (Soni >= Yolaklar[Y].MaxMashina || bAvtobusKochada || Korinadi(Bosh) || !Qosh(Y, 300.f, false))
		{
			PaydoTaymer[Y] = 1.f;   // keyinroq yana urinadi
			continue;
		}
		PaydoTaymer[Y] = FMath::FRandRange(PaydoOraligi.X, PaydoOraligi.Y);
	}

	if (bTest) TestTick(Dt);
	if (bKadr) KadrTick(Dt);
}

// -QatnovKadr: chorrahalarni yuqoridan suratga oladi (Saved/Screenshots/QatnovKadr) - burilishlarni tekshirish uchun
void ABozorQatnov::KadrTick(float Dt)
{
	TestVaqt += Dt;
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC) return;
	float Z = 3200.f;   // -QatnovKadrZ=1200: pastroqdan (g'ildirak va kuzov og'ishini ko'rish uchun)
	FParse::Value(FCommandLine::Get(), TEXT("QatnovKadrZ="), Z);
	const FVector Joylar[] = { FVector(-850.f, 4450.f, Z), FVector(-1000.f, -4550.f, Z) };
	const int32 Joy = TestVaqt < 40.f ? 0 : 1;
	if (!KadrKamera.IsValid() || KadrJoy != Joy)
	{
		if (!KadrKamera.IsValid())
			KadrKamera = GetWorld()->SpawnActor<ACameraActor>(Joylar[Joy], FRotator(-90.f, 0.f, 0.f));
		KadrKamera->SetActorLocation(Joylar[Joy]);
		KadrKamera->SetActorRotation(FRotator(-89.9f, 0.f, 0.f));
		PC->SetViewTarget(KadrKamera.Get());
		KadrJoy = Joy;
	}
	if (TestVaqt - KadrOxirgi > 1.5f && TestVaqt > 4.f)
	{
		KadrOxirgi = TestVaqt;
		FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / FString::Printf(TEXT("Screenshots/QatnovKadr/%d_%03d.png"),
			Joy, FMath::RoundToInt(TestVaqt)), false, false);
		for (const FMashina& M : Mashinalar)
		{
			const FVector D = Yollar[M.Yolak]->GetDirectionAtDistanceAlongSpline(M.S, ESplineCoordinateSpace::World);
			const FVector Old = FRotator(0.f, Turlar[M.Tur].OldYaw, 0.f).RotateVector(FVector::ForwardVector);
			const FVector OldDunyo = M.Kuzov->GetComponentQuat().RotateVector(Old);
			UE_LOG(LogBozorQatnov, Display, TEXT("[QatnovKadr] %.0f s %s bo'lak %d joy %s yo'l yaw %.0f old yaw %.0f v %.0f"), TestVaqt,
				*GetNameSafe(Turlar[M.Tur].Kuzov.Get()), M.Yolak, *M.Kuzov->GetComponentLocation().ToCompactString(),
				D.Rotation().Yaw, OldDunyo.Rotation().Yaw, M.V);
		}
	}
	if (TestVaqt > 80.f) FGenericPlatformMisc::RequestExit(false);
}

// -QatnovTest: o'yinchi o'tish joyida turadi (mashinalar to'xtashi kerak), keyin yo'lakka chiqadi (yo'lga tushishi kerak)
void ABozorQatnov::TestTick(float Dt)
{
	TestVaqt += Dt;
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn || OtishJoylari.Num() == 0) return;
	const FBozorOtishJoyi& O = OtishJoylari[0];
	const FString Dir = FPaths::ProjectSavedDir() / TEXT("Screenshots/QatnovTest/");
	auto Holat = [&]()
	{
		int32 Toxtagan = 0;
		for (const FMashina& M : Mashinalar) Toxtagan += M.bToxtagan;
		UE_LOG(LogBozorQatnov, Display, TEXT("[QatnovTest] %.0f s: mashina %d, piyoda uchun to'xtagan %d"), TestVaqt, Mashinalar.Num(), Toxtagan);
	};
	if (TestQadam == 0 && TestVaqt > 1.f)
	{
		// o'tish joyining o'rtasida, janubga (shimolga ketayotgan bo'lakka) qarab
		Pawn->SetActorLocation(O.Markaz + FVector(0.f, 0.f, 100.f), false, nullptr, ETeleportType::TeleportPhysics);
		PC->SetControlRotation(FRotator(-12.f, -90.f, 0.f));
		TestQadam = 1;
	}
	else if (TestQadam == 1 && TestVaqt > 25.f)
	{
		Holat();
		FScreenshotRequest::RequestScreenshot(Dir + TEXT("1_otish_joyida.png"), false, false);
		TestQadam = 2;
	}
	else if (TestQadam == 2 && TestVaqt > 26.f && TestVaqt < 27.f)
	{
		// o'yinchi to'xtab turgan mashina tomiga tushadi: kuzov ichiga kirib ketmasligi kerak (kolliziya)
		for (const FMashina& M : Mashinalar)
			if (M.bToxtagan)
			{
				TomMashina = M.Kuzov;
				Pawn->SetActorLocation(M.Kuzov->GetComponentLocation() + FVector(0.f, 0.f, 400.f), false, nullptr, ETeleportType::TeleportPhysics);
				PC->SetControlRotation(FRotator(-35.f, M.Kuzov->GetComponentRotation().Yaw, 0.f));
				break;
			}
		TestVaqt = 27.f;
	}
	else if (TestQadam == 2 && TestVaqt > 30.f && TestVaqt < 31.f)
	{
		if (TomMashina.IsValid())
		{
			const FBox B = TomMashina->Bounds.GetBox();
			const float Oyoq = Pawn->GetActorLocation().Z - Cast<ACharacter>(Pawn)->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			UE_LOG(LogBozorQatnov, Display, TEXT("[QatnovTest] tomda: oyoq z %.0f, mashina tomi z %.0f -> %s"), Oyoq, B.Max.Z,
				Oyoq > B.Max.Z - 60.f ? TEXT("TOMIDA (to'g'ri)") : TEXT("ICHIGA KIRIB KETDI (xato)"));
		}
		FScreenshotRequest::RequestScreenshot(Dir + TEXT("1b_tomida.png"), false, false);
		TestVaqt = 31.f;
	}
	else if (TestQadam == 2 && TestVaqt > 31.f)
	{
		// o'yinchi sharqiy yo'lakka chiqadi (bordyurdan ~1.5 m, o'tish joyidan chetda), yo'lga qarab turadi
		Pawn->SetActorLocation(O.Markaz + FVector(O.Yarim.X + 180.f, -400.f, 100.f), false, nullptr, ETeleportType::TeleportPhysics);
		PC->SetControlRotation(FRotator(-10.f, -150.f, 0.f));
		TestQadam = 3;
	}
	else if (TestQadam == 3 && TestVaqt > 36.f)
	{
		Holat();
		FScreenshotRequest::RequestScreenshot(Dir + TEXT("2_yolakda.png"), false, false);
		TestQadam = 4;
	}
	else if (TestQadam == 4 && TestVaqt > 40.f)
	{
		Holat();
		FScreenshotRequest::RequestScreenshot(Dir + TEXT("3_qatnov.png"), false, false);
		// o'yinchi o'tish joyidan tashqarida yo'l o'rtasiga (shimolga ketuvchi bo'lak) chiqadi: mashina to'xtab, signal berishi kerak
		Pawn->SetActorLocation(FVector(-1150.f, -1500.f, 100.f), false, nullptr, ETeleportType::TeleportPhysics);
		PC->SetControlRotation(FRotator(-8.f, -90.f, 0.f));
		TestQadam = 5;
	}
	else if (TestQadam == 5 && TestVaqt > 53.f)
	{
		Holat();
		FScreenshotRequest::RequestScreenshot(Dir + TEXT("4_yolda.png"), false, false);
		TestQadam = 6;
	}
	else if (TestQadam == 6 && TestVaqt > 54.f)
	{
		UE_LOG(LogBozorQatnov, Display, TEXT("[QatnovTest] tugadi"));
		FGenericPlatformMisc::RequestExit(false);
	}
}
