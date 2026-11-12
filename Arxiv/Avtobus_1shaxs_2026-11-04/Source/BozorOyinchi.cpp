#include "BozorOyinchi.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputActionValue.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "TwoBoneIK.h"
#include "UnrealClient.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogBozorOyinchi, Log, All);

namespace
{
	// Bosh qahramon: Fab "Cartoon Young Boy Rigged" (Epic skeleti), animatsiyalar eski boladan IK retarget qilingan
	const TCHAR* const BolaMeshYoli = TEXT("/Game/BoshQahramon/YangiBola/SKM_YangiBola.SKM_YangiBola");
	const TCHAR* const BolaAnimYoli = TEXT("/Game/BoshQahramon/YangiBola/Anim/ABP_Unarmed_YangiBola");
	constexpr float MeshBoyi = 201.5f;    // SKM_YangiBola bo'yi, sm
	constexpr float KozNisbati = 0.92f;   // ko'z balandligi / bo'y
}

// ================= qo'llar

void FBozorTabiiyQol::Qoy(USkeletalMeshComponent* Mesh)
{
	const USkeletalMesh* M = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	if (!M) return;
	const FReferenceSkeleton& Ref = M->GetRefSkeleton();
	if (Meshi != M)
	{
		Meshi = M;
		Suyaklar.Reset();
		const int32 Hl = Ref.FindBoneIndex(TEXT("hand_l")), Hr = Ref.FindBoneIndex(TEXT("hand_r"));
		for (int32 i = 1; i < Ref.GetNum(); ++i)
		{
			bool bQol = false;
			for (int32 P = i; P != INDEX_NONE && !bQol; P = Ref.GetParentIndex(P)) bQol = (P == Hl || P == Hr);
			if (bQol) Suyaklar.Add(i);
		}
		// modelda ko'rsatkich barmoq tik turadi (ishora qilayotgandek) - unga o'rta barmoqning bukilishi beriladi
		Mahalliy.Reset();
		for (const int32 I : Suyaklar)
		{
			FTransform T = Ref.GetRefBonePose()[I];
			const FString Nom = Ref.GetBoneName(I).ToString();
			if (Nom.Contains(TEXT("index")))
			{
				const int32 O = Ref.FindBoneIndex(*Nom.Replace(TEXT("index"), TEXT("middle")));
				if (O != INDEX_NONE) T.SetRotation(Ref.GetRefBonePose()[O].GetRotation());
			}
			Mahalliy.Add(T);
		}
	}
	TArray<FTransform>& Cs = Mesh->GetEditableComponentSpaceTransforms();
	for (int32 k = 0; k < Suyaklar.Num(); ++k)
	{
		const int32 I = Suyaklar[k], P = Ref.GetParentIndex(I);
		if (I < Cs.Num() && P != INDEX_NONE) Cs[I] = Mahalliy[k] * Cs[P];
	}
}

void UBozorQolMesh::FinalizeBoneTransform()
{
	TabiiyQol.Qoy(this);
	GavdaniYigish();
	Super::FinalizeBoneTransform();
}

// Kameraga faqat qo'llar ko'rinsin: ikki umrov suyagidan pastdagi qo'llardan boshqa hamma suyak (gavda, oyoq,
// bosh, ko'z, tish, yuz) nuqtaga yig'iladi - joyi o'zgarmaydi, faqat masshtab. Component space'da har suyak
// mustaqil, shuning uchun qo'llar FP warp qo'ygan joyida qoladi
void UBozorQolMesh::GavdaniYigish()
{
	const USkeletalMesh* M = GetSkeletalMeshAsset();
	if (!M) return;
	if (Yigiladigan.IsEmpty() || YigishMeshi != M)
	{
		YigishMeshi = M;
		Yigiladigan.Reset();
		const FReferenceSkeleton& Ref = M->GetRefSkeleton();
		const int32 Cl = Ref.FindBoneIndex(TEXT("clavicle_l")), Cr = Ref.FindBoneIndex(TEXT("clavicle_r"));
		for (int32 i = 1; i < Ref.GetNum(); ++i)
		{
			bool bQol = false;
			for (int32 P = i; P != INDEX_NONE && !bQol; P = Ref.GetParentIndex(P)) bQol = (P == Cl || P == Cr);
			if (!bQol) Yigiladigan.Add(i);
		}
	}
	TArray<FTransform>& Cs = GetEditableComponentSpaceTransforms();
	for (const int32 I : Yigiladigan)
		if (I < Cs.Num()) Cs[I].SetScale3D(FVector(0.001f));
}

