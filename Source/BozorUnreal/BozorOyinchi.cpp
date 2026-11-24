#include "BozorOyinchi.h"
#include "BozorAiMiya.h"
#include "BozorNavoiy.h"
#include "BozorPaket.h"
#include "AudioMixerBlueprintLibrary.h"
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

static TAutoConsoleVariable<float> CVarMiyaKorinishKutish(TEXT("bozor.MiyaKorinishKutish"), 5.f,
	TEXT("AI (bulut) javobini necha soniya kutish; kech qolsa sotuvchi tayyor gapni aytadi"));

// 1 edi: karnaydan qaytgan bozor ovozi gap deb olinardi (2026-11-19) - endi faqat V bosib turib gapiriladi
static TAutoConsoleVariable<int32> CVarQolsiz(TEXT("bozor.Qolsiz"), 0,
	TEXT("Ovozli chat kabi qo'lsiz suhbat: 1 - suhbatda mikrofon o'zi tinglaydi (tugmasiz, gapni bo'lish mumkin), 0 - faqat V bosib turib"));

namespace
{
	// Mol uzatish (MolBer/MolYangila). Bola molni ushlaydigan payt (V_Handout boshidan, s). Cashier_Handout: sotuvchi qo'li
	// 0.4 s da cho'zilib 3.6 s gacha turadi - bola qo'li shu oraliqning boshida yetib boradi (avval 0.55 x davom = 2.2 s edi:
	// sotuvchi qo'lini uzoq cho'zib kutardi, keyin mol 0.3 s da kamon bo'ylab "otilib" kelardi)
	float MolUzatPayti(float MolH) { return FMath::Min(0.55f * MolH, 1.3f); }
	// ushlagandan keyin bola molni o'ziga tortib oladigan vaqt (s)
	constexpr float MolTortish = 0.5f;
	// FP qo'l nishoni kameradan shu masofada (sm): bola modeli 0.66 masshtabda, qo'li ~40 sm - uzoqroq nishonga IK yetmaydi
	constexpr float MolFpMasofa = 30.f;

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
	QolniCHoz();
	TabiiyQol.Qoy(this);
	GavdaniYigish();
	Super::FinalizeBoneTransform();
}

// O'ng qo'l IK (yelka-tirsak-kaft): tirsak hozirgi bukilish tomonida qoladi, qo'lning qolgan suyaklari (aylanish suyaklari,
// barmoqlar) ota-onasiga nisbatan avvalgi joyida ergashadi
void UBozorQolMesh::QolniCHoz()
{
	if (QolAlfa <= 0.001f) return;
	const USkeletalMesh* M = GetSkeletalMeshAsset();
	if (!M) return;
	const FReferenceSkeleton& Ref = M->GetRefSkeleton();
	if (IkMeshi != M)
	{
		IkMeshi = M;
		Yelka = Ref.FindBoneIndex(TEXT("upperarm_r"));
		Tirsak = Ref.FindBoneIndex(TEXT("lowerarm_r"));
		Kaft = Ref.FindBoneIndex(TEXT("hand_r"));
		OngQolZanjiri.Reset();
		for (int32 i = 1; i < Ref.GetNum(); ++i)
		{
			bool bAvlod = false;
			for (int32 P = Ref.GetParentIndex(i); P != INDEX_NONE && !bAvlod; P = Ref.GetParentIndex(P)) bAvlod = P == Yelka;
			if (bAvlod) OngQolZanjiri.Add(i);
		}
	}
	TArray<FTransform>& Cs = GetEditableComponentSpaceTransforms();
	if (Yelka == INDEX_NONE || Tirsak == INDEX_NONE || Kaft == INDEX_NONE || Kaft >= Cs.Num()) return;
	TArray<FTransform> Eski;
	Eski.Reserve(OngQolZanjiri.Num());
	for (const int32 I : OngQolZanjiri) Eski.Add(Cs[I]);
	const FTransform EskiYelka = Cs[Yelka];

	FTransform A = Cs[Yelka], B = Cs[Tirsak], C = Cs[Kaft];
	const FVector Nishon = FMath::Lerp(C.GetLocation(), GetComponentTransform().InverseTransformPosition(QolNishon), FMath::Clamp(QolAlfa, 0.f, 1.f));
	const FVector Orta = (A.GetLocation() + C.GetLocation()) * 0.5f;
	const FVector Qutb = B.GetLocation() + (B.GetLocation() - Orta).GetSafeNormal() * 30.f;
	AnimationCore::SolveTwoBoneIK(A, B, C, Qutb, Nishon, false, 1.f, 1.f);
	Cs[Yelka] = A;
	Cs[Tirsak] = B;
	Cs[Kaft] = C;
	// avlodlar: ota-onaga nisbatan avvalgi joyi saqlanadi (Tirsak va Kaft IK'dan)
	auto EskiCs = [&](int32 I) -> const FTransform&
	{
		if (I == Yelka) return EskiYelka;
		return Eski[OngQolZanjiri.IndexOfByKey(I)];
	};
	for (int32 k = 0; k < OngQolZanjiri.Num(); ++k)
	{
		const int32 I = OngQolZanjiri[k];
		if (I == Tirsak || I == Kaft || I >= Cs.Num()) continue;
		const int32 P = Ref.GetParentIndex(I);
		Cs[I] = Eski[k].GetRelativeTransform(EskiCs(P)) * Cs[P];
	}
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
	// -QuloqFayl=<audio>: mikrofonsiz STT sinovi (masofaviy Rubai yoki lokal) - natija logda "STT (...)" qatorida, keyin o'yin yopiladi
	FString QuloqFayl;
	if (FParse::Value(FCommandLine::Get(), TEXT("QuloqFayl="), QuloqFayl))
	{
		TWeakObjectPtr<ABozorOyinchi> Men(this);
		FTimerHandle H;
		GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([Men, QuloqFayl]()
		{
			if (!Men.IsValid()) return;
			Men->Quloq->NatijaKeldi = [Men](const FString& Matn)
			{
				UE_LOG(LogBozorOyinchi, Display, TEXT("[QuloqFayl] natija: \"%s\""), *Matn);
				if (Men.IsValid()) UKismetSystemLibrary::QuitGame(Men.Get(), nullptr, EQuitPreference::Quit, false);
			};
			Men->Quloq->Fayldan(QuloqFayl);
		}), 4.f, false);
	}
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
			// qo'lsiz rejimda bo'sh natija - shovqin (yo'tal, taqillash): indamaymiz
			if (Quloq->AvtoGapmi()) return;
			Xabar = TEXT("Eshitmadim - V ni bosib turib, aniqroq gapiring");
			XabarVaqti = GetWorld()->GetTimeSeconds();
			return;
		}
		Xabar.Reset();
		Gapir(Matn);
	};
	Quloq->NutqBoshlandi = [this]() { GapBoshlandi(); };
}

