#include "BozorOyinchi.h"
#include "BozorAiMiya.h"
#include "BozorNavoiy.h"
#include "Sound/SoundWave.h"
#include "BozorHud.h"
#include "BozorNpc.h"
#include "BozorOvoz.h"
#include "BozorQuloq.h"
#include "BozorSozlamalar.h"
#include "BozorUI.h"

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
#include "Kismet/GameplayStatics.h"
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
	// yurish animatsiyasi bola masshtabida (0.66) aynan 198 sm/s qadam tashlaydi - tezlik shunga teng bo'lmasa oyoq sirpanadi
	// (BS_Idle_Walk_Run_YangiBola1 namunalari ham 198/396 ga moslangan). Tezlanish yumshoq - bola birdan to'xtab/yurib ketmaydi
	Cm->MaxWalkSpeed = 198.f;
	Cm->MaxAcceleration = 1000.f;
	Cm->BrakingDecelerationWalking = 1200.f;
	Cm->JumpZVelocity = 380.f;
	Cm->BrakingDecelerationFalling = 1500.f;
	Cm->AirControl = 0.4f;

	Hamyon = CreateDefaultSubobject<UBozorHamyon>(TEXT("Hamyon"));
	Quloq = CreateDefaultSubobject<UBozorQuloq>(TEXT("Quloq"));
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

	bTest =FParse::Param(FCommandLine::Get(), TEXT("SotuvTest"));
	bBot = FParse::Param(FCommandLine::Get(), TEXT("DarajaTest"));
	// daraja: pul, ro'yxat, eslatma
	if (const UBozorSotuvTizimi* Tz = UBozorSotuvTizimi::Get(this))
		if (const FBozorDaraja* D = Tz->GetDaraja())
		{
			Hamyon->Boshla(*D);
			Chiqar(TEXT("Daraja"), FString::Printf(TEXT("%d - %s | pul %s (%s) | ro'yxat %d"), D->Raqam, *D->Nomi,
				*BozorSotuv::Som(Hamyon->Pul), *Hamyon->Tarkibi(), Hamyon->Royxat.Num()), FColor::White);
		}
	// tekshiruv: -RefPoza - tana animatsiyasiz, asl pozada
	if (FParse::Param(FCommandLine::Get(), TEXT("RefPoza"))) GetMesh()->SetAnimInstanceClass(nullptr);
	TestVaqt = GetWorld()->GetTimeSeconds();

	// mikrofon matni: sotuvchiga gap sifatida (tugmadagi gap bilan bir xil yo'l)
	Quloq->NatijaKeldi = [this](const FString& Matn)
	{
		if (Matn.IsEmpty())
		{
			Xabar = TEXT("Eshitmadim - V ni bosib turib, aniqroq gapiring");
			XabarVaqti = GetWorld()->GetTimeSeconds();
			return;
		}
		Xabar.Reset();
		Gapir(Matn);
	};
}

void ABozorOyinchi::VBosildi()
{
	if (!bSuhbatda && !SuhbatniBoshla())
	{
		if (Xabar.IsEmpty() || GetWorld()->GetTimeSeconds() - XabarVaqti > 3.0) Xabar = TEXT("Gapirish uchun sotuvchi oldiga borib, unga qarang");
		XabarVaqti = GetWorld()->GetTimeSeconds();
		return;
	}
	if (!Quloq->Boshla())
	{
		Xabar = Quloq->GetXato();
		XabarVaqti = GetWorld()->GetTimeSeconds();
	}
}

void ABozorOyinchi::QuloqSinov(const FString& Fayl)
{
	if (!bSuhbatda && !SuhbatniBoshla())
	{
		Chiqar(TEXT("Bozor"), TEXT("Yaqin atrofda sotuvchi yo'q"), FColor::Red, true);
		return;
	}
	Quloq->Fayldan(Fayl);
}

void ABozorOyinchi::VQoyildi()
{
	if (!Quloq->Tinglayapti()) return;
	Quloq->Tugat();
	if (Quloq->Tushunyapti())
	{
		Xabar = TEXT("...");
		XabarVaqti = GetWorld()->GetTimeSeconds();
	}
}

void ABozorOyinchi::PawnClientRestart()
{
	Super::PawnClientRestart();
	KirishniYangila();
}

void ABozorOyinchi::KirishniYangila()
{
	if (const APlayerController* Pc = Cast<APlayerController>(GetController()))
		if (UEnhancedInputLocalPlayerSubsystem* Sub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Pc->GetLocalPlayer()))
		{
			Sub->ClearAllMappings();
			// o'yinchi sozlamalarda W/A/S/D/Probel ni o'zgartirgan bo'lsa - IMC nusxasi
			if (ImcDefault) Sub->AddMappingContext(UBozorSozlamalar::Get()->MoslashganImc(ImcDefault), 0);
			if (ImcMouse) Sub->AddMappingContext(ImcMouse, 0);
		}
}

void ABozorOyinchi::TugmaBosildi(FKey Tugma)
{
	const UBozorSozlamalar* Sz = UBozorSozlamalar::Get();
	if (Tugma == EKeys::Escape)
	{
		if (UBozorUITizimi* Ui = UBozorUITizimi::Get(this)) Ui->PauzaniAlmashtir(Cast<APlayerController>(GetController()));
		return;
	}
	if (Tugma == Sz->Tugma(TEXT("Gaplashish"))) EBosildi();
	if (Tugma == Sz->Tugma(TEXT("Qaytadan"))) RBosildi();
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
	// E (Gaplashish), R (Qaytadan) sozlamalarda o'zgaradi - istalgan tugma tutilib TugmaBosildi'da solishtiriladi (Esc - pauza)
	{
		FInputKeyBinding& Har = PlayerInputComponent->BindKey(EKeys::AnyKey, IE_Pressed, this, &ABozorOyinchi::TugmaBosildi);
		Har.bConsumeInput = false;
	}
	PlayerInputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ABozorOyinchi::SichqonchaBosildi);
	PlayerInputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &ABozorOyinchi::EnterBosildi);
	PlayerInputComponent->BindKey(EKeys::V, IE_Pressed, this, &ABozorOyinchi::VBosildi);
	PlayerInputComponent->BindKey(EKeys::V, IE_Released, this, &ABozorOyinchi::VQoyildi);
	// 1-9 raqamlari - javob variantlari (to'lovda kupyuralar ham)
	const FKey Raqamlar[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine };
	for (int32 I = 0; I < UE_ARRAY_COUNT(Raqamlar); ++I)
	{
		FInputKeyBinding B(FInputChord(Raqamlar[I]), IE_Pressed);
		B.KeyDelegate.GetDelegateForManualSet().BindWeakLambda(this, [this, I]() { if (bSuhbatda) Tanla(I); });
		PlayerInputComponent->KeyBindings.Add(B);
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
	const UBozorSozlamalar* Sz = UBozorSozlamalar::Get();
	const FVector2D V = Value.Get<FVector2D>() * Sz->Sezgirlik;
	AddControllerYawInput(V.X);
	AddControllerPitchInput(Sz->bTeskariY ? -V.Y : V.Y);
}

