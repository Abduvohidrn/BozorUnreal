#include "BozorMashina.h"
#include "BozorMobil.h"

#include "BozorSozlamalar.h"
#include "Camera/CameraComponent.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

DEFINE_LOG_CATEGORY_STATIC(LogBozorMashina, Log, All);

namespace
{
	// suyak nomlari: g'ildirak mesh'lari va Chaos g'ildiraklari shu tartibda
	const FName GildirakSuyaklari[] = { TEXT("wheel_fl"), TEXT("wheel_fr"), TEXT("wheel_rl"), TEXT("wheel_rr") };
}

UBozorGildirakOld::UBozorGildirakOld()
{
	AxleType = EAxleType::Front;
	WheelRadius = 35.4f;
	WheelWidth = 30.f;
	bAffectedBySteering = true;
	MaxSteerAngle = 38.f;
	bAffectedByEngine = true;
	bAffectedByHandbrake = false;
	FrictionForceMultiplier = 3.f;
	CorneringStiffness = 1000.f;
	SuspensionMaxRaise = 8.f;
	SuspensionMaxDrop = 10.f;
	SpringRate = 250.f;
	SuspensionDampingRatio = 0.5f;
	MaxBrakeTorque = 3000.f;
}

UBozorGildirakOrqa::UBozorGildirakOrqa()
{
	AxleType = EAxleType::Rear;
	WheelRadius = 35.4f;
	WheelWidth = 30.f;
	bAffectedBySteering = false;
	bAffectedByEngine = true;
	bAffectedByHandbrake = true;
	FrictionForceMultiplier = 3.f;
	CorneringStiffness = 1000.f;
	SuspensionMaxRaise = 8.f;
	SuspensionMaxDrop = 10.f;
	SpringRate = 250.f;
	SuspensionDampingRatio = 0.5f;
	MaxBrakeTorque = 2500.f;
	MaxHandBrakeTorque = 6000.f;
}

ABozorMashina::ABozorMashina(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;

	UChaosWheeledVehicleMovementComponent* Hk = CastChecked<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement());
	Hk->ChassisHeight = 146.f;
	Hk->ChassisWidth = 190.f;
	Hk->DragCoefficient = 0.3f;
	Hk->Mass = 1900.f;
	Hk->WheelSetups.SetNum(4);
	for (int32 I = 0; I < 4; ++I)
	{
		Hk->WheelSetups[I].BoneName = GildirakSuyaklari[I];
		Hk->WheelSetups[I].WheelClass = I < 2 ? UBozorGildirakOld::StaticClass() : UBozorGildirakOrqa::StaticClass();
	}
	// BMW M5 F90: 600 o.k., 750 Nm, to'liq privod, avtomat
	Hk->EngineSetup.MaxTorque = 750.f;
	Hk->EngineSetup.MaxRPM = 7000.f;
	Hk->EngineSetup.EngineIdleRPM = 900.f;
	Hk->EngineSetup.EngineBrakeEffect = 0.2f;
	Hk->EngineSetup.TorqueCurve.GetRichCurve()->Reset();
	Hk->EngineSetup.TorqueCurve.GetRichCurve()->AddKey(0.f, 500.f);
	Hk->EngineSetup.TorqueCurve.GetRichCurve()->AddKey(1800.f, 750.f);
	Hk->EngineSetup.TorqueCurve.GetRichCurve()->AddKey(5800.f, 750.f);
	Hk->EngineSetup.TorqueCurve.GetRichCurve()->AddKey(7000.f, 600.f);
	Hk->DifferentialSetup.DifferentialType = EVehicleDifferential::AllWheelDrive;
	Hk->TransmissionSetup.bUseAutomaticGears = true;

	GetMesh()->SetCollisionProfileName(UCollisionProfile::Vehicle_ProfileName);
	GetMesh()->SetSimulatePhysics(true);
	GetMesh()->SetCanEverAffectNavigation(false);

	Kuzov = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Kuzov"));
	Kuzov->SetupAttachment(GetMesh());
	Kuzov->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Oyna = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Oyna"));
	Oyna->SetupAttachment(GetMesh());
	Oyna->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Oyna->SetCastShadow(false);
	Raqam = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Raqam"));
	Raqam->SetupAttachment(GetMesh());
	Raqam->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Qol = CreateDefaultSubobject<USpringArmComponent>(TEXT("Qol"));
	Qol->SetupAttachment(GetMesh());
	Qol->SetRelativeLocation(FVector(0.f, 0.f, 130.f));
	Qol->TargetArmLength = 620.f;
	Qol->SocketOffset = FVector(0.f, 0.f, 60.f);
	Qol->bInheritPitch = false;
	Qol->bInheritRoll = false;
	Qol->bEnableCameraLag = true;
	Qol->CameraLagSpeed = 8.f;
	Qol->bEnableCameraRotationLag = true;
	Qol->CameraRotationLagSpeed = 6.f;
	Kamera = CreateDefaultSubobject<UCameraComponent>(TEXT("Kamera"));
	Kamera->SetupAttachment(Qol);
	Kamera->FieldOfView = 80.f;

	Dvigatel = CreateDefaultSubobject<UAudioComponent>(TEXT("Dvigatel"));
	Dvigatel->SetupAttachment(GetMesh());
	Dvigatel->bAutoActivate = false;
	Signal = CreateDefaultSubobject<UAudioComponent>(TEXT("Signal"));
	Signal->SetupAttachment(GetMesh());
	Signal->bAutoActivate = false;
}