void ABozorOyinchi::GapBoshlandi()
{
	// ovozli chatdagi kabi: o'yinchi gapira boshlasa sotuvchi to'xtaydi va tinglaydi; kechikib kelgan eski javob aytilmaydi
	ABozorNpc* V = Suhbat ? Suhbat->GetSotuvchi() : nullptr;
	if (!bSuhbatda || !V || !ABozorSotuvchiOvozi::Gapiryapti(V)) return;
	ABozorSotuvchiOvozi::Jimla(V);
	++MiyaSorov;
	bJonliKutish = false;
	UE_LOG(LogBozorOyinchi, Display, TEXT("[OvozliChat] o'yinchi sotuvchining gapini bo'ldi"));
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
		return;
	}
	// V bosildi - sotuvchi gapini to'xtatadi: uning ovozi karnaydan o'yinchi yozuviga aralashmasin
	GapBoshlandi();
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
	PlayerInputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &ABozorOyinchi::EnterBosildi);
	PlayerInputComponent->BindKey(EKeys::V, IE_Pressed, this, &ABozorOyinchi::VBosildi);
	PlayerInputComponent->BindKey(EKeys::V, IE_Released, this, &ABozorOyinchi::VQoyildi);
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
	const bool bQolsiz = bSuhbatda && Quloq->Qolsizmi();
	ABozorNpc* SuhbatSotuvchi = bSuhbatda && Suhbat ? Suhbat->GetSotuvchi() : nullptr;
	const bool bSotuvchiGapiryapti = SuhbatSotuvchi && ABozorSotuvchiOvozi::Gapiryapti(SuhbatSotuvchi);
	// karnaydan qaytgan sotuvchi ovozi o'yinchi gapi deb olinmasin: u gapirayotganda esik baland
	if (bQolsiz) Quloq->SetAksSado(bSotuvchiGapiryapti ? 3.f : 1.f);
	if (Quloq->Tinglayapti())
	{
		const int32 ChiziqSoni = FMath::Clamp(FMath::RoundToInt(Quloq->GetBalandlik() * 12.f), 0, 10);
		const FString Chiziqlar = FString::ChrN(FMath::Max(1, ChiziqSoni), TEXT('|'));
		Xabar = bQolsiz ? FString::Printf(TEXT("Eshityapman...  %s"), *Chiziqlar) : FString::Printf(TEXT("Gapiring...  %s  (V ni qo'yib yuboring)"), *Chiziqlar);
		XabarVaqti = GetWorld()->GetTimeSeconds();
	}
	else if (Quloq->Tushunyapti())
	{
		Xabar = TEXT("Tushunilmoqda...");
		XabarVaqti = GetWorld()->GetTimeSeconds();
	}
	else if (bQolsiz)
	{
		// ovozli chat holati: sotuvchi o'ylayapti / gapiryapti / sizni tinglayapti
		Xabar = SotuvchiGapi == TEXT("...") || bJonliKutish ? TEXT("Sotuvchi o'ylayapti...")
			: bSotuvchiGapiryapti ? TEXT("Sotuvchi gapiryapti - gapini bo'lishingiz mumkin")
			: TEXT("Tinglayapman - bemalol gapiravering");
		XabarVaqti = GetWorld()->GetTimeSeconds();
	}
	// AI javobi kech qoldi - sotuvchi tayyor gapni aytadi (bulut 10 s gacha javob bermasligi mumkin)
	if (KutilganRaqam != 0 && FPlatformTime::Seconds() - KutilganVaqt > FMath::Max(1.f, CVarMiyaKorinishKutish.GetValueOnGameThread()))
		KutilganniAyt();
	MolYangila(DeltaSeconds);
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
	// ona telefon qilayotganda E - qo'ng'iroqqa javob / keyingi gap (BozorTelefon), sotuvchi bilan suhbat boshlanmaydi
	if (const APlayerController* P = Cast<APlayerController>(GetController()))
		if (const ABozorHud* Hud = Cast<ABozorHud>(P->GetHUD()); Hud && Hud->TelefonFaol()) return;
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
	const bool bAvvalgi = Suhbat && Suhbat->GetSotuvchi() == V;
	if (bAvvalgi && Suhbat->GetOxirgiJavob() == EBozorJavob::Haydadi)
	{
		Xabar = TEXT("Bu sotuvchi siz bilan boshqa gaplashmaydi - boshqa rastaga boring");
		XabarVaqti = GetWorld()->GetTimeSeconds();
		return false;
	}
	if (!V->OyinchiBilanBoshla(this))
	{
		Xabar = TEXT("Sotuvchi boshqa xaridor bilan band - biroz kuting");
		XabarVaqti = GetWorld()->GetTimeSeconds();
		return false;
	}
	// shu sotuvchi bilan oldin boshlangan suhbat bo'lsa - davom etadi (narx va savat esda qoladi);
	// xayrlashib ketilgan bo'lsa - qaytib kelganda yangi suhbat (yana salomdan)
	if (!bAvvalgi || Suhbat->GetOxirgiJavob() == EBozorJavob::Xayr || Suhbat->GetOxirgiJavob() == EBozorJavob::XayrSotibOlmay)
	{
		Suhbat = NewObject<UBozorSuhbat>(this);
		Suhbat->Boshla(*R, Hamyon, NewObject<UBozorTayyorJavob>(this), Tizim->BoshMunosabat(*R));
		Tarix.Reset();   // yangi suhbat - yangi tarix
	}
	bSuhbatda = true;
	TugatishVaqti = -1.0;
	OxirgiGapim.Empty();
	// mikrofon faqat V bosib turilganda ochiq (fonda tinglash yo'q). Qo'lsiz rejim (bozor.Qolsiz 1) o'zi Isit(true) qiladi
	Quloq->SetQolsiz(CVarQolsiz.GetValueOnGameThread() != 0 && !bTest && !bBot);
	if (APlayerController* Pc = Cast<APlayerController>(GetController()))
	{
		Pc->SetIgnoreMoveInput(true);
		Pc->SetIgnoreLookInput(true);
	}
	// javob variantlari yo'q - suhbat faqat ovoz/matn orqali, kursor kerak emas
	GetCharacterMovement()->StopMovementImmediately();
	Chiqar(TEXT("Sotuvchi"), FString::Printf(TEXT("%s, fe'li: %s, munosabat %d"), R->Xulq.Ism.IsEmpty() ? *GetNameSafe(V) : *R->Xulq.Ism,
		BozorSotuv::FelNomi(R->Fel), Suhbat->GetHolat().Munosabat), FColor::Yellow);
	SotuvchiAytdi(Suhbat->KutibOl());
	return true;
}