// ================= tana

void UBozorTanaMesh::FinalizeBoneTransform()
{
	TabiiyQol.Qoy(this);
	YuzniBoshgaBogla();
	Super::FinalizeBoneTransform();
}

// Ildizdan o'sib chiqqan, lekin pelvis/ik zanjiriga kirmaydigan suyaklar (ARP yuz suyaklari) bosh ortidan ergashadi
void UBozorTanaMesh::YuzniBoshgaBogla()
{
	const USkeletalMesh* M = GetSkeletalMeshAsset();
	if (!M) return;
	if (YuzMeshi != M)
	{
		YuzMeshi = M;
		YuzSuyaklari.Reset();
		BoshgaNisbatan.Reset();
		const FReferenceSkeleton& Ref = M->GetRefSkeleton();
		Bosh = Ref.FindBoneIndex(TEXT("head"));
		const int32 Pelvis = Ref.FindBoneIndex(TEXT("pelvis"));
		if (Bosh == INDEX_NONE || Pelvis == INDEX_NONE) return;
		// ref pozaning component space transformlari
		TArray<FTransform> RefCs;
		RefCs.SetNum(Ref.GetNum());
		for (int32 i = 0; i < Ref.GetNum(); ++i)
		{
			const int32 P = Ref.GetParentIndex(i);
			RefCs[i] = P == INDEX_NONE ? Ref.GetRefBonePose()[i] : Ref.GetRefBonePose()[i] * RefCs[P];
		}
		for (int32 i = 1; i < Ref.GetNum(); ++i)
		{
			bool bTana = false;
			for (int32 P = i; P != INDEX_NONE && !bTana; P = Ref.GetParentIndex(P))
				bTana = (P == Pelvis) || Ref.GetBoneName(P).ToString().StartsWith(TEXT("ik_"));
			if (bTana) continue;
			YuzSuyaklari.Add(i);
			BoshgaNisbatan.Add(RefCs[i].GetRelativeTransform(RefCs[Bosh]));
		}
	}
	if (Bosh == INDEX_NONE) return;
	TArray<FTransform>& Cs = GetEditableComponentSpaceTransforms();
	if (Bosh >= Cs.Num()) return;
	for (int32 k = 0; k < YuzSuyaklari.Num(); ++k)
		if (YuzSuyaklari[k] < Cs.Num()) Cs[YuzSuyaklari[k]] = BoshgaNisbatan[k] * Cs[Bosh];
}

// ================= o'yinchi