void ABozorMashina::BeginPlay()
{
	Super::BeginPlay();
	// g'ildirak/oyna/raqam mesh'lari: old tomoni lokal MeshOldYaw -> mashina oldi +X
	const FRotator MeshBurchak(0.f, -MeshOldYaw, 0.f);
	if (KuzovMesh)
	{
		Kuzov->SetStaticMesh(KuzovMesh);
		Kuzov->SetRelativeRotation(MeshBurchak);
		GetMesh()->SetVisibility(false);   // faqat fizika (bolalari - kuzov, g'ildirak, oyna - ko'rinadi)
	}
	if (OynaMesh) { Oyna->SetStaticMesh(OynaMesh); Oyna->SetRelativeRotation(MeshBurchak); }
	if (RaqamMesh) { Raqam->SetStaticMesh(RaqamMesh); Raqam->SetRelativeRotation(MeshBurchak); }
	for (int32 I = 0; I < 4 && I < GildirakMeshlari.Num(); ++I)
	{
		UStaticMeshComponent* G = NewObject<UStaticMeshComponent>(this);
		G->SetupAttachment(GetMesh());
		G->SetStaticMesh(GildirakMeshlari[I]);
		G->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		G->RegisterComponent();
		Gildiraklar.Add(G);
		GildirakJoylari.Add(GetMesh()->GetSocketTransform(GildirakSuyaklari[I], RTS_Component).GetLocation());
	}
	for (UAudioComponent* A : { Dvigatel.Get(), Signal.Get() }) A->AttenuationSettings = OvozSonishi;
	if (DvigatelOvozi) Dvigatel->SetSound(DvigatelOvozi);
	if (SignalOvozi) Signal->SetSound(SignalOvozi);
	if (MaterialManbasi)
	{
		int32 Qoyildi = 0;
		for (const FStaticMaterial& M : MaterialManbasi->GetStaticMaterials())
			if (GetMesh()->GetMaterialIndex(M.MaterialSlotName) != INDEX_NONE)
			{
				GetMesh()->SetMaterialByName(M.MaterialSlotName, M.MaterialInterface);
				++Qoyildi;
			}
		UE_LOG(LogBozorMashina, Display, TEXT("[Mashina] kuzov materiallari %s dan: %d / %d slot"), *MaterialManbasi->GetName(), Qoyildi,
			GetMesh()->GetNumMaterials());
	}
	// sozlamalardagi r.SkeletalMeshLODBias (2) kuzovni kuchli soddalashtirilgan LOD'da ko'rsatardi - doim eng aniq LOD
	GetMesh()->SetForcedLOD(1);
	GetVehicleMovementComponent()->SetHandbrakeInput(true);   // turgan mashina joyidan siljimasin
	GildiraklarniJoyla();
	bTest = FParse::Param(FCommandLine::Get(), TEXT("MashinaTest"));
	UE_LOG(LogBozorMashina, Display, TEXT("[Mashina] %s tayyor: g'ildirak %d, massa %.0f kg, LOD %d"), *GetName(), Gildiraklar.Num(),
		GetMesh()->GetMass(), GetMesh()->GetSkeletalMeshAsset() ? GetMesh()->GetSkeletalMeshAsset()->GetLODNum() : 0);
}