void ABozorOyinchi::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Quloq->Tinglayapti())
	{
		const int32 Chiziq = FMath::Clamp(FMath::RoundToInt(Quloq->GetBalandlik() * 12.f), 0, 10);
		Xabar = FString::Printf(TEXT("Gapiring...  %s  (V ni qo'yib yuboring)"), *FString::ChrN(FMath::Max(1, Chiziq), TEXT('|')));
		XabarVaqti = GetWorld()->GetTimeSeconds();
	}
	else if (Quloq->Tushunyapti())
	{
		Xabar = TEXT("Tushunilmoqda...");
		XabarVaqti = GetWorld()->GetTimeSeconds();
	}
	if (bTest) SotuvTestQadami();
	if (bBot) BotQadami();
	if (!bSuhbatda) YaqinSotuvchiniTop();
	else
	{
		if (TugatishVaqti > 0.0 && GetWorld()->GetTimeSeconds() >= TugatishVaqti) SuhbatniTugat();
		// suhbat paytida nigoh sekin sotuvchining yuziga buriladi
		const ABozorNpc* V = Suhbat ? Suhbat->GetSotuvchi() : nullptr;
		if (V && Controller)
		{
			const FRotator Maqsad = (V->GetActorLocation() + FVector(0.f, 0.f, 60.f) - Kamera->GetComponentLocation()).Rotation();
			Controller->SetControlRotation(FMath::RInterpTo(GetControlRotation(), FRotator(Maqsad.Pitch, Maqsad.Yaw, 0.f), DeltaSeconds, 4.f));
		}
	}
	KameraJoyi(DeltaSeconds);
}

// Shablondagi kabi kamera boshda: FP warp moslagan bosh suyagi joyi + ko'z siljishi (gavda yo'nalishida).
// Bosh suyagi masshtabi yig'ilgani uchun socket'ga biriktirilmaydi - joyi har kadr olinadi.
// Boshning kapsulaga nisbatan joyi ikkiga ajratiladi: sekin o'zgaradigan o'rtacha (egilish, sakrash) to'liq olinadi,
// qadam tebranishi esa kamaytiriladi - animatsiyadagi bosh chayqalishi ko'zda keskin va noqulay sezilardi
void ABozorOyinchi::KameraJoyi(float Dt)
{
	const int32 Bosh = Qollar ? Qollar->GetBoneIndex(TEXT("head")) : INDEX_NONE;
	if (Bosh == INDEX_NONE) return;
	const FTransform Kapsula = GetActorTransform();
	const FVector Mahalliy = Kapsula.InverseTransformPosition(Qollar->GetBoneLocation(TEXT("head")));
	if (!bKameraTayyor)
	{
		KameraOrtacha = Mahalliy;
		bKameraTayyor = true;
	}
	KameraOrtacha = FMath::VInterpTo(KameraOrtacha, Mahalliy, Dt, 3.f);
	const FVector Tebranish = (Mahalliy - KameraOrtacha) * TebranishKuchi;
	const FRotator Yaw(0.f, GetControlRotation().Yaw, 0.f);
	Kamera->SetWorldLocation(Kapsula.TransformPosition(KameraOrtacha + Tebranish) + Yaw.RotateVector(KozSiljish * (Boy / MeshBoyi)));
}

// ================= sotuv: E bilan sotuvchiga yaqinlashish (tanlov oynasi bo'lguncha gap konsol "Gapir" orqali)