ABozorOyinchi::ABozorOyinchi(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UBozorTanaMesh>(ACharacter::MeshComponentName))
{
	PrimaryActorTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> BolaMesh(BolaMeshYoli);
	static ConstructorHelpers::FClassFinder<UAnimInstance> BolaAnim(BolaAnimYoli);
	// Epic First Person shabloni: tana pozasini nusxalaydi + CtrlRig_FPWarp (bola skeleti Manny bilan mos qilingan)
	static ConstructorHelpers::FClassFinder<UAnimInstance> FpAnim(TEXT("/Game/FirstPerson/Anims/ABP_FP_Copy"));
	static ConstructorHelpers::FObjectFinder<UInputMappingContext> Imc(TEXT("/Game/Input/IMC_Default.IMC_Default"));
	static ConstructorHelpers::FObjectFinder<UInputMappingContext> ImcM(TEXT("/Game/Input/IMC_MouseLook.IMC_MouseLook"));
	static ConstructorHelpers::FObjectFinder<UInputAction> Move(TEXT("/Game/Input/Actions/IA_Move.IA_Move"));
	static ConstructorHelpers::FObjectFinder<UInputAction> LookA(TEXT("/Game/Input/Actions/IA_Look.IA_Look"));
	static ConstructorHelpers::FObjectFinder<UInputAction> MouseLook(TEXT("/Game/Input/Actions/IA_MouseLook.IA_MouseLook"));
	static ConstructorHelpers::FObjectFinder<UInputAction> Jump(TEXT("/Game/Input/Actions/IA_Jump.IA_Jump"));
	ImcDefault = Imc.Object;
	ImcMouse = ImcM.Object;
	IaMove = Move.Object;
	IaLook = LookA.Object;
	IaMouseLook = MouseLook.Object;
	IaJump = Jump.Object;

	const float S = Boy / MeshBoyi;
	const float HalfHeight = Boy * 0.5f;
	GetCapsuleComponent()->InitCapsuleSize(Boy * 0.16f, HalfHeight);

	// tana: boshqalar va soya uchun, egasi ko'rmaydi
	USkeletalMeshComponent* Body = GetMesh();
	Body->SetSkeletalMesh(BolaMesh.Object);
	Body->SetAnimInstanceClass(BolaAnim.Class);
	Body->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -HalfHeight), FRotator(0.f, -90.f, 0.f));
	Body->SetRelativeScale3D(FVector(S));
	Body->SetOwnerNoSee(true);
	Body->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;
	// egasi ko'rmasa ham animatsiya hisoblansin - qo'llar shu pozani nusxalaydi
	Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	Kamera = CreateDefaultSubobject<UCameraComponent>(TEXT("Kamera"));
	Kamera->SetupAttachment(GetCapsuleComponent());
	Kamera->SetRelativeLocation(FVector(6.f, 0.f, Boy * KozNisbati - HalfHeight));
	Kamera->bUsePawnControlRotation = true;
	Kamera->SetFieldOfView(90.f);
	Kamera->bEnableFirstPersonFieldOfView = true;
	Kamera->FirstPersonFieldOfView = 70.f;
	Kamera->bEnableFirstPersonScale = true;
	Kamera->FirstPersonScale = 0.6f;

	// qo'llar: tanaga biriktirilgan bola modeli (ABP_FP_Copy pozani ota-komponentdan oladi), faqat egasiga
	Qollar = CreateDefaultSubobject<UBozorQolMesh>(TEXT("Qollar"));
	Qollar->SetupAttachment(Body);
	Qollar->SetSkeletalMesh(BolaMesh.Object);
	Qollar->SetAnimInstanceClass(FpAnim.Succeeded() ? FpAnim.Class : BolaAnim.Class);
	Qollar->SetOnlyOwnerSee(true);
	Qollar->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
	Qollar->SetCastShadow(false);
	Qollar->SetCollisionProfileName(TEXT("NoCollision"));

	bUseControllerRotationYaw = true;
	UCharacterMovementComponent* Cm = GetCharacterMovement();
	Cm->bOrientRotationToMovement = false;
	Cm->MaxWalkSpeed = 220.f;
	Cm->JumpZVelocity = 380.f;
	Cm->BrakingDecelerationFalling = 1500.f;
	Cm->AirControl = 0.4f;

	Hamyon = CreateDefaultSubobject<UBozorHamyon>(TEXT("Hamyon"));
}

void ABozorOyinchi::BeginPlay()
{
	Super::BeginPlay();

	for (const TCHAR* Suyak : { TEXT("thigh_l"), TEXT("thigh_r") })
		Qollar->HideBoneByName(Suyak, PBO_None);
	// ko'z ustidagi soya, ko'z yoshi va shox parda qatlamlari glTF'dan to'q bo'lib keladi va ko'zni butunlay qoplaydi - o'chiriladi
	if (const USkeletalMesh* M = GetMesh()->GetSkeletalMeshAsset())
		if (const FSkeletalMeshRenderData* Rd = M->GetResourceForRendering())
			for (int32 Lod = 0; Lod < Rd->LODRenderData.Num(); ++Lod)
			{
				const TArray<FSkelMeshRenderSection>& Bolim = Rd->LODRenderData[Lod].RenderSections;
				for (int32 B = 0; B < Bolim.Num(); ++B)
				{
					const int32 Mi = Bolim[B].MaterialIndex;
					const FString Nom = M->GetMaterials().IsValidIndex(Mi) ? M->GetMaterials()[Mi].MaterialSlotName.ToString() : FString();
					if (Nom.Contains(TEXT("Occlusion")) || Nom.Contains(TEXT("Tearline")) || Nom.Contains(TEXT("Cornea")))
						GetMesh()->ShowMaterialSection(Mi, B, false, Lod);
				}
			}
	// sozlash uchun: -KozX=8 -KozZ=7.5 -QolFov=70
	const TCHAR* Cmd = FCommandLine::Get();
	FParse::Value(Cmd, TEXT("KozX="), KozSiljish.X);
	FParse::Value(Cmd, TEXT("KozZ="), KozSiljish.Z);
	FParse::Value(Cmd, TEXT("QolFov="), Kamera->FirstPersonFieldOfView);
	UE_LOG(LogBozorOyinchi, Display, TEXT("Qollar animatsiyasi: %s"), *GetNameSafe(Qollar->GetAnimClass()));

	YangiSotuvchi();

	bTest = FParse::Param(FCommandLine::Get(), TEXT("SotuvTest"));
	// tekshiruv: -RefPoza - tana animatsiyasiz, asl pozada
	if (FParse::Param(FCommandLine::Get(), TEXT("RefPoza"))) GetMesh()->SetAnimInstanceClass(nullptr);
	TestVaqt = GetWorld()->GetTimeSeconds();
}

