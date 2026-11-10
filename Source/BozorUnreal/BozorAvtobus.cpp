#include "BozorAvtobus.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "InputCoreTypes.h"
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

// Matn va materiallar har safar yangilanadi. Qismlarning joyi esa faqat bAvtoJoylash bo'lsa (birinchi marta) soyabon o'lchamiga
// qarab hisoblanadi - keyin muharrirda qo'lda surilgan joyi saqlanadi (OnConstruction har o'zgarishda chaqiriladi)
void ABozorBekat::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	Ustun->SetStaticMesh(Belgi->GetStaticMesh());
	if (UstunMaterial) Ustun->SetMaterial(0, UstunMaterial);
	if (KokMaterial) Belgi->SetMaterial(0, KokMaterial);
	if (QoraMaterial) TabloQuti->SetMaterial(0, QoraMaterial);
	const FString BelgiYozuv = FString::Printf(TEXT("AVTOBUS\nBEKATI\n%s"), *Yonalishlar);
	BelgiMatn->SetText(FText::FromString(BelgiYozuv));
	BelgiOrqa->SetText(FText::FromString(BelgiYozuv));
	NomMatn->SetText(FText::FromString(BekatNomi));
	TabloMatn->SetText(FText::FromString(Tablo));

	if (!bAvtoJoylash) return;
	bAvtoJoylash = false;

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
	Ustun->SetRelativeScale3D(FVector(0.06f, 0.06f, UstunBoyi / 100.f));
	Ustun->SetRelativeLocation(UstunJoy + FVector(0.f, 0.f, UstunBoyi * 0.5f));
	Belgi->SetRelativeLocation(FVector(UstunJoy.X, UstunJoy.Y, UstunBoyi - 40.f));
	Belgi->SetRelativeRotation(FRotator::ZeroRotator);
	Belgi->SetRelativeScale3D(FVector(0.6f, 0.03f, 0.75f));

	// matnlar kubning ikki yuzida (kub 100 sm, masshtab ota-komponentdan keladi - matn uchun teskarisi)
	const FVector Teskari(1.f / 0.6f, 1.f / 0.03f, 1.f / 0.75f);
	for (UTextRenderComponent* T : { BelgiMatn.Get(), BelgiOrqa.Get() })
	{
		const bool bOrqa = T == BelgiOrqa;
		T->SetRelativeScale3D(Teskari);
		T->SetRelativeLocation(FVector(0.f, bOrqa ? 52.f : -52.f, 8.f));
		T->SetRelativeRotation(FRotator(0.f, bOrqa ? 90.f : -90.f, 0.f));
	}
	NomMatn->SetRelativeScale3D(Teskari);
	NomMatn->SetRelativeLocation(FVector(0.f, -52.f, -38.f));
	NomMatn->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));

	// elektron tablo: tom ostidagi bo'ylama markaziy to'singa tegib osilgan (to'sin tagi ~ B.Max.Z - 11 sm), yo'lga qarab.
	// Oldin tom ostida 40 sm pastda hech narsaga ulanmay havoda turardi
	TabloQuti->SetRelativeLocation(FVector(B.GetCenter().X + 7.f, B.GetCenter().Y, B.Max.Z - 27.f));
	TabloQuti->SetRelativeRotation(FRotator::ZeroRotator);
	TabloQuti->SetRelativeScale3D(FVector(0.06f, 1.1f, 0.32f));
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
	if (FParse::Param(Cmd, TEXT("Avtobus1Shaxs"))) bKinoKadr = false;
	if (!Avtobus || KelishNuqtalari.Num() < 2 || bYoq)
	{
		AvtobusniYoqot();
		UE_LOG(LogBozorAvtobus, Display, TEXT("[Avtobus] intro yo'q (avtobus=%s, nuqta=%d)"), *GetNameSafe(Avtobus), KelishNuqtalari.Num());
		Holat = EBosqich::Tugadi;
		SetActorTickEnabled(false);
		return;
	}

	bOynaldi = true;

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
			G.bOld = Nom.StartsWith(TEXT("SM_Wheel_F"));
			Gildiraklar.Add(G);
		}
	}

	// Toshkent raqami: plita materiali almashtiriladi (raqam teksturada), BP'ning Yevropa uslubidagi matnlari yashiriladi
	if (RaqamMaterial)
	{
		for (UStaticMeshComponent* M : Meshes)
		{
			if (GetNameSafe(M->GetStaticMesh()) != TEXT("SM_Reg_Plate")) continue;
			for (int32 I = 0; I < M->GetNumMaterials(); ++I)
				if (GetNameSafe(M->GetMaterial(I)).StartsWith(TEXT("M_Reg_Plate"))) M->SetMaterial(I, RaqamMaterial);
		}
		TArray<UTextRenderComponent*> Matnlar;
		Avtobus->GetComponents(Matnlar);
		for (UTextRenderComponent* T : Matnlar)
			if (T->GetName().Contains(TEXT("Plate_Text")) || T->GetName().Contains(TEXT("Plate_Country"))) T->SetHiddenInGame(true);
	}

	// BP'ning Chaos harakat komponenti avtobusni o'zi qo'ymasin (fizika o'chirilgan, yo'lni biz beramiz)
	for (UActorComponent* C : Avtobus->GetComponents())
		if (C && C->GetClass()->GetName().Contains(TEXT("VehicleMovement")))
		{
			C->SetComponentTickEnabled(false);
			UE_LOG(LogBozorAvtobus, Display, TEXT("[Avtobus] %s tick o'chirildi"), *C->GetName());
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

// Avtobus markazi splaynda; burchagi esa splayndagi orqa va old o'q nuqtalari orasidagi yo'nalish (haqiqiy avtobus kabi -
// splayn tangentining mayda to'lqinlari avtobusni u yoq-bu yoqqa burmaydi), o'zgarish tezligi cheklangan.
// Old g'ildiraklar old o'q nuqtasidagi yo'nalishga qarab buriladi. Dt=0 - joyiga darhol qo'yish
void ABozorAvtobusIntro::AvtobusniQoy(USplineComponent* S, float Masofa, float Dt)
{
	const float L = S->GetSplineLength();
	const float D = FMath::Clamp(Masofa, 0.f, L);
	const FVector P = S->GetLocationAtDistanceAlongSpline(D, ESplineCoordinateSpace::World);
	const float Yarim = OqOraligi * 0.5f;
	const FVector Orqa = S->GetLocationAtDistanceAlongSpline(FMath::Max(D - Yarim, 0.f), ESplineCoordinateSpace::World);
	const FVector Old = S->GetLocationAtDistanceAlongSpline(FMath::Min(D + Yarim, L), ESplineCoordinateSpace::World);
	FVector Dir = Old - Orqa;
	Dir.Z = 0.f;
	if (Dir.SizeSquared() < 1.f) Dir = S->GetDirectionAtDistanceAlongSpline(D, ESplineCoordinateSpace::World);
	const float Maqsad = Dir.Rotation().Yaw;

	if (!bYawBor || Dt <= 0.f)
	{
		Yaw = Maqsad;
		bYawBor = true;
	}
	else
	{
		const float Farq = FMath::FindDeltaAngleDegrees(Yaw, Maqsad);
		const float Qadam = FMath::Clamp(Farq, -MaxBurilishTezligi * Dt, MaxBurilishTezligi * Dt);
		Yaw = FRotator::NormalizeAxis(Yaw + Qadam);
		if (bTest) MaxYawQadam = FMath::Max(MaxYawQadam, FMath::Abs(Qadam));
	}

	// old g'ildirak: old o'q nuqtasidagi splayn yo'nalishi va avtobus burchagi farqi
	const float OldYaw = S->GetDirectionAtDistanceAlongSpline(FMath::Min(D + Yarim, L), ESplineCoordinateSpace::World).Rotation().Yaw;
	Burilish = FMath::Clamp(FMath::FindDeltaAngleDegrees(Yaw, OldYaw), -30.f, 30.f);

	Avtobus->SetActorLocationAndRotation(P, FRotator(0.f, Yaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
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
		// old g'ildirak avval o'z o'qida aylanadi, keyin tik o'q atrofida buriladi
		const FQuat Rul = G.bOld ? FQuat(FVector::ZAxisVector, FMath::DegreesToRadians(Burilish)) : FQuat::Identity;
		const FTransform Aylanish = FTransform(-G.Markaz) * FTransform(FQuat(FVector::YAxisVector, Burchak)) * FTransform(Rul) * FTransform(G.Markaz);
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
	// eshiklar soni = MeshArray_Doors / 2 (har eshikda 2 tavaqa). Bo'g'imsiz avtobusda 2 ta eshik bor;
	// mavjud bo'lmagan eshikni ochish BP'da "Accessed None ... Array_Get_Item" xatolarini beradi
	int32 Soni = 2;
	if (FArrayProperty* AP = FindFProperty<FArrayProperty>(Avtobus->GetClass(), TEXT("MeshArray_Doors")))
	{
		FScriptArrayHelper H(AP, AP->ContainerPtrToValuePtr<void>(Avtobus));
		Soni = H.Num() / 2;
	}
	for (int32 Eshik = 0; Eshik < Soni; ++Eshik)
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
	if (bKinoKadr) P->SetIgnoreLookInput(true);
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

// bBoshqaruv=false: bola yo'lakda yura oladigan holatga o'tadi, lekin kino tugaguncha klaviatura/sichqonchaga bo'ysunmaydi
void ABozorAvtobusIntro::OyinchiniQoyibYubor(bool bBoshqaruv)
{
	ACharacter* C = Oyinchi.Get();
	if (!C) return;
	UCharacterMovementComponent* Cm = C->GetCharacterMovement();
	Cm->SetMovementMode(MOVE_Walking);
	Cm->MaxFlySpeed = 600.f;
	C->SetActorEnableCollision(true);
	if (!bBoshqaruv) return;
	if (APlayerController* P = Pc.Get())
	{
		P->ResetIgnoreMoveInput();
		P->ResetIgnoreLookInput();
	}
	bBoshqaruvBerildi = true;
	UE_LOG(LogBozorAvtobus, Display, TEXT("[Avtobus] boshqaruv o'yinchiga o'tdi: %s"), *C->GetActorLocation().ToString());
}

FVector ABozorAvtobusIntro::TushishOxiri(ACharacter* C) const
{
	const float Half = C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FTransform Bt = AvtobusTransform();
	const FVector Yolak = Bt.TransformPosition(EshikMahalliy + FVector(0.f, YolakkaMasofa, 0.f));
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(BozorAvtobus), true, C);
	Q.AddIgnoredActor(Avtobus);
	const float YerZ = GetWorld()->LineTraceSingleByChannel(Hit, Yolak + FVector(0, 0, 200), Yolak - FVector(0, 0, 300), ECC_Visibility, Q)
		? Hit.ImpactPoint.Z : Bt.GetLocation().Z;
	return FVector(Yolak.X, Yolak.Y, YerZ + Half + 3.f);
}

// ------------------------------------------------------------ kino kadrlar
// Kameralar avtobusning bekatdagi joyiga nisbatan (X - avtobus oldi, Y - o'ng tomoni, ya'ni eshiklar va yo'lak)
void ABozorAvtobusIntro::Kadr(int32 N)
{
	APlayerController* P = Pc.Get();
	if (!P) return;
	if (!KinoKamera)
	{
		KinoKamera = GetWorld()->SpawnActor<ACameraActor>(GetActorLocation(), FRotator::ZeroRotator);
		KinoKamera->GetCameraComponent()->SetFieldOfView(KinoFOV);
		KinoKamera->GetCameraComponent()->SetConstraintAspectRatio(false);
	}
	KinoKadrN = N;
	FVector Joy, Qara;
	if (N == 1)
	{
		// yo'l chetida, bekatdan oldinda, past: avtobus kameraga qarab kelib, bekatda to'xtaydi
		const float L = Kelish->GetSplineLength();
		const FRotator R(0.f, Kelish->GetDirectionAtDistanceAlongSpline(L, ESplineCoordinateSpace::World).Rotation().Yaw, 0.f);
		const FTransform Toxtash(R, Kelish->GetLocationAtDistanceAlongSpline(L, ESplineCoordinateSpace::World));
		Joy = Toxtash.TransformPosition(FVector(1100.f, 330.f, 110.f));
		Qara = Avtobus->GetActorLocation() + FVector(0.f, 0.f, 150.f);
	}
	else if (N == 2)
	{
		// yo'lakdan (bordyurga yaqin - soyabon reklama paneli to'smasin), avtobusning orqa tomonidan o'rta eshikka
		const FTransform Bt = AvtobusTransform();
		Joy = Bt.TransformPosition(EshikMahalliy + FVector(-450.f, 220.f, 145.f));
		Qara = Bt.TransformPosition(EshikMahalliy + FVector(0.f, 20.f, 115.f));
	}
	else
	{
		// yo'lakdan, avtobus orqasidan (soyabon reklama panelidan narida), bola va avtobus ortidagi bozorga qiyalab:
		// avtobus oldinga jo'naydi va ortidan bozor ochiladi
		const FTransform Bt = AvtobusTransform();
		Joy = Bt.TransformPosition(FVector(-520.f, 420.f, 170.f));
		Qara = Bt.TransformPosition(FVector(400.f, -500.f, 120.f));
	}
	KinoKamera->SetActorLocationAndRotation(Joy, (Qara - Joy).Rotation());
	P->SetViewTarget(KinoKamera);
	UE_LOG(LogBozorAvtobus, Display, TEXT("[Avtobus] kadr %d: %s"), N, *Joy.ToString());
}

void ABozorAvtobusIntro::KinoTick(float Dt)
{
	if (!KinoKamera) return;
	if (KinoKadrN == 1)
	{
		// kelayotgan avtobusni kuzatadi
		const FVector Qara = Avtobus->GetActorLocation() + FVector(0.f, 0.f, 150.f);
		const FRotator Maqsad = (Qara - KinoKamera->GetActorLocation()).Rotation();
		KinoKamera->SetActorRotation(FMath::RInterpTo(KinoKamera->GetActorRotation(), Maqsad, Dt, 4.f));
	}
	else if (KinoKadrN == 3 && !bKozgaOtdi)
	{
		// sekin oldinga suriladi
		KinoKamera->AddActorWorldOffset(KinoKamera->GetActorForwardVector() * 14.f * Dt);
	}
}

// Bo'sh joy / Enter: avtobus bekatda, bola yo'lakda, kamera ko'zida; avtobus darhol jo'naydi
void ABozorAvtobusIntro::Otkazish()
{
	ACharacter* C = Oyinchi.Get();
	APlayerController* P = Pc.Get();
	if (!C || !P || bBoshqaruvBerildi) return;
	if (Holat < EBosqich::Ketish)
	{
		if (Holat < EBosqich::Tushish)
		{
			Yol = Kelish->GetSplineLength();
			AvtobusniQoy(Kelish, Yol, 0.f);
			C->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		}
		C->SetActorLocation(TushishOxiri(C), false, nullptr, ETeleportType::TeleportPhysics);
		C->MoveIgnoreActorAdd(Avtobus);
		if (Holat >= EBosqich::EshikOchiq) Eshiklar(false);
		Bosqich(EBosqich::Ketish);
		BosqichVaqt -= 1.5;   // to'xtab turmasdan jo'naydi
	}
	OyinchiniQoyibYubor(true);
	FRotator R = (QarashNuqtasi - C->GetActorLocation()).Rotation();
	R.Pitch = FMath::Clamp(R.Pitch, -10.f, 10.f);
	P->SetControlRotation(R);
	P->SetViewTarget(C);
	if (P->PlayerCameraManager) P->PlayerCameraManager->StartCameraFade(1.f, 0.f, 0.6f, FLinearColor::Black, false, false);
	if (KinoKamera) KinoKamera->Destroy();
	KinoKamera = nullptr;
	KinoKadrN = 0;
	bKozgaOtdi = true;
	UE_LOG(LogBozorAvtobus, Display, TEXT("[Avtobus] intro o'tkazib yuborildi"));
}

void ABozorAvtobusIntro::Tick(float Dt)
{
	Super::Tick(Dt);
	if (!Avtobus) return;
	if (bKinoKadr && KinoKadrN > 0 && !bBoshqaruvBerildi)
		if (APlayerController* P = Pc.Get())
			if (P->WasInputKeyJustPressed(EKeys::SpaceBar) || P->WasInputKeyJustPressed(EKeys::Enter))
				Otkazish();
	KinoTick(Dt);
	const double T = GetWorld()->GetTimeSeconds() - BosqichVaqt;

	switch (Holat)
	{
	case EBosqich::Kutish:
		// o'yinchi paydo bo'lishini kutadi (GameMode BeginPlay'dan keyin yaratadi)
		AvtobusniQoy(Kelish, 0.f, Dt);
		if (OyinchiniOl())
		{
			Bosqich(EBosqich::Kelish);
			if (bKinoKadr) Kadr(1);
		}
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
		if (Qoldi <= 1.f)
		{
			Bosqich(EBosqich::Toxtadi);
			if (bKinoKadr) Kadr(2);
		}
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
		if (bTest && T > 0.4 && T - Dt <= 0.4) Surat(TEXT("2_eshik_ochiq.png"));
		if (bTest && !bKinoKadr)
		{
			// sinov: ichkaridan, keyin ko'cha narigi tomonidan (avtobus va bekat) surat
			if (T > 0.6 && !TestKamera && Pc.IsValid())
			{
				const FVector Joy = AvtobusTransform().TransformPosition(FVector(-700.f, -620.f, 210.f));
				const FVector Qara = AvtobusTransform().TransformPosition(FVector(0.f, 250.f, 130.f));
				TestKamera = GetWorld()->SpawnActor<ACameraActor>(Joy, (Qara - Joy).Rotation());
				Pc->SetViewTarget(TestKamera);
			}
			if (T > 1.3 && T - Dt <= 1.3) Surat(TEXT("2b_tashqi.png"));
		}
		if (T > EshikKutish.Y + (bTest && !bKinoKadr ? 0.8f : 0.f))
		{
			if (TestKamera && Pc.IsValid()) Pc->SetViewTarget(Oyinchi.Get());
			// tushish yo'li: eshik ostonasi (pol sathida) -> yo'lak. Hammasi dunyo koordinatasida
			ACharacter* C = Oyinchi.Get();
			if (!C) { Bosqich(EBosqich::Ketish); break; }
			C->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
			const float Half = C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			const FTransform Bt = AvtobusTransform();
			TushishYoli = {
				Bt.TransformPosition(EshikMahalliy + FVector(0.f, -10.f, PolBalandligi + Half + 2.f)),
				Bt.TransformPosition(EshikMahalliy + FVector(0.f, 45.f, PolBalandligi * 0.5f + Half + 2.f)),
				TushishOxiri(C),
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
			OyinchiniQoyibYubor(!bKinoKadr);
			Eshiklar(false);
			Bosqich(EBosqich::Ketish);
			if (bKinoKadr) Kadr(3);
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
		// kino: avtobus uzoqlashgach kamera bola ko'ziga o'tadi, keyin boshqaruv beriladi
		if (bKinoKadr && !bBoshqaruvBerildi)
		{
			APlayerController* P = Pc.Get();
			if (!bKozgaOtdi && T >= KozgaOtish.X && P)
			{
				P->SetViewTargetWithBlend(Oyinchi.Get(), KozgaOtish.Y, VTBlend_Cubic);
				bKozgaOtdi = true;
			}
			if (T >= KozgaOtish.X + KozgaOtish.Y)
			{
				OyinchiniQoyibYubor(true);
				if (KinoKamera) KinoKamera->Destroy();
				KinoKamera = nullptr;
				KinoKadrN = 0;
				Surat(TEXT("4b_kozda.png"));
			}
		}
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
			if (bTest) UE_LOG(LogBozorAvtobus, Display, TEXT("[Avtobus] eng katta burilish qadami: %.2f gradus/kadr"), MaxYawQadam);
			AvtobusniYoqot();
			Bosqich(EBosqich::Tugadi);
		}
		break;
	}

	case EBosqich::Tugadi:
		if (bKinoKadr && !bBoshqaruvBerildi) Otkazish();
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