void ABozorOyinchi::Chiqar(const FString& Kim, const FString& Matn, const FColor& Rang, bool bEkran)
{
	const FString Qator = FString::Printf(TEXT("%s: %s"), *Kim, *Matn);
	UE_LOG(LogBozorOyinchi, Display, TEXT("%s"), *Qator);
	if (bEkran && GEngine) GEngine->AddOnScreenDebugMessage(-1, 30.f, Rang, Qator, false, FVector2D(1.3f));
	FFileHelper::SaveStringToFile(Qator + LINE_TERMINATOR, *(FPaths::ProjectSavedDir() / TEXT("Sotuv_suhbat.txt")),
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
}

// Rasta yaqinida (GaplashishMasofasi) va sotuvchi tomonga qarab turgan bo'lsa - E belgisi chiqadi
void ABozorOyinchi::YaqinSotuvchiniTop()
{
	YaqinSotuvchi = nullptr;
	const UBozorSotuvTizimi* Tizim = UBozorSotuvTizimi::Get(this);
	const FBozorRasta* R = Tizim ? Tizim->EngYaqin(GetActorLocation(), GaplashishMasofasi) : nullptr;
	if (!R || !R->Sotuvchi) return;
	const FVector Tomon = (R->Sotuvchi->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	const FVector Qarash = FRotator(0.f, GetControlRotation().Yaw, 0.f).Vector();
	if (FVector::DotProduct(Tomon, Qarash) > 0.5f) YaqinSotuvchi = R->Sotuvchi;
}

void ABozorOyinchi::EBosildi()
{
	if (bSuhbatda) SuhbatniTugat();
	else SuhbatniBoshla();
}

bool ABozorOyinchi::SuhbatniBoshla()
{
	if (bSuhbatda) return true;
	YaqinSotuvchiniTop();
	ABozorNpc* V = YaqinSotuvchi.Get();
	const UBozorSotuvTizimi* Tizim = UBozorSotuvTizimi::Get(this);
	const FBozorRasta* R = V && Tizim ? Tizim->Top(V) : nullptr;
	if (!R) return false;
	if (!V->OyinchiBilanBoshla(this))
	{
		Xabar = TEXT("Sotuvchi boshqa xaridor bilan band - biroz kuting");
		XabarVaqti = GetWorld()->GetTimeSeconds();
		return false;
	}
	// shu sotuvchi bilan oldin boshlangan suhbat bo'lsa - davom etadi (narx va savat esda qoladi)
	if (!Suhbat || Suhbat->GetSotuvchi() != V)
	{
		Suhbat = NewObject<UBozorSuhbat>(this);
		Suhbat->Boshla(*R, Hamyon, NewObject<UBozorTayyorJavob>(this));
	}
	bSuhbatda = true;
	TugatishVaqti = -1.0;
	OxirgiGapim.Empty();
	Quloq->Isit(true);   // birinchi gapning boshi yo'qolmasin
	if (APlayerController* Pc = Cast<APlayerController>(GetController()))
	{
		Pc->SetIgnoreMoveInput(true);
		Pc->SetIgnoreLookInput(true);
	}
	KursorniKorsat(true);
	GetCharacterMovement()->StopMovementImmediately();
	Chiqar(TEXT("Sotuvchi"), FString::Printf(TEXT("fe'li: %s"), BozorSotuv::FelNomi(R->Fel)), FColor::Yellow);
	SotuvchiAytdi(Suhbat->KutibOl());
	return true;
}

void ABozorOyinchi::SuhbatniTugat()
{
	if (!bSuhbatda) return;
	bSuhbatda = false;
	TugatishVaqti = -1.0;
	Quloq->Isit(false);
	if (Suhbat && Suhbat->GetSotuvchi()) Suhbat->GetSotuvchi()->OyinchiBilanTugat();
	if (APlayerController* Pc = Cast<APlayerController>(GetController()))
	{
		Pc->SetIgnoreMoveInput(false);
		Pc->SetIgnoreLookInput(false);
	}
	KursorniKorsat(false);
	GapVaqti = GetWorld()->GetTimeSeconds();   // oxirgi gap yana biroz ko'rinib turadi
}

void ABozorOyinchi::KursorniKorsat(bool bHa)
{
	APlayerController* Pc = Cast<APlayerController>(GetController());
	if (!Pc) return;
	Pc->SetShowMouseCursor(bHa);
	if (bHa)
	{
		FInputModeGameAndUI M;
		M.SetHideCursorDuringCapture(false);
		M.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways);
		Pc->SetInputMode(M);
	}
	else Pc->SetInputMode(FInputModeGameOnly());
}

TArray<FBozorTanlov> ABozorOyinchi::GetTanlovlar() const
{
	return bSuhbatda && Suhbat && !Xayrlashdi() ? Suhbat->Tanlovlar() : TArray<FBozorTanlov>();
}

void ABozorOyinchi::Tanla(int32 I)
{
	if (!bSuhbatda || !Suhbat || Xayrlashdi()) return;
	FString Men, Javob;
	if (!Suhbat->Tanla(I, Men, Javob)) return;   // oyna ichidagi o'tish (miqdor, mollar ro'yxati)
	OxirgiGapim = Men;
	Chiqar(TEXT("Siz"), Men, FColor::White);
	SotuvchiAytdi(Javob);
	HamyonKor();
}

void ABozorOyinchi::Natija()
{
	if (bNatija) return;
	SuhbatniTugat();
	bNatija = true;
	if (APlayerController* Pc = Cast<APlayerController>(GetController()))
	{
		Pc->SetIgnoreMoveInput(true);
		Pc->SetIgnoreLookInput(true);
	}
	KursorniKorsat(true);
	GetCharacterMovement()->StopMovementImmediately();
	for (const FBozorNatijaQator& Q : Hamyon->NatijaQatorlari())
		Chiqar(TEXT("Xarid"), FString::Printf(TEXT("%s: kerak %s, olindi %s %s, to'landi %s so'm - %s"), *Q.Nomi,
			Q.Kerak < 0.f ? TEXT("-") : *BozorSotuv::Miqdor(Q.Kerak), *BozorSotuv::Miqdor(Q.Olindi), *Q.Birlik, *BozorSotuv::Som(Q.Tolandi), *Q.Baho()), FColor::White);
	Chiqar(TEXT("Natija"), FString::Printf(TEXT("%d yulduz | qoldi %s so'm | tejaldi %s | salomsiz %d/%d | qo'pol %d | qaytim sezildi %d, o'tkazildi %d | tarozi sezildi %d, o'tkazildi %d"),
		Hamyon->Yulduzlar(), *BozorSotuv::Som(Hamyon->Pul), *BozorSotuv::Som(Hamyon->Tejaldi), Hamyon->Salomsiz, Hamyon->Suhbatlar,
		Hamyon->Qopol, Hamyon->XatoQaytimSezildi, Hamyon->XatoQaytimOtkazildi, Hamyon->TaroziSezildi, Hamyon->TaroziOtkazildi), FColor::White);
}

void ABozorOyinchi::EnterBosildi()
{
	if (bNatija) DarajagaOt(Hamyon->Yulduzlar() > 0);
}

void ABozorOyinchi::RBosildi()
{
	if (bNatija) DarajagaOt(false);
}

void ABozorOyinchi::DarajagaOt(bool bKeyingi)
{
	const UBozorSotuvTizimi* Tz = UBozorSotuvTizimi::Get(this);
	const FBozorDaraja* D = Tz ? Tz->GetDaraja() : nullptr;
	int32 N = D ? D->Raqam : 1;
	if (bKeyingi && Tz && N < Tz->DarajalarSoni()) ++N;
	if (APlayerController* Pc = Cast<APlayerController>(GetController()))
		Pc->ConsoleCommand(FString::Printf(TEXT("open %s?Daraja=%d"), *UGameplayStatics::GetCurrentLevelName(this, false), N));
}

void ABozorOyinchi::SichqonchaBosildi()
{
	if (!bSuhbatda) return;
	APlayerController* Pc = Cast<APlayerController>(GetController());
	const ABozorHud* Hud = Pc ? Cast<ABozorHud>(Pc->GetHUD()) : nullptr;
	float X = 0.f, Y = 0.f;
	if (!Hud || !Pc->GetMousePosition(X, Y)) return;
	const int32 I = Hud->TanlovQayerda(FVector2D(X, Y));
	if (I != INDEX_NONE) Tanla(I);
}

// javob turiga mos harakat va (bor bo'lsa) yozilgan ovoz; xayrlashsa suhbat 2 s dan keyin o'zi yopiladi
void ABozorOyinchi::SotuvchiAytdi(const FString& Matn)
{
	ABozorNpc* V = Suhbat ? Suhbat->GetSotuvchi() : nullptr;
	UBozorAiMiya* Miya = UBozorAiMiya::Get(this);
	const int32 Raqam = ++MiyaSorov;
	// bot va sotuv sinovlari tayyor gaplar bilan (natija takrorlanishi uchun); -AiSinov - sotuv sinovida AI bilan
	static const bool bAiSinov = FParse::Param(FCommandLine::Get(), TEXT("AiSinov"));
	// jonli ovoz (Navoiy): ekranda chiqqan yakuniy gapni o'qiydi; sinov/botda - yozilgan ovozlar (natija takrorlanishi uchun)
	UBozorNavoiy* Navoiy = UBozorNavoiy::Get(this);
	const bool bJonli = Navoiy && Navoiy->Tayyormi() && V && !Matn.IsEmpty() && (!bTest || bAiSinov) && !bBot;
	AytVaqt = FPlatformTime::Seconds();
	if (Miya && Miya->Tayyormi() && V && !Matn.IsEmpty() && (!bTest || bAiSinov) && !bBot)
	{
		// sotuvchi "o'ylab turadi": matn LLM'dan qaytguncha (yoki kutish vaqti tugaguncha) uch nuqta
		SotuvchiGapi = TEXT("...");
		GapVaqti = GetWorld()->GetTimeSeconds();
		const FBozorSuhbatHolati& H = Suhbat->GetHolat();
		// gapda tilga olingan mol nomlari AI javobida ham qolishi kerak
		TArray<FString> Saqla;
		for (const FBozorMahsulot& M : H.Mollar)
			if (!M.Nomi.IsEmpty() && Matn.ToLower().Contains(M.Nomi.ToLower())) Saqla.AddUnique(M.Nomi);
		TWeakObjectPtr<ABozorOyinchi> Men(this);
		Miya->QaytaYoz(UBozorAiMiya::Shaxs(V, H.Fel), H.Munosabat, OxirgiGapim, Matn, Saqla, [Men, Raqam, Matn, bJonli](const FString& Javob, bool)
		{
			if (!Men.IsValid() || Men->MiyaSorov != Raqam) return;
			Men->SotuvchiMatni(Javob);
			if (bJonli) Men->JonliAyt(Raqam, Javob, Matn);
		});
	}
	else
	{
		SotuvchiMatni(Matn);
		if (bJonli) JonliAyt(Raqam, Matn, Matn);
	}
	if (!V) return;
	// shu gap uchun yozilgan ovoz bo'lsa (Ovoz/unreal_eksport.py) - o'shani aytadi, umumiy salom/rahmat klipi kerak emas.
	// Jonli ovoz bo'lsa - gap tayyor bo'lgach JonliAyt o'qiydi
	const bool bOvozli = bJonli || ABozorSotuvchiOvozi::SayLine(V, BozorSotuv::FelNomi(Suhbat->GetHolat().Fel), Matn);
	FName Harakat;
	switch (Suhbat->GetOxirgiJavob())
	{
	case EBozorJavob::Salom:
	case EBozorJavob::KutibOlish:
		Harakat = TEXT("V_Greet");
		if (!bOvozli) ABozorSotuvchiOvozi::Say(V, TEXT("salom"));
		break;
	case EBozorJavob::Narx:
	case EBozorJavob::MolSora:
	case EBozorJavob::MolTanlandi:
		Harakat = TEXT("V_Show");
		break;
	case EBozorJavob::TushirdiMan:
	case EBozorJavob::MiqdorSora:
	case EBozorJavob::TaroziKelishdi:
		Harakat = TEXT("V_Yes");
		break;
	case EBozorJavob::TushirmaymanEngPast:
	case EBozorJavob::TushirmaymanHozircha:
	case EBozorJavob::QopolgaRad:
	case EBozorJavob::NarxniKotardi:
	case EBozorJavob::PulYetmaydi:
	case EBozorJavob::HamyondaYoq:
	case EBozorJavob::SalomBering:
		Harakat = TEXT("V_No");
		break;
	case EBozorJavob::Tortdim:
	case EBozorJavob::PulOldim:
	case EBozorJavob::QaytimUzr:
	case EBozorJavob::TaroziUzr:
		Harakat = TEXT("V_Offer");
		break;
	case EBozorJavob::QaytimTogri:
	case EBozorJavob::TaroziTogri:
		Harakat = TEXT("V_No");
		break;
	case EBozorJavob::OshBolsin:
		Harakat = TEXT("V_Thanks");
		break;
	case EBozorJavob::Xayr:
	case EBozorJavob::XayrSotibOlmay:
		Harakat = TEXT("V_Thanks");
		if (!bOvozli) ABozorSotuvchiOvozi::Say(V, TEXT("rahmat"));
		TugatishVaqti = GetWorld()->GetTimeSeconds() + 2.0;
		break;
	case EBozorJavob::Haydadi:
		Harakat = TEXT("V_No");
		TugatishVaqti = GetWorld()->GetTimeSeconds() + 2.5;
		break;
	default:
		break;
	}
	if (!Harakat.IsNone()) V->Do(Harakat, 1.5f, 3.f);
}

void ABozorOyinchi::SotuvchiMatni(const FString& Matn)
{
	SotuvchiGapi = Matn;
	GapVaqti = GetWorld()->GetTimeSeconds();
	Chiqar(TEXT("Sotuvchi"), Matn, FColor::Yellow);
	// xayrlashgan bo'lsa - gap ekranda kamida 2 s tursin (AI javobi kechroq kelgan bo'lishi mumkin)
	if (TugatishVaqti > 0.0) TugatishVaqti = FMath::Max(TugatishVaqti, GapVaqti + 2.0);
}

void ABozorOyinchi::JonliAyt(int32 Raqam, const FString& Matn, const FString& Asl)
{
	ABozorNpc* V = Suhbat ? Suhbat->GetSotuvchi() : nullptr;
	UBozorNavoiy* Navoiy = UBozorNavoiy::Get(this);
	if (!V || !Navoiy) return;
	const FString Fel = BozorSotuv::FelNomi(Suhbat->GetHolat().Fel);
	// oxiridagi "[Tarozida: 2 kilo]" - o'yin belgisi, sotuvchi buni aytmaydi
	FString Gap = Matn.TrimEnd();
	const int32 Q = Gap.Find(TEXT("["), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
	if (Q > 0 && Gap.EndsWith(TEXT("]"))) Gap = Gap.Left(Q).TrimEnd();
	TWeakObjectPtr<ABozorOyinchi> Men(this);
	TWeakObjectPtr<ABozorNpc> Sotuvchi(V);
	bJonliKutish = true;
	const double MatnVaqt = FPlatformTime::Seconds();
	Navoiy->Ayt(Fel, V->bFemale, Gap, [Men, Sotuvchi, Raqam, Fel, Asl, Gap, MatnVaqt](USoundWave* Ovoz)
	{
		if (!Men.IsValid()) return;
		if (Men->MiyaSorov == Raqam) Men->bJonliKutish = false;
		// orada yangi gap boshlangan yoki suhbat tugagan bo'lsa - eski ovoz aytilmaydi
		if (!Sotuvchi.IsValid() || Men->MiyaSorov != Raqam || !Men->Suhbat || Men->Suhbat->GetSotuvchi() != Sotuvchi.Get()) return;
		const bool bJonliAytdi = ABozorSotuvchiOvozi::SayWave(Sotuvchi.Get(), Ovoz);
		if (!bJonliAytdi) ABozorSotuvchiOvozi::SayLine(Sotuvchi.Get(), Fel, Asl);
		const double Hozir = FPlatformTime::Seconds();
		UE_LOG(LogBozorOyinchi, Display, TEXT("[AiOvoz] %s: matn %.1f s + ovoz %.1f s = %.1f s kutish, gap %.1f s | %s"), bJonliAytdi ? TEXT("jonli") : TEXT("yozilgan"),
			MatnVaqt - Men->AytVaqt, Hozir - MatnVaqt, Hozir - Men->AytVaqt, Ovoz ? Ovoz->Duration : 0.f, *Gap);
	});
}

void ABozorOyinchi::YangiSotuvchi()
{
	SuhbatniTugat();
	Suhbat = nullptr;
	if (!SuhbatniBoshla()) Chiqar(TEXT("Bozor"), TEXT("Yaqin atrofda sotuvchi yo'q"), FColor::Red, true);
}

void ABozorOyinchi::RastalarniChiqar(bool bEkran)
{
	const UBozorSotuvTizimi* Tizim = UBozorSotuvTizimi::Get(this);
	TArray<FString> Qatorlar;
	(Tizim ? Tizim->Hisobot() : FString(TEXT("sotuv tizimi yo'q"))).ParseIntoArrayLines(Qatorlar);
	for (const FString& Q : Qatorlar) Chiqar(TEXT("Rasta"), Q.TrimStart(), FColor::Green, bEkran);
}

void ABozorOyinchi::Rastalar()
{
	RastalarniChiqar(true);
}

void ABozorOyinchi::Gapir(const FString& Matn)
{
	if (Matn.TrimStartAndEnd().IsEmpty()) return;
	if (!bSuhbatda && !SuhbatniBoshla())
	{
		Chiqar(TEXT("Bozor"), TEXT("Yaqin atrofda sotuvchi yo'q - rasta oldiga borib, sotuvchiga qarang"), FColor::Red, true);
		return;
	}
	OxirgiGapim = Matn;
	Chiqar(TEXT("Siz"), Matn, FColor::White);
	SotuvchiAytdi(Suhbat->Yoz(Matn));
	HamyonKor();
}

void ABozorOyinchi::HamyonKor()
{
	Chiqar(TEXT("Hamyon"), FString::Printf(TEXT("%s so'm (%s) | savat: %s"), *BozorSotuv::Som(Hamyon->Pul), *Hamyon->Tarkibi(),
		Hamyon->Savat.Num() ? *FString::Join(Hamyon->Savat, TEXT(", ")) : TEXT("bo'sh")), FColor::Cyan);
}

// Sinov: pomidorli (iloji bo'lsa bir nechta molli) rasta navbatining boshiga o'tadi, sotuvchiga qaraydi va gaplarni tuzadi
void ABozorOyinchi::TestRastasigaBor()
{
	const UBozorSotuvTizimi* Tizim = UBozorSotuvTizimi::Get(this);
	const ABozorNpcManager* Mgr = ABozorNpcManager::Get(this);
	TestGaplar = {TEXT("Rahmat, xayr!")};
	if (!Tizim || !Mgr || Tizim->GetRastalar().IsEmpty())
	{
		Chiqar(TEXT("Test"), TEXT("rasta topilmadi"), FColor::Red);
		return;
	}
	// qattiq fe'lli, bir nechta molli rasta - qo'pollik ta'siri (narx ko'tarish, haydash) ham sinovga tushadi
	auto Ball = [](const FBozorRasta& R)
	{
		const bool bPomidor = R.Mollar.ContainsByPredicate([](const FBozorMahsulot& M) { return M.Id == TEXT("pomidor"); });
		return (R.Fel == EBozorFel::Qattiq ? 4 : 0) + (bPomidor ? 2 : 0) + (R.Mollar.Num() > 1 ? 1 : 0);
	};
	const FBozorRasta* R = &Tizim->GetRastalar()[0];
	for (const FBozorRasta& X : Tizim->GetRastalar())
		if (Ball(X) > Ball(*R)) R = &X;
	RastagaBor(*R);
	int32 Asosiy = R->Mollar.IndexOfByPredicate([](const FBozorMahsulot& M) { return M.Id == TEXT("pomidor"); });
	if (Asosiy == INDEX_NONE) Asosiy = 0;
	const int32 Ikkinchi = R->Mollar.Num() > 1 ? (Asosiy + 1) % R->Mollar.Num() : Asosiy;
	using FShart = TFunction<bool(const FBozorTanlov&)>;
	auto Harakat = [](EBozorHarakat H, int32 Odob = 0, int32 Mol = INDEX_NONE) -> FShart
	{
		return [H, Odob, Mol](const FBozorTanlov& T)
		{
			return T.Tur == EBozorTanlovTur::Gap && T.Niyat.Harakat == H && (Odob == 0 || FMath::Sign(T.Niyat.Odob) == Odob) && (Mol == INDEX_NONE || T.Niyat.Mol == Mol);
		};
	};
	auto Tur = [](EBozorTanlovTur X) -> FShart { return [X](const FBozorTanlov& T) { return T.Tur == X; }; };
	// odobli xarid, keyin ikkinchi molda qo'pol savdolashish (kayfiyat buziladi)
	TestTanlovlar = {
		Harakat(EBozorHarakat::Salom),
		Harakat(EBozorHarakat::NarxAytish, 0, Asosiy),
		Harakat(EBozorHarakat::Savdolashish, 1),
		Harakat(EBozorHarakat::Savdolashish, 1),
		Tur(EBozorTanlovTur::MiqdorOch),
		Tur(EBozorTanlovTur::MiqdorKop),
		Harakat(EBozorHarakat::Tortish),
		Harakat(EBozorHarakat::TaroziRozi),
		Tur(EBozorTanlovTur::TolovOch),
		Tur(EBozorTanlovTur::KupyuraQoy),   // eng katta kupyura (ro'yxatda birinchi)
		Harakat(EBozorHarakat::PulQabul),
		Harakat(EBozorHarakat::QaytimKam),  // -XatoQaytim bilan sotuvchi kam beradi
		Tur(EBozorTanlovTur::MolRoyxati),
		Harakat(EBozorHarakat::NarxAytish, 0, Ikkinchi),
		Harakat(EBozorHarakat::Savdolashish, -1),
		Harakat(EBozorHarakat::Savdolashish, -1),
		Harakat(EBozorHarakat::Savdolashish, -1),
		Harakat(EBozorHarakat::Savdolashish, -1),
		Harakat(EBozorHarakat::Xayr),
	};
	TestSuratlar.Init(FString(), TestTanlovlar.Num());
	TestSuratlar[1] = TEXT("3d_narx_tanlovlar.png");
	TestSuratlar[3] = TEXT("3e_odobli_savdo.png");
	TestSuratlar[5] = TEXT("3f_miqdor.png");
	TestSuratlar[6] = TEXT("3f2_tarozi.png");
	TestSuratlar[8] = TEXT("3g_kupyuralar.png");
	TestSuratlar[9] = TEXT("3h_peshtaxta.png");
	TestSuratlar[10] = TEXT("3i_qaytim.png");
	TestSuratlar[11] = TEXT("3j_qaytim_kam.png");
	TestSuratlar[15] = TEXT("3k_qopol.png");
	TestGaplar = {TEXT("-")};   // TestQadam==5 da qayta chaqirilmasin
	RastalarniChiqar(false);
}

// rasta navbatining boshiga o'tadi (sinov va bot uchun) va sotuvchiga qaraydi
bool ABozorOyinchi::RastagaBor(const FBozorRasta& R)
{
	const ABozorNpcManager* Mgr = ABozorNpcManager::Get(this);
	if (!Mgr || !R.Sotuvchi) return false;
	for (const FBozorQueueSpot& Q : Mgr->Queues)
		if (Q.Vendor == R.Sotuvchi && Q.Points.Num())
		{
			SetActorLocation(Q.Points[0] + FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 5.f), false, nullptr, ETeleportType::TeleportPhysics);
			if (AController* C = GetController())
				C->SetControlRotation(FRotator(-10.f, (R.Sotuvchi->GetActorLocation() - GetActorLocation()).Rotation().Yaw, 0.f));
			return true;
		}
	return false;
}

// Bot: har 0.6 s bitta harakat. Ro'yxatdagi navbatdagi molni eng arzon rastadan oladi: salom, narx, 2 marta odobli savdo,
// miqdor, kupyuralar (kattasidan, ortiqcha bermaslikka harakat qiladi), qaytimni sanaydi, rahmat aytib xayrlashadi.
// Hammasi olingach boshlang'ich joyga qaytadi -> natija -> surat -> chiqish
void ABozorOyinchi::BotQadami()
{
	const double Hozir = GetWorld()->GetTimeSeconds();
	if (Hozir < BotKeyingi) return;
	BotKeyingi = Hozir + 0.6;
	APlayerController* Pc = Cast<APlayerController>(GetController());
	const UBozorSotuvTizimi* Tz = UBozorSotuvTizimi::Get(this);
	const FString Dir = FPaths::ProjectSavedDir() / TEXT("Screenshots/DarajaTest/");
	const int32 Daraja = Tz && Tz->GetDaraja() ? Tz->GetDaraja()->Raqam : 0;
	if (BotBoshJoy.IsZero())
	{
		if (Hozir < 3.0) return;   // HUD boshlang'ich joyni yozib olsin
		BotBoshJoy = GetActorLocation();
		IFileManager::Get().Delete(*(FPaths::ProjectSavedDir() / TEXT("Sotuv_suhbat.txt")));
		if (Pc) Pc->ConsoleCommand(TEXT("r.MotionBlurQuality 0"));
		return;
	}
	if (BotBosqich == 1) { if (bNatija) { BotBosqich = 2; BotKeyingi = Hozir + 1.0; } return; }
	if (BotBosqich == 2)
	{
		FScreenshotRequest::RequestScreenshot(Dir + FString::Printf(TEXT("daraja%d_natija.png"), Daraja), true, false);
		BotBosqich = 3;
		BotKeyingi = Hozir + 1.0;
		return;
	}
	if (BotBosqich == 3) { if (Pc) Pc->ConsoleCommand(TEXT("quit")); BotBosqich = 4; return; }
	if (BotBosqich != 0 || !Tz) return;

	if (!bSuhbatda)
	{
		// navbatdagi olinmagan mol
		const FBozorRoyxatQator* Q = Hamyon->Royxat.FindByPredicate([this](const FBozorRoyxatQator& X) { return !X.bOlindi && !BotTashlandi.Contains(X.Nomi); });
		if (!Q)
		{
			Chiqar(TEXT("Bot"), FString::Printf(TEXT("ro'yxat tugadi (olinmadi: %s) - chiqishga qaytadi"), BotTashlandi.Num() ? *FString::Join(BotTashlandi, TEXT(", ")) : TEXT("yo'q")), FColor::White);
			SetActorLocation(BotBoshJoy, false, nullptr, ETeleportType::TeleportPhysics);
			BotBosqich = 1;
			if (BotTashlandi.Num()) Natija();   // ro'yxat to'liq emas - HUD o'zi ochmaydi
			return;
		}
		// shu mol bor eng arzon rasta
		const FBozorRasta* Eng = nullptr;
		int32 EngNarx = MAX_int32;
		for (const FBozorRasta& R : Tz->GetRastalar())
			for (const FBozorMahsulot& M : R.Mollar)
				if (M.Nomi.Equals(Q->Nomi, ESearchCase::IgnoreCase) && M.Narx < EngNarx) { EngNarx = M.Narx; Eng = &R; }
		if (!Eng)
		{
			Chiqar(TEXT("Bot"), FString::Printf(TEXT("%s hech qayerda yo'q"), *Q->Nomi), FColor::Red);
			BotTashlandi.Add(Q->Nomi);
			return;
		}
		BotMol = Q->Nomi;
		Chiqar(TEXT("Bot"), FString::Printf(TEXT("%s: eng arzoni stall %d da %s so'm"), *BotMol, Eng->Stall, *BozorSotuv::Som(EngNarx)), FColor::White);
		RastagaBor(*Eng);
		BotKeyingi = Hozir + 0.4;
		if (!SuhbatniBoshla()) BotKeyingi = Hozir + 2.0;   // sotuvchi band - kutadi
		return;
	}
	if (Xayrlashdi() || !Suhbat) return;

	const FBozorSuhbatHolati& H = Suhbat->GetHolat();
	const TArray<FBozorTanlov> V = GetTanlovlar();
	const int32 Mol = H.Mollar.IndexOfByPredicate([this](const FBozorMahsulot& M) { return M.Nomi.Equals(BotMol, ESearchCase::IgnoreCase); });
	const FBozorRoyxatQator* Qator = Hamyon->Royxat.FindByPredicate([this](const FBozorRoyxatQator& X) { return X.Nomi.Equals(BotMol, ESearchCase::IgnoreCase); });
	auto Top = [&V](TFunctionRef<bool(const FBozorTanlov&)> F) { return V.IndexOfByPredicate(F); };
	auto Gap = [](EBozorHarakat Hk, int32 Odob = -2) { return [Hk, Odob](const FBozorTanlov& T) { return T.Tur == EBozorTanlovTur::Gap && T.Niyat.Harakat == Hk && (Odob == -2 || T.Niyat.Odob == Odob); }; };
	auto Tur = [](EBozorTanlovTur X) { return [X](const FBozorTanlov& T) { return T.Tur == X; }; };

	int32 I = INDEX_NONE;
	const bool bBuMol = H.Tanlangan == Mol && Mol != INDEX_NONE;
	if (H.Bosqich == EBozorBosqich::QaytimTekshir)
	{
		int32 Olindi = 0;
		for (int32 K : H.QaytimKupyuralar) Olindi += K;
		const bool bKam = Olindi < BotBerildi - BotSumma;
		Chiqar(TEXT("Bot"), FString::Printf(TEXT("qaytim: kerak %s, olindi %s"), *BozorSotuv::Som(BotBerildi - BotSumma), *BozorSotuv::Som(Olindi)), FColor::White);
		I = Top(Gap(bKam ? EBozorHarakat::QaytimKam : EBozorHarakat::QaytimRozi));
	}
	else if (H.Bosqich == EBozorBosqich::TaroziTekshir)
	{
		// tarozidagini so'ralgani bilan solishtiradi: farq bo'lsa odob bilan aytadi
		const bool bXato = !FMath::IsNearlyEqual(H.Miqdor, H.SoralganMiqdor);
		Chiqar(TEXT("Bot"), FString::Printf(TEXT("tarozi: so'ralgan %s, tarozida %s %s"), *BozorSotuv::Miqdor(H.SoralganMiqdor), *BozorSotuv::Miqdor(H.Miqdor), *H.Mahsulot.Birlik), FColor::White);
		I = Top(Gap(bXato ? EBozorHarakat::TaroziAyt : EBozorHarakat::TaroziRozi, 1));
	}
	else if (Suhbat->Tolovda())
	{
		int32 Jami = 0;
		for (int32 K : Suhbat->GetPeshtaxta()) Jami += K;
		if (Jami >= H.Summa) { I = Top(Gap(EBozorHarakat::PulQabul)); BotBerildi = Jami; BotSumma = H.Summa; }
		else
		{
			// qolgan summadan katta bo'lmagan eng katta kupyura, bo'lmasa undan kattasining eng kichigi
			const int32 Qoldi = H.Summa - Jami;
			int32 Eng = INDEX_NONE;
			for (int32 J = 0; J < V.Num(); ++J)
				if (V[J].Tur == EBozorTanlovTur::KupyuraQoy && V[J].Kupyura <= Qoldi && (Eng == INDEX_NONE || V[J].Kupyura > V[Eng].Kupyura)) Eng = J;
			if (Eng == INDEX_NONE)
				for (int32 J = 0; J < V.Num(); ++J)
					if (V[J].Tur == EBozorTanlovTur::KupyuraQoy && (Eng == INDEX_NONE || V[J].Kupyura < V[Eng].Kupyura)) Eng = J;
			I = Eng;
		}
	}
	else if (Suhbat->MiqdorTanlanmoqda())
	{
		const float Kerak = Qator ? Qator->Miqdor : 1.f;
		const float Hoz = Suhbat->GetTanlovMiqdor();
		I = FMath::IsNearlyEqual(Hoz, Kerak) ? Top(Gap(EBozorHarakat::Tortish)) : Top(Tur(Hoz < Kerak ? EBozorTanlovTur::MiqdorKop : EBozorTanlovTur::MiqdorKam));
	}
	else if (!H.bSalomlashdi) I = Top(Gap(EBozorHarakat::Salom));
	else if (H.Bosqich == EBozorBosqich::Tugadi) I = Top(Gap(EBozorHarakat::Xayr));
	else if (H.Bosqich == EBozorBosqich::PulKutilmoqda)
	{
		I = Top(Tur(EBozorTanlovTur::TolovOch));
		if (I == INDEX_NONE && H.Savdolashuv < 4) I = Top(Gap(EBozorHarakat::Savdolashish, 1));   // pul yetmaydi - yana so'raydi
		if (I == INDEX_NONE)
		{
			Chiqar(TEXT("Bot"), FString::Printf(TEXT("%s ga pul yetmadi"), *BotMol), FColor::Red);
			BotTashlandi.Add(BotMol);
			I = Top(Gap(EBozorHarakat::Xayr));
		}
	}
	else if (!bBuMol || H.JoriyNarx == 0) I = Top([Mol](const FBozorTanlov& T) { return T.Tur == EBozorTanlovTur::Gap && T.Niyat.Harakat == EBozorHarakat::NarxAytish && T.Niyat.Mol == Mol; });
	else if (H.Savdolashuv < 2) I = Top(Gap(EBozorHarakat::Savdolashish, 1));
	else I = Top(Tur(EBozorTanlovTur::MiqdorOch));

	if (I == INDEX_NONE)
	{
		TArray<FString> Bor;
		for (const FBozorTanlov& X : V) Bor.Add(X.Matn);
		Chiqar(TEXT("Bot"), FString::Printf(TEXT("mos variant yo'q (%s): [%s] - xayrlashadi"), *BotMol, *FString::Join(Bor, TEXT(" | "))), FColor::Red);
		BotTashlandi.Add(BotMol);
		SuhbatniTugat();
		return;
	}
	const EBozorBosqich Oldin = H.Bosqich;
	Tanla(I);
	if (Oldin == EBozorBosqich::PulKutilmoqda && Suhbat && Suhbat->GetHolat().Bosqich != EBozorBosqich::PulKutilmoqda)
		FScreenshotRequest::RequestScreenshot(Dir + FString::Printf(TEXT("daraja%d_%s_tolov.png"), Daraja, *BotMol), true, false);
}

// -SotuvTest: 3 s yuradi (qo'llar ko'rinishini suratga oladi), keyin rastaga o'tib tayyor suhbat va yana surat, so'ng chiqadi
void ABozorOyinchi::SotuvTestQadami()
{
	const double T = GetWorld()->GetTimeSeconds() - TestVaqt;
	APlayerController* Pc = Cast<APlayerController>(GetController());
	const FString Dir = FPaths::ProjectSavedDir() / TEXT("Screenshots/SotuvTest/");
	static const bool bAiSinov = FParse::Param(FCommandLine::Get(), TEXT("AiSinov"));

	// -AiSinov: LLM va Navoiy (o'rnatilgan bo'lsa) tayyor bo'lguncha kutadi (eng ko'pi 3 daqiqa)
	if (TestQadam == 0 && bAiSinov)
	{
		const double Hozir = GetWorld()->GetTimeSeconds();
		if (TestKutishBosh < 0.0) TestKutishBosh = Hozir;
		const UBozorAiMiya* Miya = UBozorAiMiya::Get(this);
		const UBozorNavoiy* Navoiy = UBozorNavoiy::Get(this);
		const bool bMiya = !UBozorAiMiya::FayllarBormi() || (Miya && Miya->Tayyormi());
		const bool bNavoiy = !UBozorNavoiy::FayllarBormi() || (Navoiy && Navoiy->Tayyormi());
		if (!(bMiya && bNavoiy) && Hozir - TestKutishBosh < 180.0)
		{
			TestVaqt = Hozir;
			return;
		}
	}

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
	if (Pc && TestQadam >= 1) Pc->SetControlRotation(FRotator(TestQadam <= 3 ? -5.f : (TestQadam == 4 ? -70.f : -10.f), GetControlRotation().Yaw, 0.f));
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
	// rastaga o'tadi -> E belgisi surati -> E bosiladi (sotuvchi kutib oladi) -> surat -> gaplar
	if (TestQadam == 5 && TestGaplar.IsEmpty()) TestRastasigaBor();
	if (TestQadam == 5 && !bTestSuratE && T > 7.6) { FScreenshotRequest::RequestScreenshot(Dir + TEXT("3b_e_belgi.png"), true, false); bTestSuratE = true; }
	if (TestQadam == 5 && bTestSuratE && !bSuhbatda && !bTestSuratKutib && T > 8.0) EBosildi();
	if (TestQadam == 5 && bSuhbatda && !bTestSuratKutib && T > 9.0) { FScreenshotRequest::RequestScreenshot(Dir + TEXT("3c_kutib_olish.png"), true, false); bTestSuratKutib = true; }
	// har qadamda shartga mos variant tanlanadi (topilmasa - logga yoziladi va o'tkaziladi); surat tanlovdan keyin
	const int32 N = TestTanlovlar.Num();
	const int32 Gap = TestQadam - 5;
	const double GapBosh = 9.5, GapOraliq = 1.3;
	// AI sinovida: sotuvchi o'ylab bo'lib (matn chiqdi), ovozini aytib bo'lgach 1 s dan keyin keyingi tanlov
	ABozorNpc* Sot = Suhbat ? Suhbat->GetSotuvchi() : nullptr;
	const bool bBand = bJonliKutish || SotuvchiGapi == TEXT("...") || (Sot && ABozorSotuvchiOvozi::Gapiryapti(Sot));
	if (bAiSinov && bBand) TestKeyingi = T + 1.0;
	const bool bNavbat = bAiSinov ? (T > GapBosh && T > TestKeyingi && !bBand) : T > GapBosh + Gap * GapOraliq;
	if (bTestSuratKutib && Gap >= 0 && Gap < N && bNavbat)
	{
		const TArray<FBozorTanlov> V = GetTanlovlar();
		const int32 I = V.IndexOfByPredicate(TestTanlovlar[Gap]);
		if (I == INDEX_NONE)
		{
			TArray<FString> Bor;
			for (const FBozorTanlov& X : V) Bor.Add(X.Matn);
			Chiqar(TEXT("Test"), FString::Printf(TEXT("%d-qadam: mos variant yo'q [%s]"), Gap, *FString::Join(Bor, TEXT(" | "))), FColor::Red);
		}
		else
		{
			Chiqar(TEXT("Test"), FString::Printf(TEXT("%d-qadam: %d-variant \"%s\""), Gap, I + 1, *V[I].Matn), FColor::White);
			Tanla(I);
			if (Suhbat) Chiqar(TEXT("Test"), FString::Printf(TEXT("munosabat %d, narx %s"), Suhbat->GetHolat().Munosabat, *BozorSotuv::Som(Suhbat->GetHolat().JoriyNarx)), FColor::White);
		}
		if (TestSuratlar.IsValidIndex(Gap) && !TestSuratlar[Gap].IsEmpty()) FScreenshotRequest::RequestScreenshot(Dir + TestSuratlar[Gap], true, false);
		++TestQadam;
	}
	if (Gap == N && (bAiSinov ? bNavbat : T > GapBosh + Gap * GapOraliq - 0.6)) { FScreenshotRequest::RequestScreenshot(Dir + TEXT("4_suhbat.png"), true, false); ++TestQadam; }
	// personajni tashqaridan ko'rish: oldiga kamera qo'yiladi, bola unga qarab yuradi
	if (Gap == N + 1 && Pc && !bSuhbatda)
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
	// natija oynasi (ro'yxat to'liq bo'lmasa ham ko'rsatiladi), surat, chiqish
	if (Gap == N + 4 && T > TestVaqt2 + 4.0 && Pc) { Pc->SetViewTarget(this); Natija(); ++TestQadam; }
	if (Gap == N + 5 && T > TestVaqt2 + 5.5) { FScreenshotRequest::RequestScreenshot(Dir + TEXT("7_natija.png"), true, false); ++TestQadam; }
	if (Gap == N + 6 && T > TestVaqt2 + 6.5 && Pc) { Pc->ConsoleCommand(TEXT("quit")); ++TestQadam; }
}

// ================= GameMode

ABozorGameMode::ABozorGameMode()
{
	DefaultPawnClass = ABozorOyinchi::StaticClass();
	HUDClass = ABozorHud::StaticClass();
}