void ABozorOyinchi::PawnClientRestart()
{
	Super::PawnClientRestart();
	if (const APlayerController* Pc = Cast<APlayerController>(GetController()))
		if (UEnhancedInputLocalPlayerSubsystem* Sub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Pc->GetLocalPlayer()))
		{
			Sub->ClearAllMappings();
			if (ImcDefault) Sub->AddMappingContext(ImcDefault, 0);
			if (ImcMouse) Sub->AddMappingContext(ImcMouse, 0);
		}
}

void ABozorOyinchi::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	if (UEnhancedInputComponent* In = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (IaJump)
		{
			In->BindAction(IaJump, ETriggerEvent::Started, this, &ACharacter::Jump);
			In->BindAction(IaJump, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
		}
		if (IaMove) In->BindAction(IaMove, ETriggerEvent::Triggered, this, &ABozorOyinchi::Move);
		if (IaLook) In->BindAction(IaLook, ETriggerEvent::Triggered, this, &ABozorOyinchi::Look);
		if (IaMouseLook) In->BindAction(IaMouseLook, ETriggerEvent::Triggered, this, &ABozorOyinchi::Look);
	}
}

void ABozorOyinchi::Move(const FInputActionValue& Value)
{
	const FVector2D V = Value.Get<FVector2D>();
	AddMovementInput(GetActorForwardVector(), V.Y);
	AddMovementInput(GetActorRightVector(), V.X);
}

void ABozorOyinchi::Look(const FInputActionValue& Value)
{
	const FVector2D V = Value.Get<FVector2D>();
	AddControllerYawInput(V.X);
	AddControllerPitchInput(V.Y);
}

void ABozorOyinchi::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bTest) SotuvTestQadami();
	KameraJoyi();
}

// Shablondagi kabi kamera boshda: FP warp moslagan bosh suyagi joyi + ko'z siljishi (gavda yo'nalishida).
// Bosh suyagi masshtabi yig'ilgani uchun socket'ga biriktirilmaydi - joyi har kadr olinadi
void ABozorOyinchi::KameraJoyi()
{
	const int32 Bosh = Qollar ? Qollar->GetBoneIndex(TEXT("head")) : INDEX_NONE;
	if (Bosh == INDEX_NONE) return;
	const FRotator Yaw(0.f, GetControlRotation().Yaw, 0.f);
	Kamera->SetWorldLocation(Qollar->GetBoneLocation(TEXT("head")) + Yaw.RotateVector(KozSiljish * (Boy / MeshBoyi)));
}

// ================= sotuv (chat oynasi bo'lguncha konsol orqali)

void ABozorOyinchi::Chiqar(const FString& Kim, const FString& Matn, const FColor& Rang)
{
	const FString Qator = FString::Printf(TEXT("%s: %s"), *Kim, *Matn);
	UE_LOG(LogBozorOyinchi, Display, TEXT("%s"), *Qator);
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 60.f, Rang, Qator, false, FVector2D(1.3f));
	FFileHelper::SaveStringToFile(Qator + LINE_TERMINATOR, *(FPaths::ProjectSavedDir() / TEXT("Sotuv_suhbat.txt")),
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
}

void ABozorOyinchi::YangiSotuvchi()
{
	Suhbat = NewObject<UBozorSuhbat>(this);
	Suhbat->Boshla(Mahsulot, Hamyon, NewObject<UBozorTayyorJavob>(this));
}

void ABozorOyinchi::Gapir(const FString& Matn)
{
	if (!Suhbat || Matn.TrimStartAndEnd().IsEmpty()) return;
	Chiqar(TEXT("Siz"), Matn, FColor::White);
	Chiqar(TEXT("Sotuvchi"), Suhbat->Yoz(Matn), FColor::Yellow);
	HamyonKor();
}

void ABozorOyinchi::HamyonKor()
{
	Chiqar(TEXT("Hamyon"), FString::Printf(TEXT("%s so'm | savat: %s"), *BozorSotuv::Som(Hamyon->Pul),
		Hamyon->Savat.Num() ? *FString::Join(Hamyon->Savat, TEXT(", ")) : TEXT("bo'sh")), FColor::Cyan);
}

