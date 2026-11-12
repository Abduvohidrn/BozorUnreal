#include "BozorAvtobus.h"

#include "Camera/CameraActor.h"
#include "Components/AudioComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Font.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogBozorAvtobus, Log, All);

// ============================================================ bekat

ABozorBekat::ABozorBekat()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent->SetMobility(EComponentMobility::Static);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Kub(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> LedMat(TEXT("/Game/City_Bus/Materials/Font/M_Destination_Font.M_Destination_Font"));
	static ConstructorHelpers::FObjectFinder<UFont> LedFont(TEXT("/Game/City_Bus/Materials/Font/Font_DestinationSign.Font_DestinationSign"));

	auto Mesh = [this](const TCHAR* Nom, UStaticMesh* M)
	{
		UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(Nom);
		C->SetupAttachment(RootComponent);
		C->SetMobility(EComponentMobility::Static);
		if (M) C->SetStaticMesh(M);
		return C;
	};
	auto Matn = [this](const TCHAR* Nom, USceneComponent* Ota, float Kattalik, const FColor& Rang)
	{
		UTextRenderComponent* T = CreateDefaultSubobject<UTextRenderComponent>(Nom);
		T->SetupAttachment(Ota);
		T->SetHorizontalAlignment(EHTA_Center);
		T->SetVerticalAlignment(EVRTA_TextCenter);
		T->SetWorldSize(Kattalik);
		T->SetTextRenderColor(Rang);
		T->SetMobility(EComponentMobility::Static);
		return T;
	};

	Soyabon = Mesh(TEXT("Soyabon"), nullptr);
	Oyna = Mesh(TEXT("Oyna"), nullptr);
	Oyna->SetupAttachment(Soyabon);
	Ustun = Mesh(TEXT("Ustun"), nullptr);
	Belgi = Mesh(TEXT("Belgi"), Kub.Object);
	Belgi->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TabloQuti = Mesh(TEXT("TabloQuti"), Kub.Object);
	TabloQuti->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	BelgiMatn = Matn(TEXT("BelgiMatn"), Belgi, 13.f, FColor::White);
	BelgiOrqa = Matn(TEXT("BelgiOrqa"), Belgi, 13.f, FColor::White);
	NomMatn = Matn(TEXT("NomMatn"), Belgi, 9.f, FColor::White);
	TabloMatn = Matn(TEXT("TabloMatn"), TabloQuti, 7.f, FColor(255, 140, 20));
	TabloMatn->SetHorizontalAlignment(EHTA_Left);
	if (LedMat.Succeeded()) TabloMatn->SetTextMaterial(LedMat.Object);
	if (LedFont.Succeeded()) TabloMatn->SetFont(LedFont.Object);
}