FString ABozorMashina::Maslahat() const
{
	const FString Tugma = UBozorSozlamalar::TugmaNomi(UBozorSozlamalar::Get()->Tugma(TEXT("Mashina"))).ToString();
	if (Haydovchi.IsValid())
	{
		const UChaosWheeledVehicleMovementComponent* Hk = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent());
		const int32 Uzatma = Hk ? Hk->GetCurrentGear() : 0;
		const FString Tezlik = FString::Printf(TEXT("%.0f km/soat   %s"),
			FMath::Abs(GetVehicleMovementComponent()->GetForwardSpeed()) * 0.036f,
			Uzatma < 0 ? TEXT("R") : Uzatma == 0 ? TEXT("N") : *FString::FromInt(Uzatma));
		if (BozorMobil::Faol()) return Tezlik;
		return Tezlik + FString::Printf(TEXT("   |   W/S - gaz/tormoz, A/D - rul, Probel - qo'l tormozi, H - signal, %s - tushish"), *Tugma);
	}
	return bYonida && !BozorMobil::Faol() ? FString::Printf(TEXT("%s - mashinaga o'tirish"), *Tugma) : FString();
}

void ABozorMashina::Tick(float Dt)
{
	Super::Tick(Dt);
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const FKey Tugma = UBozorSozlamalar::Get()->Tugma(TEXT("Mashina"));
	if (bTest) TestTick(Dt);
	if (Haydovchi.IsValid())
	{
		if (!bTest) Boshqaruv(Dt);
		const float Tezlik = FMath::Abs(GetVehicleMovementComponent()->GetForwardSpeed());
		if (PC && PC->WasInputKeyJustPressed(Tugma) && Tezlik < 500.f) Tush();
	}
	else
	{
		ACharacter* Oyinchi = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
		bYonida = Oyinchi && FVector::Dist2D(Oyinchi->GetActorLocation(), GetActorLocation()) < OtirishMasofasi;
		if (bYonida && PC->WasInputKeyJustPressed(Tugma)) Otir();
	}
	GildiraklarniJoyla();
	// dvigatel: aylanish tezligiga qarab pitch (dizel namunasi -> benzin dvigateli kabi baland)
	if (Dvigatel->IsPlaying())
	{
		const UChaosWheeledVehicleMovementComponent* Hk = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent());
		const float Nisbat = Hk ? FMath::Clamp(Hk->GetEngineRotationSpeed() / Hk->EngineSetup.MaxRPM, 0.f, 1.f) : 0.f;
		Dvigatel->SetPitchMultiplier(FMath::Lerp(1.0f, 2.6f, Nisbat));
		Dvigatel->SetVolumeMultiplier(FMath::Lerp(0.35f, 0.9f, Nisbat));
	}
	if (SignalTugash > 0.f && GetWorld()->GetTimeSeconds() > SignalTugash)
	{
		Signal->Stop();
		SignalTugash = -1.f;
	}
}

// Chaos g'ildirak holati: aylanish (pitch), rul (yaw), osma (balandlik) - VehicleAnimationInstance bilan bir xil hisob
void ABozorMashina::GildiraklarniJoyla()
{
	const UChaosWheeledVehicleMovementComponent* Hk = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent());
	const FQuat MeshBurchak = FRotator(0.f, -MeshOldYaw, 0.f).Quaternion();
	for (int32 I = 0; I < Gildiraklar.Num(); ++I)
	{
		FRotator Aylanish = FRotator::ZeroRotator;
		FVector Osma = FVector::ZeroVector;
		if (Hk && Hk->Wheels.IsValidIndex(I) && Hk->Wheels[I])
		{
			const UChaosVehicleWheel* W = Hk->Wheels[I];
			Aylanish = FRotator(W->GetRotationAngle(), W->GetSteerAngle(), 0.f);
			Osma = -W->GetSuspensionAxis() * W->GetSuspensionOffset();
		}
		Gildiraklar[I]->SetRelativeLocationAndRotation(GildirakJoylari[I] + Osma, Aylanish.Quaternion() * MeshBurchak);
	}
}