// -SotuvTest: 3 s yuradi (qo'llar ko'rinishini suratga oladi), keyin tayyor suhbat va yana surat, so'ng chiqadi
void ABozorOyinchi::SotuvTestQadami()
{
	static const TCHAR* Gaplar[] = {
		TEXT("Pomidor necha pul?"),
		TEXT("Assalomu alaykum!"),
		TEXT("Qimmat ekan, arzonroq qiling"),
		TEXT("Yana ozgina tushiring"),
		TEXT("2 kilo torting"),
		TEXT("20 ming beraman"),
		TEXT("25 000 so'm oling"),
		TEXT("Rahmat, xayr!"),
	};
	const double T = GetWorld()->GetTimeSeconds() - TestVaqt;
	APlayerController* Pc = Cast<APlayerController>(GetController());
	const FString Dir = FPaths::ProjectSavedDir() / TEXT("Screenshots/SotuvTest/");

	if (TestQadam == 0 && T > 2.0)
	{
		if (Pc)
		{
			Pc->SetIgnoreLookInput(true);
			Pc->ConsoleCommand(TEXT("r.MotionBlurQuality 0"));
		}
		IFileManager::Get().Delete(*(FPaths::ProjectSavedDir() / TEXT("Sotuv_suhbat.txt")));
		++TestQadam;
	}
	// burchak har kadrda ushlab turiladi: 1-3 qadam - oldinga qarab yurish, keyin pastga qarash
	if (Pc && TestQadam >= 1) Pc->SetControlRotation(FRotator(TestQadam <= 3 ? -5.f : -70.f, GetControlRotation().Yaw, 0.f));
	if (TestQadam >= 1 && TestQadam <= 3) AddMovementInput(GetActorForwardVector(), 1.f);
	// yurish paytida qadamning turli fazalarida 6 ta surat
	static int32 Surat = 0;
	if (TestQadam == 1 && T > 4.0 + Surat * 0.17)
	{
		FScreenshotRequest::RequestScreenshot(Dir + FString::Printf(TEXT("1_yurish_%d.png"), Surat), false, false);
		if (++Surat == 6) ++TestQadam;
	}
	if (TestQadam == 2) ++TestQadam;
	if (TestQadam == 3 && T > 5.5) ++TestQadam;
	if (TestQadam == 4 && T > 6.5) { FScreenshotRequest::RequestScreenshot(Dir + TEXT("3_pastga.png"), false, false); ++TestQadam; }
	const int32 Gap = TestQadam - 5;
	if (Gap >= 0 && Gap < UE_ARRAY_COUNT(Gaplar) && T > 7.0 + Gap * 0.6) { Gapir(Gaplar[Gap]); ++TestQadam; }
	if (Gap == UE_ARRAY_COUNT(Gaplar) && T > 7.0 + Gap * 0.6 + 1.0) { FScreenshotRequest::RequestScreenshot(Dir + TEXT("4_suhbat.png"), true, false); ++TestQadam; }
	// personajni tashqaridan ko'rish: oldiga kamera qo'yiladi, bola unga qarab yuradi
	const int32 N = UE_ARRAY_COUNT(Gaplar);
	if (Gap == N + 1 && Pc)
	{
		const FRotator Yaw(0.f, GetActorRotation().Yaw, 0.f);
		const FVector Joy = GetActorLocation() + Yaw.Vector() * 320.f + FVector(0.f, 0.f, 10.f);
		if (ACameraActor* Cam = GetWorld()->SpawnActor<ACameraActor>(Joy, (GetActorLocation() - Joy).Rotation()))
			Pc->SetViewTarget(Cam);
		TestVaqt2 = T;
		++TestQadam;
	}
	if (Gap == N + 2)
	{
		Pc->SetControlRotation(FRotator(0.f, GetControlRotation().Yaw, 0.f));
		AddMovementInput(GetActorForwardVector(), 0.6f);
		if (T > TestVaqt2 + 1.2) { FScreenshotRequest::RequestScreenshot(Dir + TEXT("5_tashqi_yurish.png"), false, false); ++TestQadam; }
	}
	if (Gap == N + 3 && T > TestVaqt2 + 3.0) { FScreenshotRequest::RequestScreenshot(Dir + TEXT("6_tashqi_turish.png"), false, false); ++TestQadam; }
	if (Gap == N + 4 && T > TestVaqt2 + 4.0 && Pc) { Pc->ConsoleCommand(TEXT("quit")); ++TestQadam; }
}

// ================= GameMode

ABozorGameMode::ABozorGameMode()
{
	DefaultPawnClass = ABozorOyinchi::StaticClass();
}