void ABozorOyinchi::SuhbatniTugat()
{
	if (!bSuhbatda) return;
	bSuhbatda = false;
	TugatishVaqti = -1.0;
	Quloq->SetQolsiz(false);
	Quloq->Isit(false);
	if (Suhbat && Suhbat->GetSotuvchi())
	{
		Suhbat->GetSotuvchi()->OyinchiBilanTugat();
		// sotuvchi bolani eslab qoladi: keyingi suhbat shu munosabatdan boshlanadi
		if (UBozorSotuvTizimi* Tizim = UBozorSotuvTizimi::Get(this)) Tizim->Eslab(Suhbat->GetSotuvchi(), Suhbat->GetHolat().Munosabat);
	}
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
	// javob variantlari yo'q - suhbat faqat ovoz/matn orqali, kursor kerak emas
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
	// oldingi gapning AI javobi hali kelmagan bo'lsa (bola tez-tez gapirdi / bosdi) yoki AI yaqinda kech qoldi - bu gap
	// AI'siz, darhol aytiladi. Aks holda har yangi gap eskisini bekor qilib, sotuvchi umuman gapirmay qolardi
	const bool bOldingiKutilmoqda = KutilganRaqam != 0 && KutilganRaqam == Raqam - 1;
	KutilganRaqam = 0;
	const bool bMiyaBand = bOldingiKutilmoqda || FPlatformTime::Seconds() < MiyaDamOlish;
	if (bMiyaBand && Miya && Miya->Tayyormi() && V && !Matn.IsEmpty())
		UE_LOG(LogBozorOyinchi, Display, TEXT("[AiOvoz] %s - tayyor gap darhol: %s"),
			bOldingiKutilmoqda ? TEXT("oldingi AI javobi kelmagan") : TEXT("AI sekin"), *Matn);
	if (!bMiyaBand && Miya && Miya->Tayyormi() && V && !Matn.IsEmpty() && (!bTest || bAiSinov) && !bBot)
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
		// LLM ham videokartada: javob yozilayotganda kadr cheklanadi (Navoiy bilan bir xil; o'yin GPU'ni to'liq band qilsa javob sekinlashadi)
		TWeakObjectPtr<UBozorNavoiy> NavoiyZaif(Navoiy);
		if (Navoiy) Navoiy->KadrChekla(true);
		// bolaning ohangiga munosabat (tanbeh/maqtov) LLM'ga berilmaydi - u buni "ortiqcha" deb tashlab yuboradi; javob oldiga qaytadi
		const FString Izoh = Suhbat->GetOxirgiIzoh();
		FString Mano = Matn;
		if (!Izoh.IsEmpty() && Mano.StartsWith(Izoh)) Mano = Mano.RightChop(Izoh.Len()).TrimStart();
		KutilganRaqam = Raqam;
		KutilganMatn = Matn;
		KutilganVaqt = FPlatformTime::Seconds();
		bKutilganJonli = bJonli;
		auto Tugadi = [Men, Raqam, Matn, Izoh, bJonli, NavoiyZaif](const FString& Javob0, bool)
		{
			const FString Javob = Izoh.IsEmpty() ? Javob0 : Izoh + TEXT(" ") + Javob0;
			// kutish vaqti o'tib, tayyor matn allaqachon aytilgan bo'lsa (KutilganRaqam tozalangan) - kech kelgan javob tashlanadi
			if (Men.IsValid() && Men->MiyaSorov == Raqam && Men->KutilganRaqam == Raqam)
			{
				Men->KutilganRaqam = 0;
				Men->SotuvchiMatni(Javob);
				if (bJonli) Men->JonliAyt(Raqam, Javob, Matn);   // Navoiy o'z cheklovini oladi - bu yerdagisi keyin qo'yib yuboriladi
			}
			if (UBozorNavoiy* N = NavoiyZaif.Get()) N->KadrChekla(false);
		};
		// Ovozli chat: savdo mantiqi gapni tushunmasa (savdodan tashqari gap yoki savol) - "Tushunmadim" o'rniga sotuvchi o'z
		// shaxsida, suhbat tarixini bilib erkin javob beradi. Pul bilan bog'liq hamma narsa baribir UBozorSuhbat'da
		if (Suhbat->GetOxirgiJavob() == EBozorJavob::Tushunmadim && !OxirgiGapim.IsEmpty())
		{
			TArray<FString> Nomlar;
			for (const FBozorMahsulot& M : H.Mollar) Nomlar.Add(M.Nomi);
			Miya->Suhbatlash(UBozorAiMiya::Shaxs(V, H.Fel), H.Munosabat, Tarix, OxirgiGapim, FString::Join(Nomlar, TEXT(", ")), Mano, Tugadi);
		}
		else Miya->QaytaYoz(UBozorAiMiya::Shaxs(V, H.Fel), H.Munosabat, OxirgiGapim, Mano, Saqla, Tugadi, Suhbat->GetOxirgiOdob(), &Tarix, H.Savdolashuv);
	}
	else
	{
		SotuvchiMatni(Matn);
		if (bJonli) JonliAyt(Raqam, Matn, Matn);
	}
	if (!V) return;
	// shu gap uchun yozilgan ovoz bo'lsa (Ovoz/unreal_eksport.py) - o'shani aytadi, umumiy salom/rahmat klipi kerak emas.
	// Jonli ovoz bo'lsa - gap tayyor bo'lgach JonliAyt o'qiydi. Ular bo'lmasa - oldindan yozilgan Navoiy bo'laklari (SayGap;
	// faqat AI gapni qayta yozmagan bo'lsa: bo'laklar asl matnga mos)
	const bool bAiYoz = !bMiyaBand && Miya && Miya->Tayyormi() && !Matn.IsEmpty() && (!bTest || bAiSinov) && !bBot;
	const bool bOvozli = bJonli || ABozorSotuvchiOvozi::SayLine(V, BozorSotuv::FelNomi(Suhbat->GetHolat().Fel), Matn)
		|| (!bAiYoz && ABozorSotuvchiOvozi::SayGap(V, Suhbat->GetBolaklar(), Suhbat->GetUslub()));
	FName Harakat;
	// Scripts/savdo_sinov.ps1 shu qatorlardan sotuvchi harakatlari ketma-ketligini tekshiradi
	UE_LOG(LogBozorOyinchi, Log, TEXT("[SavdoAnim] javob=%s"), *StaticEnum<EBozorJavob>()->GetNameStringByValue(int64(Suhbat->GetOxirgiJavob())));
	switch (Suhbat->GetOxirgiJavob())
	{
	case EBozorJavob::KutibOlish:
		// ta'zim OyinchiBilanBoshla'da boshlangan - qayta chaqirilsa klip boshidan qayta boshlanib uziladi
		if (!bOvozli) ABozorSotuvchiOvozi::Say(V, TEXT("salom"));
		break;
	case EBozorJavob::Salom:
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
	case EBozorJavob::PulOldim:
		// pul olindi - molni uzatadi (sahna V_Handout'ni o'zi o'ynaydi)
		MolBer(V);
		break;
	case EBozorJavob::Tortdim:
	case EBozorJavob::TaroziUzr:
		// tarozida tortadi (NPC savdosidagi kabi); V_Offer - uzatish klipi, mol esa faqat pul olingach beriladi (MolBer)
		Harakat = TEXT("V_Weigh");
		ABozorRastaOvozlari::PlayNear(this, { TEXT("tarozi_pip"), TEXT("tarozi_tosh") },
			V->GetActorLocation() + V->GetActorForwardVector() * 40.f + FVector(0.f, 0.f, 15.f), 300.f, TEXT("tarozi_pip"), V);
		break;
	case EBozorJavob::QaytimUzr:
		Harakat = TEXT("V_Offer");   // qolgan qaytimni uzatadi
		break;
	case EBozorJavob::QaytimTogri:
	case EBozorJavob::TaroziTogri:
		Harakat = TEXT("V_No");
		break;
	case EBozorJavob::OshBolsin:
		// "Osh bo'lsin! Yana nima kerak?" - bosh irg'aydi; ta'zim xayrlashuvda (ikki ta'zim ketma-ket bo'lmasin)
		Harakat = TEXT("V_Yes");
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
	if (!Harakat.IsNone())
	{
		// mol hali sotuvchi qo'lida bo'lsa (MolYangila: 0.55 x MolH da uchadi) - keyingi harakat uzatish tugagach,
		// aks holda yangi klip qo'lni mol bilan birga tortib oladi ("Rahmat!" tez bosilganda)
		const float Kut = UzatishSotuvchi.Get() == V && UzatishBosh >= 0.0 ? MolUzatPayti(MolH) + 0.3f - float(GetWorld()->GetTimeSeconds() - UzatishBosh) : 0.f;
		if (Kut > 0.f) V->DoLater(Harakat, Kut + 2.f, Kut);
		else V->Do(Harakat, 1.5f, 3.f);
	}
}