// Belgi va tablo soyabon o'lchamiga qarab joylanadi (model almashsa ham o'z joyida qoladi)
void ABozorBekat::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// paketdagi modellar bitta sahnada yonma-yon turadi (pivot - sahna markazi): soyabon aktor markaziga ko'chiriladi
	const FRotator Bur(0.f, SoyabonBurilish, 0.f);
	FBox B(FVector(-80.f, -250.f, 0.f), FVector(80.f, 250.f, 260.f));
	if (const UStaticMesh* M = Soyabon->GetStaticMesh())
	{
		const FBox Asl = M->GetBoundingBox();
		FVector Siljish = -Bur.RotateVector(Asl.GetCenter());
		Siljish.Z = 0.f;
		Soyabon->SetRelativeLocationAndRotation(Siljish, Bur);
		B = Asl.TransformBy(Soyabon->GetRelativeTransform());
	}

	// ko'k belgi ustunda, soyabon yonida, yo'l chetiga yaqin; yuzlari yo'l bo'ylab - ikkala yo'nalishdagi haydovchi ko'radi
	const FVector UstunJoy(B.Max.X + 60.f, UstunTomon > 0.f ? B.Max.Y + 80.f : B.Min.Y - 80.f, 0.f);
	constexpr float UstunBoyi = 290.f;
	Ustun->SetStaticMesh(Belgi->GetStaticMesh());
	Ustun->SetRelativeScale3D(FVector(0.06f, 0.06f, UstunBoyi / 100.f));
	Ustun->SetRelativeLocation(UstunJoy + FVector(0.f, 0.f, UstunBoyi * 0.5f));
	if (UstunMaterial) Ustun->SetMaterial(0, UstunMaterial);
	Belgi->SetRelativeLocation(FVector(UstunJoy.X, UstunJoy.Y, UstunBoyi - 40.f));
	Belgi->SetRelativeRotation(FRotator::ZeroRotator);
	Belgi->SetRelativeScale3D(FVector(0.6f, 0.03f, 0.75f));
	if (KokMaterial) Belgi->SetMaterial(0, KokMaterial);

	// matnlar kubning ikki yuzida (kub 100 sm, masshtab ota-komponentdan keladi - matn uchun teskarisi)
	const FVector Teskari(1.f / 0.6f, 1.f / 0.03f, 1.f / 0.75f);
	const FString BelgiYozuv = FString::Printf(TEXT("AVTOBUS\nBEKATI\n%s"), *Yonalishlar);
	for (UTextRenderComponent* T : { BelgiMatn.Get(), BelgiOrqa.Get() })
	{
		const bool bOrqa = T == BelgiOrqa;
		T->SetText(FText::FromString(BelgiYozuv));
		T->SetRelativeScale3D(Teskari);
		T->SetRelativeLocation(FVector(0.f, bOrqa ? 52.f : -52.f, 8.f));
		T->SetRelativeRotation(FRotator(0.f, bOrqa ? 90.f : -90.f, 0.f));
	}
	NomMatn->SetText(FText::FromString(BekatNomi));
	NomMatn->SetRelativeScale3D(Teskari);
	NomMatn->SetRelativeLocation(FVector(0.f, -52.f, -38.f));
	NomMatn->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));

	// elektron tablo: soyabon tomi ostida, old tomonida, yo'lga qarab
	TabloQuti->SetRelativeLocation(FVector(B.Max.X - 25.f, B.GetCenter().Y, B.Max.Z - 70.f));
	TabloQuti->SetRelativeRotation(FRotator::ZeroRotator);
	TabloQuti->SetRelativeScale3D(FVector(0.06f, 1.1f, 0.32f));
	if (QoraMaterial) TabloQuti->SetMaterial(0, QoraMaterial);
	TabloMatn->SetText(FText::FromString(Tablo));
	TabloMatn->SetRelativeScale3D(FVector(1.f / 0.06f, 1.f / 1.1f, 1.f / 0.32f));
	TabloMatn->SetRelativeLocation(FVector(52.f, -48.f, 0.f));
	TabloMatn->SetRelativeRotation(FRotator::ZeroRotator);
}

// ============================================================ intro

ABozorAvtobusIntro::ABozorAvtobusIntro()
{
	PrimaryActorTick.bCanEverTick = true;
	// avtobus BP'si o'z Tick'ida g'ildiraklarni qo'yishi mumkin - biz undan keyin aylantiramiz
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Kelish = CreateDefaultSubobject<USplineComponent>(TEXT("Kelish"));
	Kelish->SetupAttachment(RootComponent);
	Ketish = CreateDefaultSubobject<USplineComponent>(TEXT("Ketish"));
	Ketish->SetupAttachment(RootComponent);
}

void ABozorAvtobusIntro::YolQur(USplineComponent* S, const TArray<FVector>& P) const
{
	S->ClearSplinePoints(false);
	for (const FVector& X : P) S->AddSplinePoint(X, ESplineCoordinateSpace::World, false);
	S->UpdateSpline();
}

void ABozorAvtobusIntro::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	YolQur(Kelish, KelishNuqtalari);
	TArray<FVector> K = KetishNuqtalari;
	if (KelishNuqtalari.Num() > 0) K.Insert(KelishNuqtalari.Last(), 0);
	YolQur(Ketish, K);
}

// Kolliziyani o'chirib bo'lmaydi: o'rindiqlar tanasi avtobus tanasiga payvand qilingan (ensure). Shuning uchun yashirib, ovozini
// to'xtatib, xarita ostiga olib ketiladi
void ABozorAvtobusIntro::AvtobusniYoqot()
{
	if (!Avtobus) return;
	TArray<UAudioComponent*> Ovozlar;
	Avtobus->GetComponents(Ovozlar);
	for (UAudioComponent* A : Ovozlar) A->Stop();
	Avtobus->SetActorHiddenInGame(true);
	Avtobus->SetActorTickEnabled(false);
	Avtobus->SetActorLocation(FVector(0.f, 0.f, -20000.f), false, nullptr, ETeleportType::TeleportPhysics);
}

FTransform ABozorAvtobusIntro::AvtobusTransform() const
{
	return Avtobus ? Avtobus->GetActorTransform() : FTransform::Identity;
}