void ABozorMashina::Boshqaruv(float Dt)
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	UChaosVehicleMovementComponent* Hk = GetVehicleMovementComponent();
	if (!PC || !Hk) return;
	const float Gaz = PC->IsInputKeyDown(EKeys::W) || PC->IsInputKeyDown(EKeys::Up) ? 1.f : 0.f;
	const float Tormoz = PC->IsInputKeyDown(EKeys::S) || PC->IsInputKeyDown(EKeys::Down) ? 1.f : 0.f;
	const float Rul = (PC->IsInputKeyDown(EKeys::D) || PC->IsInputKeyDown(EKeys::Right) ? 1.f : 0.f)
		- (PC->IsInputKeyDown(EKeys::A) || PC->IsInputKeyDown(EKeys::Left) ? 1.f : 0.f);
	Hk->SetThrottleInput(FMath::Max(Gaz, static_cast<float>(MobilVektor.Y)));
	Hk->SetBrakeInput(FMath::Max(Tormoz, static_cast<float>(-MobilVektor.Y)));
	Hk->SetSteeringInput(FMath::Clamp(Rul + static_cast<float>(MobilVektor.X), -1.f, 1.f));
	Hk->SetHandbrakeInput(PC->IsInputKeyDown(EKeys::SpaceBar));
	if (PC->WasInputKeyJustPressed(EKeys::H) && SignalOvozi)
	{
		Signal->Play();
		SignalTugash = GetWorld()->GetTimeSeconds() + 0.4f;
	}
	// kamera: sichqoncha bilan aylantirish, 2 s tegilmasa mashina orqasiga qaytadi
	float Mx = 0.f, My = 0.f;
	PC->GetInputMouseDelta(Mx, My);
	Mx += MobilKameraDelta.X;
	My += MobilKameraDelta.Y;
	MobilKameraDelta = FVector2D::ZeroVector;
	if (FMath::Abs(Mx) + FMath::Abs(My) > 0.01f) SichqonchaVaqt = 0.f;
	else SichqonchaVaqt += Dt;
	KameraYaw = FMath::Fmod(KameraYaw + Mx * 2.f, 360.f);
	KameraPitch = FMath::Clamp(KameraPitch + My * 2.f, -45.f, 10.f);
	if (SichqonchaVaqt > 2.f)
	{
		KameraYaw = FMath::FInterpTo(FRotator::NormalizeAxis(KameraYaw), 0.f, Dt, 2.f);
		KameraPitch = FMath::FInterpTo(KameraPitch, -12.f, Dt, 2.f);
	}
	Qol->SetRelativeRotation(FRotator(KameraPitch, KameraYaw, 0.f));
}

void ABozorMashina::MobilOtirTush()
{
	if (Haydovchi.IsValid())
	{
		if (FMath::Abs(GetVehicleMovementComponent()->GetForwardSpeed()) < 500.f) Tush();
		return;
	}
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const ACharacter* O = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
	if (O && FVector::DistSquared2D(O->GetActorLocation(), GetActorLocation()) < FMath::Square(OtirishMasofasi)) Otir();
}

void ABozorMashina::Otir()
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	ACharacter* C = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
	if (!C) return;
	Haydovchi = C;
	C->GetCharacterMovement()->StopMovementImmediately();
	C->GetCharacterMovement()->DisableMovement();
	C->SetActorEnableCollision(false);
	C->SetActorHiddenInGame(true);
	C->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	C->SetActorRelativeLocation(FVector(-20.f, -40.f, 90.f));   // haydovchi o'rindig'i (chap)
	KameraYaw = 0.f;
	KameraPitch = -12.f;
	PC->Possess(this);
	PC->SetViewTargetWithBlend(this, 0.5f);
	GetVehicleMovementComponent()->SetHandbrakeInput(false);
	if (DvigatelOvozi) Dvigatel->Play();
	UE_LOG(LogBozorMashina, Display, TEXT("[Mashina] o'yinchi %s ga o'tirdi"), *GetName());
}