// ================= mol uzatish

namespace
{
	// FP qo'llar FOV'i va dunyo FOV'i: bir xil ekran nuqtasi uchun kamera fazosidagi yon/tik ulushi shu nisbatda
	float FovNisbati(const UCameraComponent* K)
	{
		return K->bEnableFirstPersonFieldOfView
			? FMath::Tan(FMath::DegreesToRadians(K->FirstPersonFieldOfView * 0.5f)) / FMath::Tan(FMath::DegreesToRadians(K->FieldOfView * 0.5f))
			: 1.f;
	}
}

FVector ABozorOyinchi::FpGa(const FVector& Dunyo, float Masofa) const
{
	const FVector K = Kamera->GetComponentLocation();
	const FRotator R = Kamera->GetComponentRotation();
	const FVector L = R.UnrotateVector(Dunyo - K);
	const float F = FovNisbati(Kamera);
	const FVector Yon(1.f, L.Y / FMath::Max(L.X, 1.f) * F, L.Z / FMath::Max(L.X, 1.f) * F);
	return K + R.RotateVector(Yon.GetSafeNormal() * Masofa);
}

// FP nuqtasi ekranda turgan joyga dunyo nuqtasi; masofa - ko'rinadigan o'lcham mos kelsin (dunyo FOV'i kengroq)
FVector ABozorOyinchi::DunyoGa(const FVector& Fp) const
{
	const FVector K = Kamera->GetComponentLocation();
	const FRotator R = Kamera->GetComponentRotation();
	const FVector L = R.UnrotateVector(Fp - K);
	const float F = FovNisbati(Kamera);
	const FVector Yon(1.f, L.Y / FMath::Max(L.X, 1.f) / F, L.Z / FMath::Max(L.X, 1.f) / F);
	return K + R.RotateVector(Yon.GetSafeNormal() * L.Size() / F);
}