void ABozorAvtobusIntro::BeginPlay()
{
	Super::BeginPlay();
	OnConstruction(GetActorTransform());

	const TCHAR* Cmd = FCommandLine::Get();
	const bool bYoq = FParse::Param(Cmd, TEXT("AvtobusYoq")) || FParse::Param(Cmd, TEXT("SotuvTest")) || FParse::Param(Cmd, TEXT("BozorAutoTest"))
		|| FParse::Param(Cmd, TEXT("BozorQadamTest"));
	bTest = FParse::Param(Cmd, TEXT("AvtobusTest"));
	if (!Avtobus || KelishNuqtalari.Num() < 2 || bYoq)
	{
		AvtobusniYoqot();
		UE_LOG(LogBozorAvtobus, Display, TEXT("[Avtobus] intro yo'q (avtobus=%s, nuqta=%d)"), *GetNameSafe(Avtobus), KelishNuqtalari.Num());
		Holat = EBosqich::Tugadi;
		SetActorTickEnabled(false);
		return;
	}

	// fizikasiz: yo'lni biz beramiz
	TArray<UPrimitiveComponent*> Prims;
	Avtobus->GetComponents(Prims);
	for (UPrimitiveComponent* P : Prims)
		if (P->IsSimulatingPhysics()) P->SetSimulatePhysics(false);
	if (UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Avtobus->GetRootComponent()))
		Root->SetPhysicsLinearVelocity(FVector::ZeroVector);

	// o'rta eshik: avtobus o'ng tomonidagi eshik ramalaridan markazga eng yaqini
	const FTransform Bt = AvtobusTransform();
	TArray<UStaticMeshComponent*> Meshes;
	Avtobus->GetComponents(Meshes);
	float Eng = MAX_flt;
	for (UStaticMeshComponent* M : Meshes)
	{
		const FString Nom = GetNameSafe(M->GetStaticMesh());
		if (Nom.StartsWith(TEXT("SM_Door_Frame")))
		{
			const FVector L = Bt.InverseTransformPosition(M->Bounds.Origin);
			UE_LOG(LogBozorAvtobus, Display, TEXT("[Avtobus] eshik ramasi %s: %s"), *M->GetName(), *L.ToString());
			if (FMath::Abs(L.X) < Eng && L.Y > 0.f)
			{
				Eng = FMath::Abs(L.X);
				EshikMahalliy = FVector(L.X, L.Y, 0.f);
			}
		}
		if (Nom.StartsWith(TEXT("SM_Wheel")))
		{
			FGildirak G;
			G.C = M;
			Gildiraklar.Add(G);
		}
	}
	if (Eng == MAX_flt)
	{
		EshikMahalliy = FVector(0.f, 125.f, 0.f);
		UE_LOG(LogBozorAvtobus, Warning, TEXT("[Avtobus] eshik ramasi topilmadi - taxminiy joy"));
	}
	UE_LOG(LogBozorAvtobus, Display, TEXT("[Avtobus] o'rta eshik %s, g'ildirak %d"), *EshikMahalliy.ToString(), Gildiraklar.Num());

	Yol = 0.f;
	V = Tezlik;
	AvtobusniQoy(Kelish, 0.f, 0.f);
	Bosqich(EBosqich::Kutish);
}

void ABozorAvtobusIntro::Bosqich(EBosqich B)
{
	Holat = B;
	BosqichVaqt = GetWorld()->GetTimeSeconds();
	UE_LOG(LogBozorAvtobus, Display, TEXT("[Avtobus] bosqich %d (yo'l %.0f sm)"), int32(B), Yol);
}

void ABozorAvtobusIntro::Surat(const TCHAR* Nom)
{
	if (!bTest) return;
	FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots/AvtobusTest") / Nom, false, false);
}

void ABozorAvtobusIntro::AvtobusniQoy(USplineComponent* S, float Masofa, float Dt)
{
	const float L = S->GetSplineLength();
	const float D = FMath::Clamp(Masofa, 0.f, L);
	FVector P = S->GetLocationAtDistanceAlongSpline(D, ESplineCoordinateSpace::World);
	const FVector Dir = S->GetDirectionAtDistanceAlongSpline(D, ESplineCoordinateSpace::World);
	FRotator R = Dir.Rotation();
	R.Pitch = R.Roll = 0.f;
	Avtobus->SetActorLocationAndRotation(P, R, false, nullptr, ETeleportType::TeleportPhysics);
}