void ABozorMashina::Tush()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	ACharacter* C = Haydovchi.Get();
	Haydovchi.Reset();
	MobilVektor = FVector2D::ZeroVector;
	MobilKameraDelta = FVector2D::ZeroVector;
	UChaosVehicleMovementComponent* Hk = GetVehicleMovementComponent();
	Hk->SetThrottleInput(0.f);
	Hk->SetSteeringInput(0.f);
	Hk->SetBrakeInput(0.f);
	Hk->SetHandbrakeInput(true);
	Dvigatel->Stop();
	if (!C || !PC) return;
	C->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	// haydovchi eshigi - chap tomon; joy band bo'lsa o'ng tomon
	const float Yarim = C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	FVector Joy = GetActorLocation() - GetActorRightVector() * 190.f + FVector(0.f, 0.f, Yarim + 20.f);
	C->SetActorEnableCollision(true);
	C->SetActorHiddenInGame(false);
	if (!C->TeleportTo(Joy, FRotator(0.f, GetActorRotation().Yaw, 0.f)))
	{
		Joy = GetActorLocation() + GetActorRightVector() * 190.f + FVector(0.f, 0.f, Yarim + 20.f);
		C->TeleportTo(Joy, FRotator(0.f, GetActorRotation().Yaw, 0.f));
	}
	C->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	PC->Possess(C);
	PC->SetControlRotation(FRotator(0.f, GetActorRotation().Yaw + 90.f, 0.f));
	UE_LOG(LogBozorMashina, Display, TEXT("[Mashina] o'yinchi %s dan tushdi"), *GetName());
}

void ABozorMashina::TestTick(float Dt)
{
	TestVaqt += Dt;
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	ACharacter* C = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
	UChaosWheeledVehicleMovementComponent* Hk = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent());
	const FString Dir = FPaths::ProjectSavedDir() / TEXT("Screenshots/MashinaTest/");
	auto Holat = [&](const TCHAR* Nom)
	{
		const UChaosVehicleWheel* W = Hk && Hk->Wheels.Num() ? Hk->Wheels[0].Get() : nullptr;
		UE_LOG(LogBozorMashina, Display, TEXT("[MashinaTest] %.1f s %s: joy %s yaw %.0f tezlik %.0f km/soat uzatma %d rpm %.0f g'ildirak burchagi %.0f rul %.0f"),
			TestVaqt, Nom, *GetActorLocation().ToCompactString(), GetActorRotation().Yaw, Hk ? Hk->GetForwardSpeed() * 0.036f : 0.f,
			Hk ? Hk->GetCurrentGear() : 0, Hk ? Hk->GetEngineRotationSpeed() : 0.f, W ? W->GetRotationAngle() : 0.f, W ? W->GetSteerAngle() : 0.f);
		FScreenshotRequest::RequestScreenshot(Dir + FString::Printf(TEXT("%d_%s.png"), TestQadam, Nom), false, false);
	};
	if (TestQadam == 0 && TestVaqt > 2.f && C)
	{
		Holat(TEXT("boshida"));
		C->TeleportTo(GetActorLocation() - GetActorRightVector() * 250.f + FVector(0.f, 0.f, 100.f), GetActorRotation());
		TestQadam = 1;
	}
	else if (TestQadam == 1 && TestVaqt > 3.f)
	{
		UE_LOG(LogBozorMashina, Display, TEXT("[MashinaTest] yonida: %s, maslahat: %s"), bYonida ? TEXT("ha") : TEXT("yo'q"), *Maslahat());
		Otir();
		TestQadam = 2;
	}
	else if (TestQadam == 2)
	{
		// 4 s gaz (to'g'ri), keyin 3 s gaz + chapga rul, keyin tormoz
		const float T = TestVaqt - 3.f;
		if (Hk)
		{
			Hk->SetThrottleInput(T < 7.f ? 1.f : 0.f);
			Hk->SetSteeringInput(T > 4.f && T < 7.f ? -1.f : 0.f);
			Hk->SetBrakeInput(T >= 7.f ? 1.f : 0.f);
		}
		if (T > 2.f && T - Dt <= 2.f) Holat(TEXT("gaz"));
		if (T > 5.5f && T - Dt <= 5.5f) Holat(TEXT("burilish"));
		if (T > 10.f)
		{
			Holat(TEXT("tormoz"));
			Tush();
			TestQadam = 3;
		}
	}
	else if (TestQadam == 3 && TestVaqt > 15.f)
	{
		Holat(TEXT("tushdi"));
		UE_LOG(LogBozorMashina, Display, TEXT("[MashinaTest] o'yinchi: %s, ko'rinadi: %s"),
			C ? *C->GetActorLocation().ToCompactString() : TEXT("-"), C && !C->IsHidden() ? TEXT("ha") : TEXT("yo'q"));
		TestQadam = 4;
	}
	else if (TestQadam == 4 && TestVaqt > 16.f)
	{
		UE_LOG(LogBozorMashina, Display, TEXT("[MashinaTest] tugadi"));
		FGenericPlatformMisc::RequestExit(false);
	}
}