void ABozorOyinchi::MolBer(ABozorNpc* V)
{
	MolTozala();
	if (!V || !Suhbat) return;
	const FBozorSuhbatHolati& H = Suhbat->GetHolat();
	// kilo - xaltada, dona - rastadagi o'zi (bo'lmasa xalta)
	UStaticMesh* MolMesh = nullptr;
	const bool bKilo = H.Mahsulot.Birlik.Equals(TEXT("kilo"), ESearchCase::IgnoreCase);
	if (!bKilo)
		if (const UBozorSotuvTizimi* Tz = UBozorSotuvTizimi::Get(this))
			for (const FBozorRasta& R : Tz->GetRastalar())
				if (R.Sotuvchi == V)
					for (int32 i = 0; i < R.Mollar.Num(); ++i)
						if (R.Mollar[i].Id == H.Mahsulot.Id && R.MolMeshlari.IsValidIndex(i)) MolMesh = R.MolMeshlari[i];
	const bool bXalta = !MolMesh;
	if (!MolMesh) MolMesh = V->XaltaMesh();
	if (!MolMesh)
	{
		UE_LOG(LogBozorOyinchi, Warning, TEXT("[MolBer] %s: %s uchun mesh ham, xalta ham yo'q - mol uzatilmaydi"), *V->GetActorNameOrLabel(), *H.Mahsulot.Nomi);
		return;
	}
	BerilganMol = NewObject<UStaticMeshComponent>(this, TEXT("BerilganMol"));
	BerilganMol->SetStaticMesh(MolMesh);
	BerilganMol->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BerilganMol->SetMobility(EComponentMobility::Movable);
	BerilganMol->RegisterComponent();
	// xalta ~24 sm (yangi paket 46 sm - FP'da ekrandan chiqmasin), dona (non, qovun) eng katta o'lchami ~22 sm
	const float Kattasi = MolMesh->GetBoundingBox().GetSize().GetMax();
	MolOlcham = Kattasi > 1.f ? FMath::Clamp((bXalta ? 24.f : 22.f) / Kattasi, 0.02f, 4.f) : 1.f;
	BerilganMol->SetWorldScale3D(FVector(MolOlcham));
	const FBox Ch = MolMesh->GetBoundingBox();
	MolTutqich = bXalta ? FVector(Ch.GetCenter().X, Ch.GetCenter().Y, Ch.Max.Z) : Ch.GetCenter();
	// paket ichida - olingan mol (kilo: tarozidagi miqdorga qarab 2..8 dona; sabzi, pomidor ...)
	if (bXalta)
		BozorPaket::IchigaSol(BerilganMol, BozorPaket::MolMeshi(H.Mahsulot.Nomi), FMath::Clamp(FMath::RoundToInt(H.Miqdor * 4.f), 2, 8), FMath::Rand());
	MolSotuvchi = V;
	// to'liq uzunlikda (Cashier_Handout 4 s: qo'l ~0.4 s da cho'ziladi va oxirigacha ushlab turadi - 3 s da kesilsa qo'l keskin qaytardi)
	MolH = V->Do(TEXT("V_Handout"), 2.f, 4.5f);
	MolBosh = GetWorld()->GetTimeSeconds();
	UzatishSotuvchi = V;
	UzatishBosh = MolBosh;
	bMolFp = false;
	UE_LOG(LogBozorOyinchi, Display, TEXT("[MolBer] %s: %s (%s), uzatish %.1f s"), *V->GetActorNameOrLabel(), *MolMesh->GetName(), *H.Mahsulot.Nomi, MolH);
}

// bola qo'li sotuvchi qo'lidagi molga yeta oladimi: yaqinda (2.5 m) va ekranda, kamera oldida (FP qo'l faqat oldinga cho'ziladi)
bool ABozorOyinchi::MolYetadimi(const FVector& Dunyo) const
{
	const FVector L = Kamera->GetComponentRotation().UnrotateVector(Dunyo - Kamera->GetComponentLocation());
	return L.Size() < 250.f && L.X > 20.f && FMath::Abs(L.Y) < L.X * 1.2f && FMath::Abs(L.Z) < L.X;
}

// qo'lga olingan mol ushlab turiladigan joy (FP): o'ngda, ko'z balandligidan sal pastda - ekranda suhbat panelidan o'ngda.
// Xalta dastasidan kaftga osiladi: qo'l pastroq bo'lsa tanasi ekrandan chiqib, yeng ortida qolardi
FVector ABozorOyinchi::MolQolJoyi() const
{
	return Kamera->GetComponentLocation() + Kamera->GetForwardVector() * 30.f + Kamera->GetUpVector() * 6.f + Kamera->GetRightVector() * 15.f;
}

void ABozorOyinchi::MolTozala()
{
	if (BerilganMol)
		for (UStaticMeshComponent* M : BozorPaket::Ichidagilar(BerilganMol)) M->DestroyComponent();
	if (BerilganMol) BerilganMol->DestroyComponent();
	BerilganMol = nullptr;
	MolSotuvchi = nullptr;
	MolBosh = -1.0;
	if (Qollar) Qollar->QolAlfa = 0.f;
}