// g'ildirak o'z markazi atrofida, ota-komponentning Y o'qi bo'ylab aylanadi. BP g'ildirakni o'zi qo'ysa (har kadr yoki bir marta),
// o'sha joy asl holat deb olinadi - biz faqat aylanishni qo'shamiz
void ABozorAvtobusIntro::GildirakAylantir(float Masofa)
{
	for (FGildirak& G : Gildiraklar)
	{
		UStaticMeshComponent* C = G.C.Get();
		if (!C || !C->GetStaticMesh()) continue;
		const FTransform Hozir = C->GetRelativeTransform();
		if (!G.bBor || !Hozir.Equals(G.Qoyilgan, 0.01f))
		{
			const FBox B = C->GetStaticMesh()->GetBoundingBox();
			G.Asl = Hozir;
			G.Markaz = Hozir.TransformPosition(B.GetCenter());
			G.Radius = FMath::Max(B.GetExtent().Z * Hozir.GetScale3D().Z, 20.f);
			G.bBor = true;
		}
		const float Burchak = -Masofa / G.Radius;
		const FTransform Aylanish = FTransform(-G.Markaz) * FTransform(FQuat(FVector::YAxisVector, Burchak)) * FTransform(G.Markaz);
		G.Qoyilgan = G.Asl * Aylanish;
		C->SetRelativeTransform(G.Qoyilgan);
	}
}

// BP_City_Bus eshiklari: o'z hodisasi Doors(Door #, Open) bilan - ovozi va avtobusning cho'kishi (kneeling) ham BP'da
void ABozorAvtobusIntro::Eshiklar(bool bOch)
{
	UFunction* F = Avtobus ? Avtobus->FindFunction(TEXT("Doors")) : nullptr;
	if (!F)
	{
		UE_LOG(LogBozorAvtobus, Warning, TEXT("[Avtobus] Doors hodisasi topilmadi"));
		return;
	}
	for (int32 Eshik = 0; Eshik < 3; ++Eshik)
	{
		TArray<uint8> Buf;
		Buf.SetNumZeroed(FMath::Max<int32>(F->ParmsSize, 1));
		for (TFieldIterator<FProperty> P(F); P && P->HasAnyPropertyFlags(CPF_Parm); ++P)
		{
			if (FBoolProperty* B = CastField<FBoolProperty>(*P)) B->SetPropertyValue_InContainer(Buf.GetData(), bOch);
			if (FIntProperty* I = CastField<FIntProperty>(*P)) I->SetPropertyValue_InContainer(Buf.GetData(), Eshik);
		}
		Avtobus->ProcessEvent(F, Buf.GetData());
	}
	UE_LOG(LogBozorAvtobus, Display, TEXT("[Avtobus] eshiklar %s"), bOch ? TEXT("ochildi") : TEXT("yopildi"));
}