void ABozorOyinchi::MolYangila(float Dt)
{
	if (MolBosh < 0.0 || !BerilganMol) return;
	ABozorNpc* V = MolSotuvchi.Get();
	const float T = float(GetWorld()->GetTimeSeconds() - MolBosh);
	const float Uzat = MolUzatPayti(MolH);      // bola qo'li molga yetib, uni ushlaydi
	const float Uch = Uzat + MolTortish;        // mol bolaning o'ngdagi ushlab turish joyiga yetadi
	const FVector KaftFp = Qollar->GetSocketLocation(TEXT("hand_r"));
	const FVector Kam = Kamera->GetComponentLocation(), Old = Kamera->GetForwardVector(), Past = -Kamera->GetUpVector();
	auto Silliq = [](float A, float B, float X) { const float S = FMath::Clamp((X - A) / (B - A), 0.f, 1.f); return S * S * (3.f - 2.f * S); };
	if (T < Uzat)
	{
		if (!V) { MolTozala(); return; }
		// sotuvchining o'ng qo'lida (bolaga sal yaqin); xalta dastasidan osiladi, dona markazidan ushlanadi
		const FVector Q = V->GetMesh()->GetSocketLocation(TEXT("hand_r"));
		const FRotator Bur(0.f, V->GetActorRotation().Yaw, 0.f);
		const FVector Tutqich = Q + (Kam - Q).GetSafeNormal2D() * 5.f;
		BerilganMol->SetWorldLocationAndRotation(Tutqich - Bur.RotateVector(MolTutqich * MolOlcham), Bur);
		// bola uzatish tugamay ketib qoldi yoki o'girildi - qo'li yetmaydi: kutmasdan ushlash bosqichiga (mol pastdan qo'lga chiqadi)
		if (!MolYetadimi(Tutqich))
		{
			MolBosh = GetWorld()->GetTimeSeconds() - Uzat;
			MolYangila(0.f);
			return;
		}
		// bola qo'lini molning tutqichiga cho'zadi (ekranda tutqich turgan joyga) - sotuvchi qo'li cho'zilgach (0.4 s) boshlaydi
		Qollar->QolNishon = FpGa(Tutqich, MolFpMasofa);
		Qollar->QolAlfa = Silliq(FMath::Min(0.4f, 0.5f * Uzat), Uzat, T);
		if (bTest)
		{
			static int32 Surat = 0;
			const float Paytlar[] = { 0.5f * Uzat, Uzat - 0.05f };
			if (Surat < UE_ARRAY_COUNT(Paytlar) && T > Paytlar[Surat])
				FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / FString::Printf(TEXT("Screenshots/SotuvTest/6_uzatish_%d.png"), Surat++), false, false);
		}
		return;
	}
	const float F = FovNisbati(Kamera);
	if (!bMolFp)
	{
		// Ushlash: bola qo'li tutqichga yetdi - mol shu kadrdan bolaning qo'lida. U uchmaydi: FP fazosiga (qo'llar bilan birga,
		// devor/peshtaxtaga kirmaydi) ekranda AYNAN turgan joyi va ko'rinadigan o'lchami bilan o'tadi, keyin qo'l bilan birga tortiladi.
		// Dunyoda D masofadagi s o'lchamli narsa = FP'da M masofadagi s * F * M / D o'lchamli narsa (FP FOV torroq - F nisbati)
		bMolFp = true;
		const FVector G0 = BerilganMol->GetComponentLocation() + BerilganMol->GetComponentRotation().RotateVector(MolTutqich * MolOlcham);
		const float D0 = FMath::Max(FVector::Dist(G0, Kam), 10.f);
		if (MolYetadimi(G0))
		{
			MolSBosh = MolOlcham * F * MolFpMasofa / D0;
			// tutqich FP'da kaftdan qancha farq qiladi (IK nishonga to'liq yetmasa) - tortish boshida asta yo'qoladi
			MolUchishBosh = FpGa(G0, MolFpMasofa) - (KaftFp + Old * 2.f);
		}
		else
		{
			// sotuvchi uzoqda yoki ko'rinmaydi: uzoqdan uchirmaymiz - mol ekran pastidan kaftga ko'tariladi
			MolSBosh = MolOlcham * 0.35f;
			MolUchishBosh = Past * 25.f;
		}
		MolQolBosh = Qollar->QolNishon;
		MolBurBosh = BerilganMol->GetComponentQuat();
		BerilganMol->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
		BerilganMol->MarkRenderStateDirty();
		for (UStaticMeshComponent* M : BozorPaket::Ichidagilar(BerilganMol))
		{
			M->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
			M->MarkRenderStateDirty();
		}
		UE_LOG(LogBozorOyinchi, Log, TEXT("[MolUshla] %.2f s, %s, masofa %.0f sm, kaftdan farq %.1f sm"), T,
			MolYetadimi(G0) ? TEXT("qo'ldan") : TEXT("uzoqda - pastdan"), D0, MolUchishBosh.Size());
	}
	// Tortish: qo'l molni ushlagan joyidan o'ngdagi ushlab turish joyiga olib keladi (mol kaftda); yaqinlashgani sari kattalashadi
	const float Tort = Silliq(Uzat, Uch, T);
	const float Osish = Silliq(Uzat, Uch + 0.2f, T);
	const float Tushish = Silliq(Uch + 1.9f, Uch + 2.4f, T);   // sumkaga
	// -DarajaTest: mol bola qo'liga yetgach (paket bo'lsa - ichidagi mol bilan) surat
	if (bBot && T >= Uch + 0.6f && T - Dt < Uch + 0.6f && BerilganMol && BerilganMol->GetStaticMesh())
		FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / FString::Printf(TEXT("Screenshots/DarajaTest/qolda_%s.png"), *BerilganMol->GetStaticMesh()->GetName()), true, false);
	// ushlash paytidagi ko'rinishdan qo'ldagi tabiiy kattaligiga (FP'da kameraga yaqin - 0.55)
	const float S = FMath::Lerp(MolSBosh, MolOlcham * 0.55f, Osish) * (1.f - 0.6f * Tushish);
	BerilganMol->SetWorldScale3D(FVector(S));
	// qo'lda kameraga nisbatan tik turadi (bola ko'tarib ko'rsatadi): dunyoga nisbatan osilsa, pastga qaraganda
	// xaltaning tanasi ekranning pastki chetidan chiqib ketardi
	const FQuat Tik = FRotator(Kamera->GetComponentRotation().Pitch, Kamera->GetComponentRotation().Yaw, 0.f).Quaternion();
	BerilganMol->SetWorldRotation(FQuat::Slerp(MolBurBosh, Tik, Silliq(Uzat, Uch, T)));
	const FRotator Bur = BerilganMol->GetComponentRotation();
	const FVector Farq = MolUchishBosh * (1.f - Silliq(Uzat, Uzat + 0.25f, T));
	BerilganMol->SetWorldLocation(KaftFp + Old * 2.f + Farq - Bur.RotateVector(MolTutqich * S) + Past * 30.f * Tushish);
	// qo'l ushlagan joyidan o'ngdagi joyiga (ekran markazida - suhbat paneli ostida - qolmaydi), keyin animatsiyadagi joyiga
	Qollar->QolNishon = FMath::Lerp(MolQolBosh, MolQolJoyi(), Tort);
	Qollar->QolAlfa = 1.f - Silliq(Uch + 1.6f, Uch + 2.4f, T);
	static bool bTortSurat = false;
	if (bTest && !bTortSurat && T > Uzat + 0.2f)
	{
		bTortSurat = true;
		FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots/SotuvTest/6_uzatish_2_tortish.png"), false, false);
	}
	// qo'ldagi mol ekranning qayerida: suhbat paneli (ABozorHud::SotuvChiz - markazda, kengligi min(1200 S, ekran - 80 S)) ostida
	// qolmasligi kerak. Scripts/savdo_sinov.ps1 shu qatorni tekshiradi
	for (const float Payt : { Uch + 0.15f, Uch + 0.5f, Uch + 1.f })
		if (T - Dt <= Payt && T > Payt)
		if (const APlayerController* Pc = Cast<APlayerController>(GetController()))
		{
			int32 Vx = 0, Vy = 0;
			FVector2D E, U;
			Pc->GetViewportSize(Vx, Vy);
			// markazi va yuqori cheti (xalta kaftdan osiladi - markazi pastda bo'lishi tabiiy, tepasi ko'rinishi kerak)
			const FVector Tepa = BerilganMol->Bounds.Origin + Kamera->GetUpVector() * BerilganMol->Bounds.BoxExtent.Z;
			if (Vx > 0 && Pc->ProjectWorldLocationToScreen(DunyoGa(BerilganMol->Bounds.Origin), E) && Pc->ProjectWorldLocationToScreen(DunyoGa(Tepa), U))
			{
				const float S0 = Vy / 1080.f;
				const float PanelW = FMath::Min(1200.f * S0, Vx - 80.f * S0);
				UE_LOG(LogBozorOyinchi, Log, TEXT("[SavdoAnim] qo'ldagi mol (+%.2f s) ekranda x=%.2f y=%.2f tepasi y=%.2f, panel x=%.2f..%.2f"),
					T - Uch, E.X / Vx, E.Y / Vy, U.Y / Vy, (Vx - PanelW) * 0.5f / Vx, (Vx + PanelW) * 0.5f / Vx);
			}
		}
	if (bTest)
	{
		// -SotuvTest: uzatish sahnasining lahzalari
		const FString Dir = FPaths::ProjectSavedDir() / TEXT("Screenshots/SotuvTest/");
		static int32 Surat = 0;
		const float Paytlar[] = { Uch + 0.15f, Uch + 1.f, Uch + 2.1f };
		if (Surat < UE_ARRAY_COUNT(Paytlar) && T > Paytlar[Surat]) FScreenshotRequest::RequestScreenshot(Dir + FString::Printf(TEXT("7_mol_%d_qolda.png"), Surat++), false, false);
	}
	if (T > Uch + 2.4f) MolTozala();
}