// o'yinchi avtobus ichida, o'rta eshik yonida turadi va yo'l bo'ylab qaraydi; yurolmaydi, atrofga qaray oladi
bool ABozorAvtobusIntro::OyinchiniOl()
{
	APlayerController* P = UGameplayStatics::GetPlayerController(this, 0);
	ACharacter* C = P ? Cast<ACharacter>(P->GetPawn()) : nullptr;
	if (!C) return false;
	Oyinchi = C;
	Pc = P;
	P->SetIgnoreMoveInput(true);
	C->GetCharacterMovement()->DisableMovement();
	C->GetCharacterMovement()->StopMovementImmediately();
	C->SetActorEnableCollision(false);
	const float Half = C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FTransform Bt = AvtobusTransform();
	const FVector Ichkari = EshikMahalliy - FVector(0.f, IchkariChuqurlik, 0.f) + FVector(0.f, 0.f, PolBalandligi + Half + 2.f);
	C->SetActorLocation(Bt.TransformPosition(Ichkari), false, nullptr, ETeleportType::TeleportPhysics);
	C->AttachToComponent(Avtobus->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
	P->SetControlRotation(FRotator(-4.f, Bt.Rotator().Yaw + 35.f, 0.f));   // oldinga, biroz eshik/deraza tomonga
	if (P->PlayerCameraManager) P->PlayerCameraManager->StartCameraFade(1.f, 0.f, 1.5f, FLinearColor::Black, false, false);
	UE_LOG(LogBozorAvtobus, Display, TEXT("[Avtobus] o'yinchi avtobusda: %s"), *C->GetActorLocation().ToString());
	return true;
}

void ABozorAvtobusIntro::OyinchiniQoyibYubor()
{
	ACharacter* C = Oyinchi.Get();
	if (!C) return;
	UCharacterMovementComponent* Cm = C->GetCharacterMovement();
	Cm->SetMovementMode(MOVE_Walking);
	Cm->MaxFlySpeed = 600.f;
	C->SetActorEnableCollision(true);
	if (APlayerController* P = Pc.Get()) P->ResetIgnoreMoveInput();
	UE_LOG(LogBozorAvtobus, Display, TEXT("[Avtobus] boshqaruv o'yinchiga o'tdi: %s"), *C->GetActorLocation().ToString());
}

void ABozorAvtobusIntro::Tick(float Dt)
{
	Super::Tick(Dt);
	if (!Avtobus) return;
	const double T = GetWorld()->GetTimeSeconds() - BosqichVaqt;

	switch (Holat)
	{
	case EBosqich::Kutish:
		// o'yinchi paydo bo'lishini kutadi (GameMode BeginPlay'dan keyin yaratadi)
		AvtobusniQoy(Kelish, 0.f, Dt);
		if (OyinchiniOl()) Bosqich(EBosqich::Kelish);
		break;

	case EBosqich::Kelish:
	{
		// bekatgacha qolgan yo'lda shu tormoz bilan to'xtay oladigan tezlik
		const float L = Kelish->GetSplineLength();
		const float Qoldi = FMath::Max(L - Yol, 0.f);
		V = FMath::Min(Tezlik, FMath::Sqrt(2.f * Tormoz * Qoldi));
		V = FMath::Max(V, Qoldi > 1.f ? 25.f : 0.f);
		const float Qadam = FMath::Min(V * Dt, Qoldi);
		Yol += Qadam;
		GildirakBurchak += Qadam;
		AvtobusniQoy(Kelish, Yol, Dt);
		GildirakAylantir(GildirakBurchak);
		if (bTest && T > 2.0 && T - Dt <= 2.0) Surat(TEXT("1_yolda.png"));
		if (Qoldi <= 1.f) Bosqich(EBosqich::Toxtadi);
		break;
	}

	case EBosqich::Toxtadi:
		GildirakAylantir(GildirakBurchak);
		if (T > EshikKutish.X)
		{
			Eshiklar(true);
			Bosqich(EBosqich::EshikOchiq);
		}
		break;

	case EBosqich::EshikOchiq:
		GildirakAylantir(GildirakBurchak);
		if (bTest)
		{
			// sinov: ichkaridan, keyin ko'cha narigi tomonidan (avtobus va bekat) surat
			if (T > 0.4 && T - Dt <= 0.4) Surat(TEXT("2_eshik_ochiq.png"));
			if (T > 0.6 && !TestKamera && Pc.IsValid())
			{
				const FVector Joy = AvtobusTransform().TransformPosition(FVector(-700.f, -620.f, 210.f));
				const FVector Qara = AvtobusTransform().TransformPosition(FVector(0.f, 250.f, 130.f));
				TestKamera = GetWorld()->SpawnActor<ACameraActor>(Joy, (Qara - Joy).Rotation());
				Pc->SetViewTarget(TestKamera);
			}
			if (T > 1.3 && T - Dt <= 1.3) Surat(TEXT("2b_tashqi.png"));
		}
		if (T > EshikKutish.Y + (bTest ? 0.8f : 0.f))
		{
			if (TestKamera && Pc.IsValid()) Pc->SetViewTarget(Oyinchi.Get());
			// tushish yo'li: eshik ostonasi (pol sathida) -> yo'lak. Hammasi dunyo koordinatasida
			ACharacter* C = Oyinchi.Get();
			if (!C) { Bosqich(EBosqich::Ketish); break; }
			C->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
			const float Half = C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			const FTransform Bt = AvtobusTransform();
			FVector Yolak = Bt.TransformPosition(EshikMahalliy + FVector(0.f, YolakkaMasofa, 0.f));
			FHitResult Hit;
			FCollisionQueryParams Q(SCENE_QUERY_STAT(BozorAvtobus), true, C);
			Q.AddIgnoredActor(Avtobus);
			const float YerZ = GetWorld()->LineTraceSingleByChannel(Hit, Yolak + FVector(0, 0, 200), Yolak - FVector(0, 0, 300), ECC_Visibility, Q)
				? Hit.ImpactPoint.Z : Bt.GetLocation().Z;
			TushishYoli = {
				Bt.TransformPosition(EshikMahalliy + FVector(0.f, -10.f, PolBalandligi + Half + 2.f)),
				Bt.TransformPosition(EshikMahalliy + FVector(0.f, 45.f, PolBalandligi * 0.5f + Half + 2.f)),
				FVector(Yolak.X, Yolak.Y, YerZ + Half + 3.f),
			};
			UCharacterMovementComponent* Cm = C->GetCharacterMovement();
			Cm->SetMovementMode(MOVE_Flying);   // tortishishsiz: zinadan tushishni yo'l o'zi beradi
			Cm->MaxFlySpeed = TushishTezligi;
			C->MoveIgnoreActorAdd(Avtobus);
			C->SetActorEnableCollision(true);
			TushishQadam = 0;
			Bosqich(EBosqich::Tushish);
		}
		break;

	case EBosqich::Tushish:
	{
		ACharacter* C = Oyinchi.Get();
		APlayerController* P = Pc.Get();
		if (!C || TushishQadam >= TushishYoli.Num() || T > 8.0)
		{
			OyinchiniQoyibYubor();
			Eshiklar(false);
			Bosqich(EBosqich::Ketish);
			break;
		}
		const FVector D = TushishYoli[TushishQadam] - C->GetActorLocation();
		if (D.Size() < 12.f)
		{
			++TushishQadam;
			if (TushishQadam == TushishYoli.Num()) Surat(TEXT("3_bekatda.png"));
			break;
		}
		// AddMovementInput -> CharacterMovement tezlanish beradi, AnimBP yurish animatsiyasini o'ynaydi
		C->AddMovementInput(D.GetSafeNormal(), 1.f, true);
		if (P)
		{
			const FRotator Hozir = P->GetControlRotation();
			FRotator Maqsad = FVector(D.X, D.Y, 0.f).Rotation();
			Maqsad.Pitch = -8.f;
			P->SetControlRotation(FMath::RInterpTo(Hozir, Maqsad, Dt, 3.f));
		}
		break;
	}

	case EBosqich::Ketish:
	{
		// tushgan o'yinchi bozor tomonga burilib qaraydi (birinchi soniyalarda; keyin qarash o'zida)
		if (T < 1.2)
			if (APlayerController* P = Pc.Get())
				if (const ACharacter* C = Oyinchi.Get())
				{
					FRotator Maqsad = (QarashNuqtasi - C->GetActorLocation()).Rotation();
					Maqsad.Pitch = FMath::Clamp(Maqsad.Pitch, -10.f, 10.f);
					P->SetControlRotation(FMath::RInterpTo(P->GetControlRotation(), Maqsad, Dt, 4.f));
				}
		if (bTest && T > 1.2 && T - Dt <= 1.2) Surat(TEXT("3b_bozorga.png"));
		if (T < 1.5)
		{
			GildirakAylantir(GildirakBurchak);
			break;
		}
		if (T - Dt < 1.5) Yol = 0.f, V = 0.f;
		V = FMath::Min(Tezlik, V + Tezlanish * Dt);
		const float Qadam = V * Dt;
		Yol += Qadam;
		GildirakBurchak += Qadam;
		AvtobusniQoy(Ketish, Yol, Dt);
		GildirakAylantir(GildirakBurchak);
		if (bTest && T > 4.0 && T - Dt <= 4.0) Surat(TEXT("4_ketdi.png"));
		if (Yol >= Ketish->GetSplineLength())
		{
			AvtobusniYoqot();
			Bosqich(EBosqich::Tugadi);
		}
		break;
	}

	case EBosqich::Tugadi:
		if (!bTest)
		{
			SetActorTickEnabled(false);
			break;
		}
		// sinov: yo'l o'rtasidan bekatning umumiy ko'rinishi, keyin chiqish
		if (APlayerController* P = Pc.Get())
		{
			if (T < 0.1 && Oyinchi.IsValid())
			{
				const FVector Bekat = Oyinchi->GetActorLocation() + FVector(-150.f, 0.f, 40.f);
				const FVector Joy = Bekat + FVector(620.f, -520.f, 120.f);
				if (TestKamera) TestKamera->Destroy();
				TestKamera = GetWorld()->SpawnActor<ACameraActor>(Joy, (Bekat - Joy).Rotation());
				P->SetViewTarget(TestKamera);
			}
			if (T > 0.8 && T - Dt <= 0.8) Surat(TEXT("5_bekat.png"));
			if (T > 1.5) P->ConsoleCommand(TEXT("quit"));
		}
		break;
	}
}