void ABozorOyinchi::SotuvchiMatni(const FString& Matn)
{
	SotuvchiGapi = Matn;
	GapVaqti = GetWorld()->GetTimeSeconds();
	// gap ekranga chiqdi - ovoz hali yo'q bo'lsa ham sotuvchining og'zi qimirlaydi (ovoz kelsa uning konverti boshqaradi)
	if (Suhbat && Suhbat->GetSotuvchi()) ABozorSotuvchiOvozi::JimGapir(Suhbat->GetSotuvchi(), Matn);
	Chiqar(TEXT("Sotuvchi"), Matn, FColor::Yellow);
	// suhbat tarixi (erkin suhbatda LLM eslasin); "[Tarozida: ...]" - o'yin belgisi, gap emas
	if (!OxirgiGapim.IsEmpty() && Matn != TEXT("..."))
	{
		FString Gap = Matn;
		const int32 Q = Gap.Find(TEXT("["), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		if (Q > 0 && Gap.EndsWith(TEXT("]"))) Gap = Gap.Left(Q).TrimEnd();
		Tarix.Emplace(OxirgiGapim, Gap);
		if (Tarix.Num() > 12) Tarix.RemoveAt(0);
		OxirgiGapim.Reset();   // bir gapga bitta javob (sotuvchining o'zi boshlagan gaplari tarixga bola gapisiz kirmaydi)
	}
	// xayrlashgan bo'lsa - gap ekranda kamida 2 s tursin (AI javobi kechroq kelgan bo'lishi mumkin)
	if (TugatishVaqti > 0.0) TugatishVaqti = FMath::Max(TugatishVaqti, GapVaqti + 2.0);
}

void ABozorOyinchi::KutilganniAyt()
{
	const int32 Raqam = KutilganRaqam;
	KutilganRaqam = 0;
	// orada yangi gap boshlangan, gap bo'lingan yoki suhbat tugagan bo'lsa - aytilmaydi
	if (Raqam != MiyaSorov || !bSuhbatda || !Suhbat || !Suhbat->GetSotuvchi()) return;
	UE_LOG(LogBozorOyinchi, Display, TEXT("[AiOvoz] AI %.1f s da javob bermadi - tayyor gap: %s"), FPlatformTime::Seconds() - KutilganVaqt, *KutilganMatn);
	// bulut sekinlashgan: keyingi gaplar ham bir muddat AI'siz (har gapda yana kutib qolmaslik uchun)
	MiyaDamOlish = FPlatformTime::Seconds() + 20.0;
	SotuvchiMatni(KutilganMatn);
	if (bKutilganJonli) JonliAyt(Raqam, KutilganMatn, KutilganMatn);
	else ABozorSotuvchiOvozi::SayGap(Suhbat->GetSotuvchi(), Suhbat->GetBolaklar(), Suhbat->GetUslub());
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
	// har sotuvchining o'z ovozi (xulq.json "ovoz.ohang")
	const UBozorSotuvTizimi* Tizim = UBozorSotuvTizimi::Get(this);
	const FBozorRasta* R = Tizim ? Tizim->Top(V) : nullptr;
	const float Ohang = R ? R->Xulq.Ohang : 0.f;
	const FString Namuna = R ? R->Xulq.OvozNamuna : FString();
	// server ovozi xulqdan; Malika ovozlarida javob turiga qarab kayfiyat (xursand/afsus). Temp - natijaviy tezlik, chalishdagi
	// pitch (xulq "tezlik") ham tempni o'zgartiradi - serverdan temp/pitch so'raladi
	FString ServerOvoz = R ? R->Xulq.ServerOvoz : FString();
	if (ServerOvoz.StartsWith(TEXT("malika_kore")) && Suhbat)
	{
		switch (Suhbat->GetOxirgiJavob())
		{
		case EBozorJavob::OshBolsin: case EBozorJavob::QaytimTogri: case EBozorJavob::TushirdiMan: case EBozorJavob::PulOldim:
			ServerOvoz = TEXT("malika_kore_xursand"); break;
		case EBozorJavob::QaytimUzr: case EBozorJavob::TaroziUzr: case EBozorJavob::PulYetmaydi: case EBozorJavob::HamyondaYoq:
		case EBozorJavob::TushirmaymanEngPast: case EBozorJavob::XayrSotibOlmay:
			ServerOvoz = TEXT("malika_kore_afsus"); break;
		default: break;
		}
	}
	const float ServerTezlik = R && R->Xulq.Temp > 0.f ? R->Xulq.Temp / FMath::Max(0.5f, R->Xulq.OvozTezlik) : 0.f;
	Navoiy->Ayt(Fel, V->Ayolmi(), Gap, [Men, Sotuvchi, Raqam, Fel, Asl, Gap, MatnVaqt](USoundWave* Ovoz)
	{
		if (!Men.IsValid()) return;
		if (Men->MiyaSorov == Raqam) Men->bJonliKutish = false;
		// orada yangi gap boshlangan yoki suhbat tugagan bo'lsa - eski ovoz aytilmaydi
		if (!Sotuvchi.IsValid() || Men->MiyaSorov != Raqam || !Men->Suhbat || Men->Suhbat->GetSotuvchi() != Sotuvchi.Get()) return;
		const bool bJonliAytdi = ABozorSotuvchiOvozi::SayWave(Sotuvchi.Get(), Ovoz);
		// Navoiy ovoz bermadi: shu gapning yozilgan ovozi, u ham bo'lmasa - oldindan yozilgan bo'laklar (sotuvchi ovozsiz qolmasin)
		if (!bJonliAytdi && !ABozorSotuvchiOvozi::SayLine(Sotuvchi.Get(), Fel, Asl))
			ABozorSotuvchiOvozi::SayGap(Sotuvchi.Get(), Men->Suhbat->GetBolaklar(), Men->Suhbat->GetUslub());
		const double Hozir = FPlatformTime::Seconds();
		UE_LOG(LogBozorOyinchi, Display, TEXT("[AiOvoz] %s: matn %.1f s + ovoz %.1f s = %.1f s kutish, gap %.1f s | %s"), bJonliAytdi ? TEXT("jonli") : TEXT("yozilgan"),
			MatnVaqt - Men->AytVaqt, Hozir - MatnVaqt, Hozir - Men->AytVaqt, Ovoz ? Ovoz->Duration : 0.f, *Gap);
	}, Ohang, Namuna, ServerOvoz, ServerTezlik);
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
		// tarozidagini so'ralgani bilan solishtiradi: farq bo'lsa odob bilan aytadi (-Indamas - hech qachon aytmaydi, "aytmaslik" yo'li sinovi)
		static const bool bIndamas = FParse::Param(FCommandLine::Get(), TEXT("Indamas"));
		const bool bXato = !FMath::IsNearlyEqual(H.Miqdor, H.SoralganMiqdor);
		Chiqar(TEXT("Bot"), FString::Printf(TEXT("tarozi: so'ralgan %s, tarozida %s %s%s"), *BozorSotuv::Miqdor(H.SoralganMiqdor), *BozorSotuv::Miqdor(H.Miqdor), *H.Mahsulot.Birlik,
			bXato && bIndamas ? TEXT(" - indamaydi") : TEXT("")), FColor::White);
		I = Top(Gap(bXato && !bIndamas ? EBozorHarakat::TaroziAyt : EBozorHarakat::TaroziRozi, 1));
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
		// AI sinovida o'yinning umumiy ovoz chiqishi yoziladi: jonli gaplar haqiqatan eshitiladimi (Saved/Navoiy/ai_sinov_ovoz.wav)
		if (bAiSinov)
		{
			UAudioMixerBlueprintLibrary::StartRecordingOutput(this, 0.f);
			UE_LOG(LogBozorOyinchi, Display, TEXT("[OvozYozuv] boshlandi"));
		}
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
	if (Gap == N && (bAiSinov ? bNavbat : T > GapBosh + Gap * GapOraliq - 0.6))
	{
		FScreenshotRequest::RequestScreenshot(Dir + TEXT("4_suhbat.png"), true, false);
		if (bAiSinov)
		{
			UAudioMixerBlueprintLibrary::StopRecordingOutput(this, EAudioRecordingExportType::WavFile, TEXT("ai_sinov_ovoz"),
				FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Navoiy")));
			UE_LOG(LogBozorOyinchi, Display, TEXT("[OvozYozuv] tugadi"));
		}
		++TestQadam;
	}
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
