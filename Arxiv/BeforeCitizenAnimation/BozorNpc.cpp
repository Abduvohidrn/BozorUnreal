#include "BozorNpc.h"

#include "AIController.h"
#include "BozorOvoz.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DetourCrowdAIController.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "Navigation/CrowdFollowingComponent.h"
#include "TimerManager.h"
#include "TwoBoneIK.h"
#include "UnrealClient.h"
#if WITH_EDITOR
#include "ShaderCompiler.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogBozorNpc, Log, All);

namespace
{
	const FName SlotName(TEXT("DefaultSlot"));
	const FName MoveState(TEXT("Move"));

	FVector Flat(FVector V)
	{
		V.Z = 0.0;
		return V;
	}

	// Unity Mathf.Clamp: chegaralar teskari bo'lsa ham yiqilmaydi
	float SafeClamp(float X, float Lo, float Hi) { return FMath::Clamp(X, Lo, FMath::Max(Lo, Hi)); }

	// DetourCrowd NavMesh chetiga (poldagi mol, rasta burchagi) radiusidan yaqin kelgan agentni teskari tomonga tortib,
	// joyida aylantirib qo'yadi: tezlik 0-5 sm/s, 11-28 s (2026-11-09, sinovda tasdiqlandi - steering o'chsa darhol yuradi).
	// Shunda crowd steering vaqtincha o'chiriladi, agent oddiy yo'l bo'yicha chiqib ketadi
	void SetCrowdSteering(AAIController* C, bool bOn)
	{
		if (UCrowdFollowingComponent* CF = C ? Cast<UCrowdFollowingComponent>(C->GetPathFollowingComponent()) : nullptr)
		{
			CF->SuspendCrowdSteering(!bOn);
		}
	}
}

// ============================================================ korutin

bool FBozorCo::StepHandle(FHandle Co, float Dt)
{
	auto& P = Co.promise();
	// ichki korutin tugasa, ota o'sha kadrda davom etadi (Unity'dagi kabi); cheklov - cheksiz aylanishdan himoya
	for (int32 Guard = 0; Guard < 256; ++Guard)
	{
		if (Co.done()) return true;
		if (P.Child)
		{
			if (!StepHandle(P.Child, Dt)) return false;
			P.Child = nullptr;
		}
		else if (P.bWaiting)
		{
			// shu kadrda boshlangan kutish keyingi kadrdan sanaladi
			if (P.WaitFrame == GFrameCounter) return false;
			P.WaitFrame = GFrameCounter;
			P.Wait -= Dt;
			if (P.Wait > 0.f) return false;
			P.bWaiting = false;
		}
		Co.resume();
	}
	return false;
}

// ============================================================ menejer

ABozorNpcManager::ABozorNpcManager()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

ABozorNpcManager* ABozorNpcManager::Get(const UObject* WorldContext)
{
	UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!W) return nullptr;
	for (TActorIterator<ABozorNpcManager> It(W); It; ++It) return *It;
	return nullptr;
}

void ABozorNpcManager::BeginPlay()
{
	Super::BeginPlay();
	StartTime = GetWorld()->GetTimeSeconds();
	for (int32 i = 0; i < Actions.Num(); ++i)
	{
		if (Actions[i].Anim) ByCat.FindOrAdd(Actions[i].Category).Add(i);
	}
	QueueCustomers.SetNum(Queues.Num());
	Popularity.SetNum(Queues.Num());
	// mashhur rastaga ko'proq odam keladi va navbat hosil bo'ladi
	for (float& P : Popularity) P = FMath::FRand() < 0.15f ? FMath::FRandRange(2.f, 3.f) : FMath::FRandRange(0.4f, 1.2f);
	SeatOccupant.SetNum(Seats.Num());
	bAutoTest = FParse::Param(FCommandLine::Get(), TEXT("BozorAutoTest"));
	NextShot = StartTime + 12.0;
	EnsureNavMesh();
	UE_LOG(LogBozorNpc, Display, TEXT("[BozorNpc] harakat=%d navbat=%d o'rindiq=%d nuqta=%d+%d"), Actions.Num(), Queues.Num(), Seats.Num(), WanderPoints.Num(), GatePoints.Num());
}

void ABozorNpcManager::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bAutoTest) AutoTest(GetWorld()->GetTimeSeconds());
}

// Xaritada NavMesh bo'sh saqlangan bo'lsa (Dynamic rejimda o'yin uni o'zi qurmaydi), hech kim yurolmaydi: shu yerda quriladi
void ABozorNpcManager::EnsureNavMesh()
{
	UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!Nav) return;
	for (const FVector& P : WanderPoints)
	{
		FNavLocation L;
		if (Nav->ProjectPointToNavigation(P, L, FVector(100.f, 100.f, 300.f))) return;
	}
	UE_LOG(LogBozorNpc, Warning, TEXT("[BozorNpc] NavMesh bo'sh: o'yin boshida quriladi"));
	Nav->Build();
}

const FBozorNpcAction* ABozorNpcManager::Pick(FName Category) const
{
	const TArray<int32>* L = ByCat.Find(Category);
	return L && L->Num() > 0 ? &Actions[(*L)[FMath::RandRange(0, L->Num() - 1)]] : nullptr;
}

FVector ABozorNpcManager::RandomWanderPoint() const
{
	return WanderPoints.Num() > 0 ? WanderPoints[FMath::RandRange(0, WanderPoints.Num() - 1)] : GetActorLocation();
}

FVector ABozorNpcManager::RandomGatePoint() const
{
	return GatePoints.Num() > 0 ? GatePoints[FMath::RandRange(0, GatePoints.Num() - 1)] : RandomWanderPoint();
}

// UE GetBestQueueForCustomer: yaqin va qisqa navbat, biroz tasodif bilan. Except - hozirgina ketgan navbat:
// o'sha rastaga (ikkinchi sotuvchisiga ham) qaytib bormaydi
int32 ABozorNpcManager::BestQueue(const FVector& From, int32 Except) const
{
	int32 Best = INDEX_NONE;
	float BestScore = MAX_flt;
	for (int32 q = 0; q < Queues.Num(); ++q)
	{
		const FBozorQueueSpot& Q = Queues[q];
		if (q == Except || !Q.Vendor || Q.Points.Num() == 0 || QueueCustomers[q].Num() >= Q.Points.Num()) continue;
		if (Except != INDEX_NONE && (Q.Vendor == Queues[Except].Vendor || (Q.Stall >= 0 && Q.Stall == Queues[Except].Stall))) continue;
		// masofa metrda (Unity bilan bir xil vazn)
		const float Score = FVector::Dist(From, Q.Points[0]) * 0.01f * FMath::FRandRange(0.4f, 1.6f) / Popularity[q] + QueueCustomers[q].Num() * 2.5f + FMath::FRandRange(0.f, 4.f);
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = q;
		}
	}
	return Best;
}

int32 ABozorNpcManager::Join(int32 Q, ABozorNpc* C)
{
	TArray<TWeakObjectPtr<ABozorNpc>>& L = QueueCustomers[Q];
	L.RemoveAll([](const TWeakObjectPtr<ABozorNpc>& P) { return !P.IsValid(); });
	const int32 I = L.IndexOfByKey(C);
	if (I != INDEX_NONE) return I;
	if (L.Num() >= Queues[Q].Points.Num()) return -1;
	return L.Add(C);
}

// chiqib ketgan mijozdan keyingilar bir qadam oldinga siljiydi (UE: RefreshQueue)
void ABozorNpcManager::Leave(int32 Q, ABozorNpc* C)
{
	if (QueueCustomers.IsValidIndex(Q)) QueueCustomers[Q].Remove(C);
}

int32 ABozorNpcManager::IndexOf(int32 Q, const ABozorNpc* C) const
{
	return QueueCustomers.IsValidIndex(Q) ? QueueCustomers[Q].IndexOfByKey(C) : INDEX_NONE;
}

FVector ABozorNpcManager::Point(int32 Q, int32 I) const
{
	const TArray<FVector>& P = Queues[Q].Points;
	return P.Num() > 0 ? P[FMath::Clamp(I, 0, P.Num() - 1)] : GetActorLocation();
}

FVector ABozorNpcManager::LookAtLocation(int32 Q) const
{
	const FBozorQueueSpot& S = Queues[Q];
	return S.Vendor ? S.Vendor->GetActorLocation() : Point(Q, 0) + S.Forward * 100.f;
}

// hamrohlar navbat nuqtalarida turib qolmasin
bool ABozorNpcManager::NearAnyPoint(const FVector& P, float Radius) const
{
	for (const FBozorQueueSpot& Q : Queues)
	{
		for (const FVector& X : Q.Points)
		{
			if (FVector::DistSquared2D(X, P) < Radius * Radius) return true;
		}
	}
	return false;
}

int32 ABozorNpcManager::ReserveSeat(ABozorNpc* Npc, const FVector& From, float MaxDistance)
{
	int32 Best = INDEX_NONE;
	float Bd = MaxDistance;
	for (int32 i = 0; i < Seats.Num(); ++i)
	{
		if (SeatOccupant[i].IsValid()) continue;
		const float D = FVector::Dist2D(From, Seats[i].Position);
		if (D < Bd)
		{
			Bd = D;
			Best = i;
		}
	}
	if (Best != INDEX_NONE)
	{
		SeatOccupant[Best] = Npc;
		SitCount++;
	}
	return Best;
}

// ssenariy aniq o'rindiqni so'raydi: band bo'lsa - false
bool ABozorNpcManager::ReserveSeatIndex(int32 Seat, ABozorNpc* Npc)
{
	if (!SeatOccupant.IsValidIndex(Seat) || (SeatOccupant[Seat].IsValid() && SeatOccupant[Seat].Get() != Npc)) return false;
	SeatOccupant[Seat] = Npc;
	SitCount++;
	return true;
}

void ABozorNpcManager::ReleaseSeat(int32 Seat, ABozorNpc* Npc)
{
	if (SeatOccupant.IsValidIndex(Seat) && SeatOccupant[Seat].Get() == Npc) SeatOccupant[Seat] = nullptr;
}

// ============================================================ NPC

ABozorNpc::ABozorNpc(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UBozorNpcMesh>(ACharacter::MeshComponentName))
{
	PrimaryActorTick.bCanEverTick = true;
	AIControllerClass = ADetourCrowdAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	// burilishni harakat (yurganda) va TurnTo (turganda) boshqaradi, kontroller emas
	bUseControllerRotationYaw = false;
	GetCapsuleComponent()->InitCapsuleSize(34.f, 88.f);
	GetMesh()->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -88.f), FRotator(0.f, -90.f, 0.f));
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	UCharacterMovementComponent* M = GetCharacterMovement();
	M->bOrientRotationToMovement = true;
	M->RotationRate = FRotator(0.f, 360.f, 0.f);
	M->MaxWalkSpeed = WalkSpeed;
	// Unity agent.acceleration = 3 m/s2
	M->MaxAcceleration = 300.f;
	M->BrakingDecelerationWalking = 300.f;
	// AnimBP yurishni tezlanishdan biladi (ShouldMove)
	M->GetNavMovementProperties()->bUseAccelerationForPaths = true;
}

void ABozorNpc::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	LinkMetaHuman();   // muharrirda ham (o'yin boshlanmaganda) bitta gavda ko'rinsin
}

void ABozorNpc::LinkMetaHuman()
{
	TArray<USkeletalMeshComponent*> Parts;
	GetComponents(Parts);

	// Citizen garments and heads share the Citizen bind pose. Only the hidden
	// animation source uses the MetaHuman skeleton.
	const auto* CitizenPose = Parts.FindByPredicate([](const USkeletalMeshComponent* Part)
	{
		return Part->ComponentHasTag(TEXT("CitizenPose"));
	});
	if (CitizenPose)
	{
		// Unreal flattens leader chains. The retargeted mesh must evaluate its
		// own animation before the modular parts can follow it.
		(*CitizenPose)->SetLeaderPoseComponent(nullptr);
		for (USkeletalMeshComponent* Part : Parts)
		{
			if (Part->GetAttachParent() == *CitizenPose)
			{
				Part->SetLeaderPoseComponent(*CitizenPose);
			}
		}
		GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		GetMesh()->SetVisibility(false, false);
		return;
	}

	// MetaHuman: kiyim tana (Mesh) pozasiga ergashadi; yuzning o'z ABP'si (ABP_Face) bor - u pozani o'zi nusxalaydi,
	// soch/qosh groom'lari ham shunga bog'langan (leader pose qilinsa ko'rinmaydi)
	for (USkeletalMeshComponent* P : Parts)
	{
		if (P != GetMesh() && !P->GetAnimClass()) P->SetLeaderPoseComponent(GetMesh());
	}
	// MetaHuman BP'si child actor sifatida (yuz, soch, LODSync o'zida): uning tanasi NPC tanasi pozasiga ergashadi,
	// NPC'ning o'z tanasi esa yashiriladi (animatsiya unda ishlayveradi)
	TArray<UChildActorComponent*> Kids;
	GetComponents(Kids);
	for (UChildActorComponent* K : Kids)
	{
		AActor* MH = K ? K->GetChildActor() : nullptr;
		if (!MH) continue;
		TArray<USkeletalMeshComponent*> MHParts;
		MH->GetComponents(MHParts);
		for (USkeletalMeshComponent* P : MHParts)
		{
			if (!P->GetAnimClass()) P->SetLeaderPoseComponent(GetMesh());
		}
		GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		GetMesh()->SetVisibility(false, false);
	}
}

void ABozorNpc::BeginPlay()
{
	Super::BeginPlay();
	LinkMetaHuman();
	HomeRotation = GetActorQuat();
	// guruh eng sekin a'zosiga moslashadi: aks holda hamroh yetib ololmay qolardi
	if (NpcRole == EBozorRole::Hamroh && Leader) Leader->WalkSpeed = FMath::Min(Leader->WalkSpeed, MaxWalk * 0.85f);
	WalkSpeed = FMath::Clamp(WalkSpeed, MinWalk, MaxWalk);
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	if (NpcRole == EBozorRole::Sotuvchi || NpcRole == EBozorRole::Suhbatdosh)
	{
		// joyidan qimirlamaydi: boshqalar uni surib ketmasin
		GetCharacterMovement()->bOrientRotationToMovement = false;
	}
	// yuk yoki arava ustida paydo bo'lgan odam pastdagi yo'lakka tushiriladi
	const float Half = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	FVector Ground;
	if (NpcRole != EBozorRole::Sotuvchi && ProjectToNav(GetActorLocation() - FVector(0.f, 0.f, Half), Ground, 150.f)
		&& FMath::Abs(GetActorLocation().Z - Half - Ground.Z) > 20.f)
	{
		SetActorLocation(Ground + FVector(0.f, 0.f, Half + 2.f), false, nullptr, ETeleportType::TeleportPhysics);
	}
	if (NpcRole == EBozorRole::Hammol && bCart) AttachCart();
}

void ABozorNpc::EndPlay(const EEndPlayReason::Type Reason)
{
	Main.Reset();
	Super::EndPlay(Reason);
}

void ABozorNpc::StartRole()
{
	bStarted = true;
	// tasodifiy faza: yaqin NPC'lar bir xil harakatni bir vaqtda qilmasin (yurish - AnimBP, harakatlar - Do)
	Current = MoveState;
	if (NpcRole != EBozorRole::Sotuvchi && NpcRole != EBozorRole::Suhbatdosh) Mgr->Movers.Add(this);
	// qo'lda yozilgan ssenariy: yuradigan rollarda rol sikli o'rniga (sotuvchi, hamroh, suhbatdosh o'z sikllarida)
	const bool bScripted = Script.Num() > 0 && (NpcRole == EBozorRole::Xaridor || NpcRole == EBozorRole::Yolovchi || NpcRole == EBozorRole::Hammol);
	if (bScripted)
	{
		Main = ScriptLoop();
		return;
	}
	switch (NpcRole)
	{
	case EBozorRole::Sotuvchi: Main = VendorLoop(); break;
	case EBozorRole::Xaridor: Main = CustomerLoop(); break;
	case EBozorRole::Hamroh: Main = FollowerLoop(); break;
	case EBozorRole::Suhbatdosh: Main = TalkerLoop(); break;
	default: Main = WalkerLoop(); break;
	}
}

void ABozorNpc::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	CallCooldown -= DeltaSeconds;
	const double Now = GetWorld()->GetTimeSeconds();
	if (SettleAt > 0.0 && Now >= SettleAt) Settle();
	if (!bStarted)
	{
		Mgr = ABozorNpcManager::Get(this);
		if (Mgr && Mgr->HasActorBegunPlay() && (AI() || NpcRole == EBozorRole::Sotuvchi || NpcRole == EBozorRole::Suhbatdosh)) StartRole();
	}
	else if (Main.IsRunning())
	{
		Main.Step(DeltaSeconds);
	}
	if (Cart)
	{
		UpdateCart(DeltaSeconds);
	}
	else if (bTurning)
	{
		const FQuat Q = FMath::QInterpConstantTo(GetActorQuat(), TurnGoal, DeltaSeconds, FMath::DegreesToRadians(220.f));
		SetActorRotation(Q);
		if (Q.AngularDistance(TurnGoal) < FMath::DegreesToRadians(1.f))
		{
			SetActorRotation(TurnGoal);
			bTurning = false;
		}
	}
}

AAIController* ABozorNpc::AI() const
{
	return Cast<AAIController>(GetController());
}

bool ABozorNpc::ProjectToNav(const FVector& P, FVector& Out, float Extent) const
{
	UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	FNavLocation L;
	if (!Nav || !Nav->ProjectPointToNavigation(P, L, FVector(Extent, Extent, 200.f))) return false;
	Out = L.Location;
	return true;
}

// ================= animatsiya

// Faqat "Move" (tik turish/yurish - AnimBP'ning o'zi): o'ynayotgan harakat to'xtaydi. Allaqachon shu holatda bo'lsa hech narsa qilmaydi
void ABozorNpc::Play(FName State, float Fade, float Offset)
{
	if (State == Current) return;
	Current = State;
	SettleAt = 0.0;
	if (UAnimInstance* A = GetMesh()->GetAnimInstance()) A->StopSlotAnimation(Fade, SlotName);
}

// Kategoriyadan tasodifiy harakat; qaytaradi: davomiylik (sikl bo'lsa - [min, max] oralig'ida tasodifiy).
// Loop bo'lmagan klip tugagach montaj o'zi AnimBP'ga (tik turishga) qaytadi; o'tirganda - o'tirish sikliga
// NPC'ning o'z klipi (bir kategoriyada bir nechta bo'lsa - tasodifiy), bo'lmasa menejerning umumiy klipi
const FBozorNpcAction* ABozorNpc::PickAction(FName Category) const
{
	int32 Count = 0;
	for (const FBozorNpcAction& A : OwnActions) Count += A.Category == Category && A.Anim ? 1 : 0;
	if (Count > 0)
	{
		int32 K = FMath::RandRange(0, Count - 1);
		for (const FBozorNpcAction& A : OwnActions)
		{
			if (A.Category == Category && A.Anim && K-- == 0) return &A;
		}
	}
	return Mgr ? Mgr->Pick(Category) : nullptr;
}

float ABozorNpc::DoFirst(const TArray<FName>& Categories, float Min, float Max)
{
	for (const FName& C : Categories)
	{
		if (PickAction(C)) return Do(C, Min, Max);
	}
	return Categories.Num() > 0 ? Do(Categories.Last(), Min, Max) : Min;
}

float ABozorNpc::Do(FName Category, float Min, float Max)
{
	const FBozorNpcAction* A = PickAction(Category);
	UAnimInstance* Inst = GetMesh()->GetAnimInstance();
	if (!A || !Inst)
	{
		PlayMove();
		return Min;
	}
	Current = NAME_None;
	const float Len = A->Anim->GetPlayLength();
	// o'tirish va turish tana surilishi bilan bir vaqtda boshlanadi va tugaydi - boshidan o'ynaydi
	const bool bChained = Category == TEXT("S_Down") || Category == TEXT("S_Up");
	// loop bo'lmagan klip ham biroz tasodifiy joydan: bir vaqtda boshlagan ikki odam bir xil qimirlamasin
	const float Offset = A->bLoop ? FMath::FRandRange(0.f, Len * 0.7f) : bChained ? 0.f : FMath::FRandRange(0.f, FMath::Min(0.3f, Len * 0.1f));
	Inst->PlaySlotAnimationAsDynamicMontage(A->Anim, SlotName, 0.3f, 0.25f, 1.f, A->bLoop ? 1000 : 1, -1.f, Offset);
	SettleAt = 0.0;
	if (!A->bLoop && !bChained && Category.ToString().StartsWith(TEXT("S_")))
	{
		SettleAt = GetWorld()->GetTimeSeconds() + FMath::Max(Len - Offset - 0.25f, 0.1f);
		SettleCategory = TEXT("S_Loop");
	}
	return A->bLoop ? FMath::FRandRange(Min, Max) : FMath::Clamp(Len, Min, Max);
}

// delay soniyadan keyin Do (bir vaqtda boshlagan odamlar bir xil klipni bir xil fazada o'ynamasin)
void ABozorNpc::DoLater(FName Category, float Duration, float Delay)
{
	FTimerHandle H;
	GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [this, Category, Duration, Delay]()
	{
		Do(Category, Duration - Delay, Duration - Delay);
	}), FMath::Max(Delay, 0.01f), false);
}

void ABozorNpc::Settle()
{
	SettleAt = 0.0;
	if (!SettleCategory.IsNone()) Do(SettleCategory);
	else PlayMove();
}

void ABozorNpc::StopMoving()
{
	if (AAIController* C = AI()) C->StopMovement();
}

bool ABozorNpc::MoveRequest(const FVector& Target, float Speed)
{
	AAIController* C = AI();
	if (!C || bSitting) return false;
	GetCharacterMovement()->MaxWalkSpeed = Speed;
	return C->MoveToLocation(Target, 10.f, false, true, true, false, nullptr, true) != EPathFollowingRequestResult::Failed;
}

void ABozorNpc::StartWalking(const FVector& Target, float Speed)
{
	bTurning = false;
	PlayMove();
	MoveRequest(Target, Speed);
}

void ABozorNpc::TurnTowards(const FVector& Forward)
{
	const FVector F = Flat(Forward);
	if (F.SizeSquared() < 1e-4) return;
	TurnGoal = F.Rotation().Quaternion();
	bTurning = true;
}

// ================= sotuvchi

FBozorCo ABozorNpc::VendorLoop()
{
	co_await FBozorWait{ FMath::FRandRange(0.f, 2.f) };
	while (true)
	{
		// savdo xaridorning DealWith korutinida boradi; salomlashish/xayrlashish tugashini ham kutadi
		if (Deal.IsValid() || OyinchiSuhbat.IsValid() || GetWorld()->GetTimeSeconds() < BusyUntil)
		{
			co_await FBozorWait{ 0.f };
			continue;
		}
		if (CallCooldown <= 0.f && SomeoneInFront(350.f) && FMath::FRand() < 0.3f)
		{
			CallCooldown = FMath::FRandRange(8.f, 16.f);
			ABozorSotuvchiOvozi::Say(this, TEXT("chaqiriq"));
			co_await WaitOrDeal(Do(TEXT("V_Call"), 2.f, 4.f));
			continue;
		}
		const float R = FMath::FRand();
		co_await WaitOrDeal(Do(R < 0.5f ? TEXT("V_Idle") : R < 0.7f ? TEXT("V_Lean") : TEXT("V_Busy"), 5.f, 12.f));
	}
}

// harakat tugashini kutadi; xaridor kelsa (savdo yoki salomlashish) darhol to'xtaydi
FBozorCo ABozorNpc::WaitOrDeal(float T)
{
	while (T > 0.f && !Deal.IsValid() && !OyinchiSuhbat.IsValid() && GetWorld()->GetTimeSeconds() >= BusyUntil)
	{
		co_await FBozorWait{ 0.f };
		T -= GetWorld()->GetDeltaSeconds();
	}
}

bool ABozorNpc::SomeoneInFront(float Radius) const
{
	const FVector Fwd = HomeRotation.GetForwardVector();
	for (const TWeakObjectPtr<ABozorNpc>& M : Mgr->Movers)
	{
		if (!M.IsValid() || !M->IsMoving()) continue;
		const FVector D = Flat(M->GetActorLocation() - GetActorLocation());
		if (D.SizeSquared() < Radius * Radius && FVector::DotProduct(D.GetSafeNormal(), Fwd) > 0.3f) return true;
	}
	return false;
}

// xaridor rasta oldiga keldi: sotuvchi salomlashadi, keyin o'z ishiga qaytadi
void ABozorNpc::Greet()
{
	if (NpcRole != EBozorRole::Sotuvchi || Deal.IsValid() || OyinchiSuhbat.IsValid()) return;
	BusyUntil = GetWorld()->GetTimeSeconds() + Do(TEXT("V_Greet"), 1.5f, 2.5f);
	ABozorSotuvchiOvozi::Say(this, TEXT("salom"));
}

bool ABozorNpc::TryBeginDeal(ABozorNpc* Customer)
{
	if (NpcRole != EBozorRole::Sotuvchi || Deal.IsValid() || OyinchiSuhbat.IsValid()) return false;
	Deal = Customer;
	FaceTowards(Customer->GetActorLocation());
	return true;
}

bool ABozorNpc::OyinchiBilanBoshla(AActor* Oyinchi)
{
	if (NpcRole != EBozorRole::Sotuvchi || !Oyinchi || Deal.IsValid()) return false;
	OyinchiSuhbat = Oyinchi;
	FaceTowards(Oyinchi->GetActorLocation());
	BusyUntil = GetWorld()->GetTimeSeconds() + Do(TEXT("V_Greet"), 1.5f, 2.5f);
	ABozorSotuvchiOvozi::Say(this, TEXT("salom"));
	return true;
}

void ABozorNpc::OyinchiBilanTugat()
{
	if (!OyinchiSuhbat.IsValid()) return;
	OyinchiSuhbat = nullptr;
	BusyUntil = GetWorld()->GetTimeSeconds() + 1.5f;
	TurnTowards(HomeRotation.GetForwardVector());
}

// busy: xayrlashish harakati yana qancha o'ynashi kerak (shu payt VendorLoop uni kesmaydi)
void ABozorNpc::EndDeal(float Busy)
{
	Deal = nullptr;
	BusyUntil = GetWorld()->GetTimeSeconds() + Busy;
	TurnTowards(HomeRotation.GetForwardVector());
}

// ================= xaridor
// Rastani navbat orqali tanlaydi: oldi bo'sh bo'lsa to'g'ri boradi, aks holda navbatga turadi,
// oldinga siljib boradi va sabri tugasa boshqa rastaga ketadi.
FBozorCo ABozorNpc::CustomerLoop()
{
	co_await FBozorWait{ FMath::FRandRange(0.f, 4.f) };
	int32 Last = INDEX_NONE;
	while (true)
	{
		const int32 Q = Mgr->BestQueue(GetActorLocation(), Last);
		bool bReached = false;
		if (Q != INDEX_NONE) co_await VisitStall(Q, FMath::FRandRange(4.f, 10.f), -1, &bReached);
		if (!bReached)
		{
			co_await WalkTo(Mgr->RandomWanderPoint());
			continue;
		}
		Last = Q;
		if (FMath::FRand() < 0.2f) co_await WalkTo(Mgr->RandomWanderPoint());
	}
}

// Bitta rasta: navbatga turadi (oldi bo'sh bo'lsa to'g'ri boradi), oldinga siljiydi, sabri tugasa ketadi;
// yetib kelsa mol ko'radi (Browse s) va savdolashadi. ForceBuy: -1 tasodifiy (80% savdo x 75% xarid), 1 xarid, 0 savdolashib ketadi
FBozorCo ABozorNpc::VisitStall(int32 Q, float Browse, int32 ForceBuy, bool* bOutReached)
{
	if (bOutReached) *bOutReached = false;
	int32 Idx = Mgr->Queues.IsValidIndex(Q) ? Mgr->Join(Q, this) : -1;
	// ssenariy xaridori: rasta oldida 2 tadan ko'p odam bo'lsa o'tib ketadi (odamlar gavjum rastani tashlab ketadi;
	// tor yo'lakda navbat yo'lni to'sib, boshqalar 10 s gacha tiqilardi - 2026-11-09 kuzatuvi)
	const bool bScripted = ForceBuy >= 0;
	if (Idx < 0 || (bScripted && Idx >= 2))
	{
		if (Idx >= 0) Mgr->Leave(Q, this);
		co_await FBozorWait{ 0.f };
		co_return;
	}
	{
		Queue = Q;                                      // hamrohlar navbat nuqtalariga turmasin (FollowerLoop)
		const FBozorQueueSpot Spot = Mgr->Queues[Q];
		StallForward = Spot.Forward;
		// StartOnly: navbatga oxiridan kiradi
		if (Spot.bStartOnly && Idx > 0) co_await WalkTo(Mgr->EntryPoint(Q));

		// ssenariyda sabr qisqa: bitta rastada 70-99 s turib qolmasin
		float Patience = bScripted ? FMath::FRandRange(10.f, 20.f) : FMath::FRandRange(Spot.Patience.X, Spot.Patience.Y);
		bool bGaveUp = false;
		while (true)
		{
			Idx = Mgr->IndexOf(Q, this);
			const FVector P = Mgr->Point(Q, Idx);
			if (FVector::DistSquared2D(GetActorLocation(), P) > 15.f * 15.f) co_await WalkTo(P);
			co_await TurnTo(Idx == 0 ? Spot.Forward : Mgr->LookAtLocation(Q) - GetActorLocation());
			if (Idx == 0) break;
			// navbatda kutadi; oldingi odam ketsa siljiydi. bCanLeaveRandomly=false: sabr tugasa ham kutaveradi
			Do(TEXT("C_Wait"), 3.f, 6.f);
			while (Mgr->IndexOf(Q, this) == Idx && (Patience > 0.f || !Spot.bCanLeaveRandomly))
			{
				Patience -= 0.25f;
				co_await FBozorWait{ 0.25f };
			}
			if (Patience <= 0.f && Spot.bCanLeaveRandomly && Mgr->IndexOf(Q, this) == Idx)
			{
				bGaveUp = true;
				break;
			}
		}
		if (bGaveUp)
		{
			Mgr->Leave(Q, this);
			Queue = INDEX_NONE;
			co_return;
		}

		if (bOutReached) *bOutReached = true;
		bAtStall = true;
		ABozorNpc* Vendor = Spot.Vendor;
		if (FMath::FRand() < 0.4f) Vendor->Greet();      // sotuvchi salomlashadi
		// molni ko'radi: tanlab savatga soladi yoki shunchaki ko'zdan kechiradi
		co_await FBozorWait{ DoFirst({ FMath::FRand() < 0.5f ? FName(TEXT("C_Pick")) : FName(TEXT("C_Browse")), TEXT("C_Browse") }, Browse, Browse) };

		if (ForceBuy >= 0 || FMath::FRand() < 0.8f)
		{
			// sotuvchi boshqa joydagi xaridor bilan band bo'lsa, oldinda kutib turadi
			float Wait = FMath::Max(Patience, 8.f);
			if (!Vendor->TryBeginDeal(this)) Do(TEXT("C_Wait"), Wait, Wait);
			while (Wait > 0.f && !Vendor->TryBeginDeal(this) && Vendor->Deal.Get() != this)
			{
				Wait -= 0.25f;
				co_await FBozorWait{ 0.25f };
			}
			if (Vendor->Deal.Get() == this) co_await DealWith(Vendor, ForceBuy);
		}
		bAtStall = false;
		Mgr->Leave(Q, this);
		Queue = INDEX_NONE;
	}
}

// Savdo (npc_ssenariy_rejasi.md 1.2): salom -> narx so'rash -> sotuvchi molni ko'rsatadi -> savdolashish (xaridor kam
// taklif qiladi, sotuvchi bosh chayqaydi, xaridor o'ylanadi) -> ba'zan tatib ko'rishga beradi -> rozi bo'ladi -> tortish ->
// paket va pul (o'ng qo'l) -> ikkalasi ham rahmat; yoki "qimmat ekan" deb ketish.
// Yangi kategoriyalar (C_Greet, C_Ask, C_Haggle, C_Think, C_Taste, C_Thanks, V_Show, V_No, V_Yes, V_Offer) klipsiz bo'lsa eskisiga qaytadi
FBozorCo ABozorNpc::DealWith(ABozorNpc* V, int32 ForceBuy)
{
	Mgr->TradesStarted++;
	const bool bBuy = ForceBuy >= 0 ? ForceBuy == 1 : FMath::FRand() < 0.75f;
	// savdodan oldin salom-alik (o'zbek odobi)
	const float G = DoFirst({ TEXT("C_Greet"), TEXT("C_Talk") }, 1.5f, 2.5f);
	V->DoFirst({ TEXT("V_Greet"), TEXT("V_Listen") }, G, G);
	co_await FBozorWait{ G };
	// "Necha pul?" - sotuvchi molni ko'rsatib narxini aytadi
	const float A = DoFirst({ TEXT("C_Ask"), TEXT("C_Talk") }, 1.5f, 3.f);
	V->Do(TEXT("V_Listen"), A, A);
	co_await FBozorWait{ A };
	const float Sh = V->DoFirst({ TEXT("V_Show"), TEXT("V_Talk") }, 2.f, 3.5f);
	ABozorSotuvchiOvozi::Say(V, TEXT("savdo"), 0.6f);
	Do(TEXT("C_Listen"), Sh, Sh);
	co_await FBozorWait{ Sh };
	// savdolashish: xaridor tushirib so'raydi, sotuvchi oxirgi gapigacha "narxi shu" deb bosh chayqaydi
	const int32 Turns = FMath::RandRange(2, 4);
	for (int32 i = 0; i < Turns; ++i)
	{
		const float C = DoFirst({ TEXT("C_Haggle"), TEXT("C_Talk") }, 2.5f, 5.f);
		V->Do(TEXT("V_Listen"), C, C);
		co_await FBozorWait{ C };
		const bool bLast = i + 1 == Turns;
		const FName Answer = bLast && bBuy ? FName(TEXT("V_Yes")) : (FMath::FRand() < 0.5f ? FName(TEXT("V_No")) : FName(TEXT("V_Talk")));
		const float S = V->DoFirst({ Answer, TEXT("V_Talk") }, 2.f, 4.f);
		ABozorSotuvchiOvozi::Say(V, TEXT("savdo"), 0.6f);
		DoFirst({ i % 2 == 1 ? FName(TEXT("C_Think")) : FName(TEXT("C_Listen")), TEXT("C_Listen") }, S, S);
		co_await FBozorWait{ S };
		// "Yeb ko'ring" - sotuvchi tatib ko'rishga uzatadi (bir marta)
		if (i == 0 && FMath::FRand() < 0.35f)
		{
			const float T = V->DoFirst({ TEXT("V_Offer"), TEXT("V_Handout") }, 2.f, 3.f);
			DoFirst({ TEXT("C_Taste"), TEXT("C_Listen") }, T, T);
			co_await FBozorWait{ T };
		}
	}
	if (bBuy)
	{
		// qo'l balandligida, tananing oldida (aktor joyi - kapsula markazi, oyoqdan ~88 sm)
		const FVector VHand = V->GetActorLocation() + V->GetActorForwardVector() * 40.f + FVector(0.f, 0.f, 15.f);
		const FVector CHand = GetActorLocation() + GetActorForwardVector() * 35.f + FVector(0.f, 0.f, 15.f);
		const float W = V->Do(TEXT("V_Weigh"), 2.5f, 4.f);   // tarozida tortadi
		ABozorRastaOvozlari::PlayNear(this, { TEXT("tarozi_pip"), TEXT("tarozi_tosh") }, VHand, 300.f, TEXT("tarozi_pip"), V);
		Do(TEXT("C_Listen"), W, W);
		co_await FBozorWait{ W };
		const float H = V->Do(TEXT("V_Handout"), 2.f, 3.f);  // paketni uzatadi
		ABozorRastaOvozlari::Play(this, TEXT("paket"), VHand, V);
		Do(TEXT("C_Pay"), H, H);                               // pul beradi
		ABozorRastaOvozlari::Play(this, FMath::FRand() < 0.5f ? FName(TEXT("pul")) : FName(TEXT("tanga")), CHand, this);
		co_await FBozorWait{ H };
		TakeBag();
		Mgr->TradesBought++;
		const float Th = V->Do(TEXT("V_Thanks"), 1.5f, 2.5f); // "Bozoringizni bersin!" - "Yana keling!"
		ABozorSotuvchiOvozi::Say(V, TEXT("rahmat"));
		const float CTh = DoFirst({ TEXT("C_Thanks"), TEXT("C_Listen") }, 1.2f, 2.f);   // xaridor ham qo'lini ko'ksiga qo'yadi
		co_await FBozorWait{ FMath::Min(CTh, 1.6f) };
		V->EndDeal(FMath::Max(Th - 1.6f, 0.f));               // xaridor ketadi, sotuvchi ta'zimni oxirigacha qiladi
		co_return;
	}
	// "Qimmat ekan" - sotuvchi o'z narxida qoladi, xaridor indamay ketadi (keyin qaytishi mumkin)
	V->DoFirst({ TEXT("V_No"), TEXT("V_Listen") }, 1.5f, 2.5f);
	co_await FBozorWait{ Do(TEXT("C_Reject"), 1.5f, 3.f) };
	V->EndDeal();
}

void ABozorNpc::TakeBag()
{
	if (Bag || !Mgr || Mgr->BagMeshes.Num() == 0) return;
	UStaticMesh* M = Mgr->BagMeshes[FMath::RandRange(0, Mgr->BagMeshes.Num() - 1)];
	if (!M) return;
	Bag = NewObject<UStaticMeshComponent>(this);
	Bag->SetStaticMesh(M);
	Bag->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Bag->RegisterComponent();
	const float H = M->GetBoundingBox().GetSize().Z;
	if (H > 1.f) Bag->SetWorldScale3D(FVector(32.f / H));
	Bag->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("hand_r"));
	Bag->SetWorldRotation(GetActorRotation());
	Bag->SetWorldLocation(GetMesh()->GetSocketLocation(TEXT("hand_r")) - FVector(0.f, 0.f, 18.f));
}

// ================= hamroh
// Boshliq yurganda uning yonida yuradi. Tezlik = boshliq tezligi + orqada qolgan masofa: aks holda hamroh doim
// orqada qoladi va guruh qator bo'lib yuradi. Boshliq navbatda yoki rasta oldida tursa, navbat nuqtalariga
// tegmaydigan joyda (odatda uning orqasida, yo'lak tomonda) kutadi va vaqti-vaqti bilan harakatini almashtiradi.
FBozorCo ABozorNpc::FollowerLoop()
{
	co_await FBozorWait{ 0.f };
	const float Side = GroupSlot % 2 == 1 ? 1.f : -1.f;
	const float Row = static_cast<float>((GroupSlot - 1) / 2);
	double IdleUntil = 0.0, StuckUntil = 0.0, DetourUntil = 0.0, YieldCheck = 0.0, CrowdOff = 0.0;
	float BestTo = MAX_flt, NoProgress = 0.f, WalkStuck = 0.f, TouchT = 0.f;
	bool bWalking = false;
	FVector LastSlot = GetActorLocation(), Detour = FVector::ZeroVector, LastGoal(1e9);
	FVector Spot = FVector::ZeroVector, SpotFor(1e9);   // kutish joyi va u tanlangan paytdagi boshliq joyi
	while (Leader)
	{
		const double Now = GetWorld()->GetTimeSeconds();
		if (CrowdOff > 0.0 && Now >= CrowdOff)
		{
			SetCrowdSteering(AI(), true);
			CrowdOff = 0.0;
		}
		const FVector Lv = Flat(Leader->GetVelocity());
		// gisterezis: boshliq tiqilib 10-30 sm/s da tebransa, hamroh yurish va kutish orasida almashib turmasin.
		// Boshliq yo'lga chiqdi (hali tezlanmagan bo'lsa ham) - hamroh ham yuradi: aks holda boshliq yo'lida turgan
		// hamroh uni to'sib qo'yardi, boshliq esa tezlana olmasdi (2026-11-09: 8-20 s qotish)
		const AAIController* LeaderAI = Leader->AI();
		const bool bLeaderGoing = LeaderAI && LeaderAI->GetMoveStatus() == EPathFollowingStatus::Moving;
		bWalking = bLeaderGoing || (bWalking ? Lv.Size() > 15.f : Lv.Size() > 35.f);
		const float Step = bWalking ? 0.1f : 0.25f;
		FVector Slot;
		if (bWalking)
		{
			const FVector Lp = Leader->GetActorLocation();
			Slot = Lp + Leader->GetActorRightVector() * Side * 60.f - Leader->GetActorForwardVector() * (15.f + Row * 60.f);
			// boshliq endi yo'lga chiqyapti (hali sekin, tanasi yo'lga burilmagan): yondagi joy uning yo'li ustida bo'lishi
			// mumkin - ikkalasi bir-birini itarib qotardi (2026-11-09: 10 s). Hamroh yo'lda tursa chetga chiqadi, aks holda orqasiga o'tadi
			const FVector Pd = Lv.Size() < 35.f ? Leader->PathDir(Lp) : FVector::ZeroVector;
			if (!Pd.IsNearlyZero())
			{
				const FVector Pr = FVector::CrossProduct(FVector::UpVector, Pd);
				const FVector Rel = Flat(GetActorLocation() - Lp);
				const float Away = FVector::DotProduct(Rel, Pr) >= 0.f ? 1.f : -1.f;
				Slot = Rel.Size() < 130.f && FVector::DotProduct(Rel.GetSafeNormal(), Pd) > 0.3f
					? Lp + Pr * Away * 110.f
					: Lp - Pd * (70.f + Row * 60.f) + Pr * Away * 45.f;
			}
			FVector H;
			if (ProjectToNav(Slot, H, 80.f)) Slot = H;
			SpotFor = FVector(1e9);
		}
		else
		{
			// kutish joyi boshliq to'xtaganda bir marta tanlanadi (navbatda bo'lmasa ham): har tikda boshliq burilishidan
			// qayta hisoblansa, joy boshliq atrofida aylanib, hamroh uning ortidan aylanardi (2026-11-09 kuzatuvi)
			if (FVector::DistSquared2D(Leader->GetActorLocation(), SpotFor) > 30.f * 30.f)
			{
				Spot = WaitSpot(Side, Row);
				SpotFor = Leader->GetActorLocation();
			}
			Slot = Spot;
		}
		const FVector Here = GetActorLocation();
		const float To = FVector::Dist2D(Slot, Here);
		// joyiga yetib bo'lmayapti: turgan joyida kutadi (boshliq yonida 2.5 m, lekin 1.5 s yaqinlasha olmadi)
		if (bWalking || FVector::DistSquared2D(Slot, LastSlot) > 50.f * 50.f)
		{
			LastSlot = Slot;
			BestTo = MAX_flt;
			NoProgress = 0.f;
			StuckUntil = 0.0;
		}
		if (To < BestTo - 10.f)
		{
			BestTo = To;
			NoProgress = 0.f;
		}
		else
		{
			NoProgress += Step;
		}
		// boshliqqa tiralib turibdi: uning atrofida aylanib yurmaydi, tezroq turgan joyida qoladi
		const bool bTouching = FVector::Dist2D(Here, Leader->GetActorLocation()) < 85.f;
		// tegib turgan holda sekin yaqinlashsa ham (boshliq atrofida aylanib) - 1 s dan keyin to'xtaydi
		TouchT = !bWalking && bTouching ? TouchT + Step : 0.f;
		if (!bWalking && To < 250.f && (NoProgress > (bTouching ? 0.5f : 1.5f) || TouchT > 1.f)) StuckUntil = DBL_MAX;
		const bool bUnreachable = !bWalking && Now < StuckUntil;
		FVector Goal = Slot;
		float Speed = 0.f;
		if (bWalking)
		{
			// yo'lni boshliq ketayotgan tomonga oldindan olamiz; tezlikni esa hozirgi joydan orqada qolishga qarab
			const float Lag = FVector::DotProduct(Slot - Here, Lv.GetSafeNormal());
			const float Lo = FMath::Max(Lv.Size() * 0.6f, MinWalk);
			Goal = Slot + Lv * 0.4f;
			Speed = SafeClamp(Lv.Size() + 0.9f * Lag, Lo, FMath::Max(MinWalk, FMath::Min(Leader->WalkSpeed * 1.5f, MaxWalk)));
		}
		else if (To > 30.f && !bUnreachable)
		{
			Speed = SafeClamp(50.f + 0.6f * To, MinWalk, FMath::Max(MinWalk, FMath::Min(Leader->WalkSpeed * 1.3f, MaxWalk)));
		}
		if (Speed > 0.f)
		{
			// tik turgan odamlarga tiraldi (boshliq ketib qolardi, 2026-11-09: 9 s, 8 m): yonidan aylanib o'tadi;
			// oldida hech kim bo'lmasa - joy yetib bo'lmaydigan joyda, boshliqning o'ziga yo'l so'raydi
			WalkStuck = IsMoving() ? 0.f : WalkStuck + Step;
			if (Now < DetourUntil && FVector::DistSquared2D(Here, Detour) > 30.f * 30.f)
			{
				// aylanib o'tish nuqtasiga ketyapti
			}
			else if (WalkStuck > 0.8f)
			{
				WalkStuck = 0.f;
				LastGoal = FVector(1e9);   // aylanib o'tgach asosiy manzil qayta so'raladi
				const FVector Dir = PathDir(Goal);
				const ABozorNpc* B = BlockerAhead(Dir, 110.f);
				const bool bFar = FVector::DistSquared2D(Here, Leader->GetActorLocation()) > 600.f * 600.f;
				if (B && B != Leader.Get() && DetourPoint(B, Dir, Detour))
				{
					StartWalking(Detour, Speed);
					DetourUntil = Now + 1.5;
				}
				else if ((!B || bFar) && bWalking)
				{
					// boshliqdan uzoqlashib qoldi (6 m+) yoki oldida hech kim yo'q: to'g'ri boshlig'ining yoniga yo'l so'raydi
					Detour = Leader->GetActorLocation();
					if (bFar && CrowdOff <= 0.0)
					{
						SetCrowdSteering(AI(), false);
						CrowdOff = Now + 1.5;
					}
					StartWalking(Detour, Speed);
					DetourUntil = Now + 1.0;
				}
				else
				{
					// aylanib o'tib bo'lmadi (tor yo'lakda navbat, o'z guruhdoshi) yoki oldida hech kim yo'q - NavMesh chetida
					// crowd tutib qolgan bo'lishi mumkin: crowd steering vaqtincha o'chadi (2026-11-09: hamroh 150 s qotgan)
					if (B != Leader.Get() && CrowdOff <= 0.0)
					{
						SetCrowdSteering(AI(), false);
						CrowdOff = Now + 1.5;
					}
					StartWalking(Goal, Speed);
				}
			}
			else
			{
				DetourUntil = 0.0;
				// yo'l har 0.1 s da emas, faqat manzil 30 sm dan ko'p surilganda yoki yo'l tugaganda qayta so'raladi
				// (har so'rov crowd yo'lini qaytadan boshlaydi); tezlik esa har tikda yangilanadi
				const AAIController* MyAI = AI();
				if (FVector::DistSquared2D(Goal, LastGoal) > 30.f * 30.f || !MyAI || MyAI->GetMoveStatus() != EPathFollowingStatus::Moving)
				{
					StartWalking(Goal, Speed);
					LastGoal = Goal;
				}
				else
				{
					GetCharacterMovement()->MaxWalkSpeed = Speed;
				}
			}
			IdleUntil = 0.0;
		}
		else
		{
			WalkStuck = 0.f;
			DetourUntil = 0.0;
			LastGoal = FVector(1e9);
			// tezlikka emas, AI holatiga qaraladi: tiralgan hamroh tezligi 0, lekin yo'l so'rovi faol qolib, boshliqni itarardi
			const AAIController* MyAI = AI();
			if (MyAI && MyAI->GetMoveStatus() != EPathFollowingStatus::Idle) StopMoving();
			// begona yo'lovchi unga tiralib o'tolmayapti (tor yo'lakda guruh orasida qisilib 12 s turgan, 2026-11-09):
			// hamroh uning yo'lidan bir qadam chetga chiqadi va o'sha yerda kutadi
			if (Now >= YieldCheck)
			{
				YieldCheck = Now + 0.5;
				for (const TWeakObjectPtr<ABozorNpc>& M : Mgr->Movers)
				{
					const ABozorNpc* N = M.Get();
					if (!N || N == this || N == Leader.Get() || N->Leader.Get() == Leader.Get() || N->IsMoving()) continue;
					const FVector Rel = Flat(Here - N->GetActorLocation());
					if (Rel.SizeSquared() > 95.f * 95.f) continue;
					const AAIController* NA = N->AI();
					if (!NA || NA->GetMoveStatus() != EPathFollowingStatus::Moving) continue;
					const FVector Nd = N->PathDir(N->GetActorLocation());
					if (Nd.IsNearlyZero() || FVector::DotProduct(Rel.GetSafeNormal(), Nd) < 0.4f) continue;
					const FVector Pr = FVector::CrossProduct(FVector::UpVector, Nd);
					FVector H;
					if (ProjectToNav(Here + Pr * (FVector::DotProduct(Rel, Pr) >= 0.f ? 1.f : -1.f) * 70.f, H, 30.f))
					{
						Spot = H;
						LastSlot = H;
						StuckUntil = 0.0;
						BestTo = MAX_flt;
						NoProgress = 0.f;
						TouchT = 0.f;
					}
					break;
				}
			}
			if (Now >= IdleUntil)
			{
				TurnTowards(Leader->Queue != INDEX_NONE && FMath::FRand() < 0.6f ? Leader->StallForward : Leader->GetActorLocation() - GetActorLocation());
				IdleUntil = Now + Do(TEXT("F_Idle"), 4.f, 9.f);
			}
		}
		co_await FBozorWait{ Step };
	}
}

// Boshliq to'xtadi (navbatda / rasta oldida yoki shunchaki): navbat nuqtalaridan uzoqroq, hamrohga eng yaqin joy.
// Hozir turgan joyi yaroqli bo'lsa - qimirlamaydi (boshliqning narigi tomoniga uni aylanib o'tmaydi)
FVector ABozorNpc::WaitSpot(float Side, float Row) const
{
	const bool bStall = Leader->Queue != INDEX_NONE;
	const FVector F = Flat(bStall ? Leader->StallForward : Leader->GetActorForwardVector()).GetSafeNormal();
	const FVector R = FVector::CrossProduct(FVector::UpVector, F);
	const FVector Lp = Leader->GetActorLocation(), Here = GetActorLocation();
	const FVector Rel = Flat(Here - Lp);
	const float D = Rel.Size();
	// rastada: boshliq bilan peshtaxta orasida (oldida) turmaydi
	if (D > 60.f && D < 170.f + Row * 55.f && FVector::DotProduct(Rel / D, F) < (bStall ? 0.2f : 0.9f) && !Mgr->NearAnyPoint(Here, 60.f))
	{
		return Here;
	}
	const float Back = 85.f + Row * 55.f, Wide = 85.f * (1.f + Row);
	const FVector Offsets[] = { -F * Back + R * Side * 35.f, -F * Back - R * Side * 35.f, R * Side * Wide - F * 30.f, -R * Side * Wide - F * 30.f };
	FVector Best = Lp + Offsets[0];
	float Bd = MAX_flt;
	for (const FVector& O : Offsets)
	{
		FVector H;
		if (!ProjectToNav(Lp + O, H, 30.f) || Mgr->NearAnyPoint(H, 60.f)) continue;
		const float Dh = FVector::DistSquared2D(H, Here);
		if (Dh < Bd)
		{
			Bd = Dh;
			Best = H;
		}
	}
	return Best;
}

// ================= yo'lovchi va hammol

FBozorCo ABozorNpc::WalkerLoop()
{
	co_await FBozorWait{ FMath::FRandRange(0.f, 2.f) };
	while (true)
	{
		co_await WalkTo(NextWalkTarget());
		if (NpcRole != EBozorRole::Yolovchi) continue;
		const float R = FMath::FRand();
		if (R < 0.4f) co_await SitDown();                                                  // choyxonada bir oz o'tiradi
		else if (R < 0.55f) co_await FBozorWait{ Do(TEXT("P_Pause"), 3.f, 7.f) };          // telefon, choy, o'ylanish
	}
}

// Keyingi manzil: kamida 3 m narida. Hammolga - iloji bo'lsa oldinda (arava ortga yurmasin)
FVector ABozorNpc::NextWalkTarget() const
{
	const bool bAvoid = GetWorld()->GetTimeSeconds() < AvoidUntil;
	for (int32 i = 0; i < 8; ++i)
	{
		const FVector P = FMath::FRand() < 0.3f ? Mgr->RandomGatePoint() : Mgr->RandomWanderPoint();
		const FVector To = Flat(P - GetActorLocation());
		if (To.SizeSquared() < 300.f * 300.f) continue;
		if (bAvoid && i < 6 && FVector::DotProduct(To.GetSafeNormal(), AvoidDir) > 0.6f) continue;
		if (Cart && i < 6 && FVector::DotProduct(To.GetSafeNormal(), GetActorForwardVector()) < -0.2f) continue;
		return P;
	}
	return FMath::FRand() < 0.3f ? Mgr->RandomGatePoint() : Mgr->RandomWanderPoint();
}

// Bo'sh o'rindiqqa o'tirish: o'tiradi -> 15-40 s -> turadi
FBozorCo ABozorNpc::SitDown()
{
	const int32 Seat = Mgr->ReserveSeat(this, GetActorLocation(), 3000.f);
	if (Seat == INDEX_NONE) co_return;
	co_await SitAt(Seat, FMath::FRandRange(15.f, 40.f));
}

// Aniq o'rindiqqa o'tirish (ssenariy): band bo'lsa o'tirmaydi; choyxonada choy ichadi yoki shunchaki o'tiradi
FBozorCo ABozorNpc::SitAt(int32 Seat, float Time)
{
	if (!Mgr->ReserveSeatIndex(Seat, this))
	{
		co_await FBozorWait{ 0.f };
		co_return;
	}
	const FBozorSeat S = Mgr->Seats[Seat];
	const FVector Front = S.Position + S.Forward * 45.f;
	co_await WalkTo(Front);
	// yetib kelmadi (yo'l yo'q, vaqt tugadi): o'rindiqqa sakramaydi, boshqa ishiga qaytadi
	if (FVector::DistSquared2D(GetActorLocation(), Front) > 35.f * 35.f)
	{
		Mgr->ReleaseSeat(Seat, this);
		co_return;
	}
	co_await TurnTo(S.Forward);
	// klipda o'tirish orqaga siljish bilan berilgan, root motion yo'q: tanani o'tirish davomida o'rindiq ustiga silliq suramiz.
	// O'rindiq NavMesh'da emas: o'tirgan paytda harakat va to'qnashuv o'chiq
	bSitting = true;
	StopMoving();
	GetCharacterMovement()->DisableMovement();
	SetActorEnableCollision(false);
	const FVector Here = GetActorLocation();
	FVector On = S.Position + S.Forward * 12.f;
	On.Z = Here.Z;
	FVector Back = Front;
	Back.Z = Here.Z;
	co_await Slide(Here, On, Do(TEXT("S_Down"), 1.2f, 2.5f));
	float T = Time;
	while (T > 0.f)
	{
		const float D = Do(FMath::FRand() < 0.6f ? TEXT("S_Loop") : TEXT("S_Drink"), 5.f, 10.f);
		co_await FBozorWait{ FMath::Min(D, T) };
		T -= D;
	}
	co_await Slide(On, Back, Do(TEXT("S_Up"), 1.2f, 2.5f));
	SettleCategory = NAME_None;
	SetActorEnableCollision(true);
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	bSitting = false;
	Mgr->ReleaseSeat(Seat, this);
}

FBozorCo ABozorNpc::Slide(FVector From, FVector To, float Time)
{
	float T = 0.f;
	while (T < Time)
	{
		SetActorLocation(FMath::Lerp(From, To, FMath::SmoothStep(0.f, 1.f, T / Time)));
		co_await FBozorWait{ 0.f };
		T += GetWorld()->GetDeltaSeconds();
	}
	SetActorLocation(To);
}

float ABozorNpc::CapsuleHalf() const
{
	return GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
}

void ABozorNpc::AttachCart()
{
	if (!Mgr) Mgr = ABozorNpcManager::Get(this);
	UStaticMesh* M = Mgr ? Mgr->CartMesh.Get() : nullptr;
	if (!M) return;
	Cart = NewObject<UStaticMeshComponent>(this);
	Cart->SetStaticMesh(M);
	Cart->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Cart->SetCanEverAffectNavigation(false);
	Cart->SetupAttachment(GetRootComponent());
	Cart->SetUsingAbsoluteLocation(true);
	Cart->SetUsingAbsoluteRotation(true);
	Cart->SetUsingAbsoluteScale(true);
	Cart->RegisterComponent();
	// modelning pivoti g'ildirakli uchida, uzunligi -Y bo'ylab, dasta -Y uchida: +Y arava oldiga (g'ildiraklar oldinda,
	// dasta hammolga). Arava o'qlarida: X = model Y, Y = -model X
	const FBox B = M->GetBoundingBox();
	CartTurn = FQuat(FVector::UpVector, FMath::DegreesToRadians(-90.f));
	const float Length = FMath::Max(B.Max.Y - B.Min.Y, 1.f);
	// model 1.92 m: bozor aravachasi ~1.5 m (2-3 m li yo'laklarda katta arava qop va odamlarga tegmay yurolmasdi)
	CartScale = FMath::Min(1.f, 150.f / Length);
	const float S = CartScale, L = Length * S, H = (B.Max.Z - B.Min.Z) * S, CenterY = -(B.Min.X + B.Max.X) * 0.5f * S;
	// SM_Aravakash_Arava geometriyasi (Blender'da o'lchangan, 2026-10-21; uzunlik 1.92, balandlik 0.77, en 0.92 m):
	// g'ildirak o'qi g'ildirakli uchdan 23% da, r = balandlikning 39%; qora dasta qarama-qarshi uchda,
	// 97% balandlikda, eni +-60% (yarim enining)
	CartAxle = FVector(B.Max.Y * S - L * 0.23f, CenterY, B.Min.Z * S + H * 0.39f);
	CartGrip = FVector(B.Min.Y * S + 2.f, CenterY, B.Min.Z * S + H * 0.97f);
	CartGripHalf = (B.Max.X - B.Min.X) * 0.5f * S * 0.6f;
	// dastani qo'l balandligiga ko'taradi: arava g'ildirak o'qi atrofida egiladi, tirgovichi yerdan uziladi
	const float Hip = 95.f * GetMesh()->GetRelativeScale3D().Z;
	const float GripZ = Hip * 0.92f;
	CartReach = Hip * 0.45f;
	const FVector G = CartGrip - CartAxle;                              // (-x, 0, +z): dasta o'qdan orqada va yuqorida
	const float D = FVector2D(G.X, G.Z).Size(), Rest = FMath::Atan2(G.Z, -G.X);
	const float Lifted = FMath::Asin(FMath::Clamp((GripZ - CartAxle.Z) / D, -1.f, 1.f));
	const float Tilt = FMath::Max(0.f, Lifted - Rest);
	CartGripDist = D * FMath::Cos(Rest + Tilt);
	// egilish ishorasi: dastani ko'taradigan tomon
	const FQuat TiltA(FVector::RightVector, Tilt), TiltB(FVector::RightVector, -Tilt);
	CartTilt = TiltA.RotateVector(G).Z >= TiltB.RotateVector(G).Z ? TiltA : TiltB;
	// kapsula (agent) endi g'ildirak o'qi: hammolning tanasini UpdateCart o'zi joylaydi
	const float Half = CapsuleHalf();
	const FVector Feet = GetActorLocation() - FVector(0.f, 0.f, Half);
	CartAxis = Flat(GetActorForwardVector()).GetSafeNormal();
	CartHitch = Feet + CartAxis * CartReach;
	FVector Axle = CartHitch + CartAxis * CartGripDist, Hit;
	if (ProjectToNav(Axle, Hit, 100.f)) Axle = FVector(Hit.X, Hit.Y, Feet.Z);
	BodyPos = Feet;
	BodyRot = GetActorQuat();
	GetCharacterMovement()->bOrientRotationToMovement = false;
	SetActorLocation(Axle + FVector(0.f, 0.f, Half), false, nullptr, ETeleportType::TeleportPhysics);
	// tana va arava harakatdan keyin joylanadi, qo'l IK esa shundan keyin baholanadi
	AddTickPrerequisiteComponent(GetCharacterMovement());
	GetMesh()->PrimaryComponentTick.AddPrerequisite(this, PrimaryActorTick);
	if (UBozorNpcMesh* Body = Cast<UBozorNpcMesh>(GetMesh())) Body->bHandIK = true;
	UpdateCart(0.f);
}

// Tirkama (Unity LateUpdate): dasta osti (hitch) g'ildirak o'qidan doimiy masofada, uning orqasidan ergashadi. Arava o'qi -
// hitch'dan g'ildiraklarga; hammol dastaning orqasida, unga qarab turadi
void ABozorNpc::UpdateCart(float Dt)
{
	const FVector Axle = GetActorLocation() - FVector(0.f, 0.f, CapsuleHalf());
	const FVector PrevBody = BodyPos;
	AAIController* C = AI();
	UPathFollowingComponent* PF = C ? C->GetPathFollowingComponent() : nullptr;
	const bool bPath = PF && PF->GetStatus() != EPathFollowingStatus::Idle;
	// Manzil orqada qolgan (yo'l aravaning orqasiga ketadi): arava orqaga yurmaydi - aks holda hammol orqasi bilan yurib,
	// aravani tortib ketardi. G'ildirak o'qi joyida turadi, hammol dastadan ushlab yoy bo'ylab aylanib, aravani buradi
	if (bPath)
	{
		const FVector Want = Flat(SteeringTarget(PF, Axle) - Axle);
		const float Off = Want.SizeSquared() > 1.f ? FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Want.GetSafeNormal(), CartAxis), -1.f, 1.f))) : 0.f;
		// ikki marta ketma-ket burib bo'lmasa (tor joy, hamma yo'l orqada) - oxirgi chora sifatida orqaga yuradi
		if (!bCartTurning && Off > 100.f && CartTurnFails < 2)
		{
			bCartTurning = true;
			CartTurns++;
			PF->PauseMove();
			CartTurnSign = FVector::CrossProduct(CartAxis, Want).Z >= 0.f ? 1.f : -1.f;
			CartTurnBest = Off;
			CartTurnStill = 0.f;
			CartTurnTries = 0;
		}
		if (bCartTurning)
		{
			CartHitch = Axle - FQuat(FVector::UpVector, FMath::DegreesToRadians(CartTurnSign * 35.f * Dt)).RotateVector(CartAxis) * CartGripDist;
			if (Off < 45.f)
			{
				bCartTurning = false;
				CartTurnFails = 0;
				PF->ResumeMove();
			}
			// yoy yo'lida rasta yoki odam bor (NavMesh hammolni qaytarib turibdi): 1.5 s burchak kamaymasa - teskari tomonga,
			// u ham bo'lmasa - bu manzildan voz kechadi. Aks holda hammol abadiy joyida qotib turardi
			else if (Off < CartTurnBest - 5.f)
			{
				CartTurnBest = Off;
				CartTurnStill = 0.f;
			}
			else if ((CartTurnStill += Dt) > 1.5f)
			{
				if (++CartTurnTries == 1)
				{
					CartTurnSign = -CartTurnSign;
					CartTurnBest = Off;
					CartTurnStill = 0.f;
				}
				else
				{
					bCartTurning = false;
					bCartTurnFailed = true;
					CartTurnFails++;
					CartGiveUps++;
					PF->ResumeMove();
				}
			}
		}
	}
	else
	{
		bCartTurning = false;
	}
	// tashxis: g'ildirak o'qi aravaning orqasiga yuryapti
	if (Dt > 0.f)
	{
		CartFrames++;
		CartBackFrames += FVector::DotProduct(Flat(GetVelocity()), CartAxis) < -20.f ? 1 : 0;
	}
	const FVector Back = Flat(CartHitch - Axle).SizeSquared() > 1.f ? Flat(CartHitch - Axle) : -CartAxis;
	FVector Axis = -Back.GetSafeNormal();
	FVector Body = Axle - Axis * (CartGripDist + CartReach);
	// hammol yuriladigan joyda (NavMesh) turadi: burilishda tashqariga surilib rasta ichiga kirib ketmasin
	FVector Hit;
	if (ProjectToNav(Body, Hit, 60.f))
	{
		const FVector A = Flat(Axle - Hit);
		Axis = A.SizeSquared() > 1.f ? A.GetSafeNormal() : -Back.GetSafeNormal();
		Body = Axle - Axis * (CartGripDist + CartReach);
	}
	Body.Z = Axle.Z;
	CartAxis = Axis;
	CartHitch = Axle - Axis * CartGripDist;
	// Odatda hammol aravaga qaraydi. Aravani aylantirayotganda yon tomonga yuradi - tanasi yurish yo'nalishi bilan arava
	// o'rtasiga buriladi. Arava ortga yursa (oxirgi chora) - o'girilib, aravani orqasidan tortib boradi (orqasi bilan yurmaydi)
	FVector Face = Axis;
	const FVector Step = Flat(Body - PrevBody);
	if (Dt > 0.f && Step.Size() > 20.f * Dt)
	{
		const FVector Sd = Step.GetSafeNormal();
		if (FVector::DotProduct(Sd, Axis) < -0.3f) Face = Sd;
		else if (bCartTurning && !(Axis + Sd).IsNearlyZero()) Face = (Axis + Sd).GetSafeNormal();
	}
	const FQuat FaceRot = Face.Rotation().Quaternion();
	BodyRot = Dt > 0.f ? FQuat::Slerp(BodyRot, FaceRot, FMath::Min(1.f, 6.f * Dt)) : FaceRot;
	BodyPos = Body;
	SetActorRotation(Axis.Rotation());
	GetMesh()->SetWorldLocationAndRotation(Body, BodyRot * FRotator(0.f, -90.f, 0.f).Quaternion());
	PlaceCart(Axle);
	// og'ir arava: keskin burilishda sekinlashadi (g'ildirak yonga sirpanmaydi - hammol aravani burib olishi kerak),
	// lekin yurish klipi ko'rsata oladigan tezlikdan sekin emas - aks holda oyoq sirpanadi
	if (bPath && !bCartTurning)
	{
		const FVector V = Flat(SteeringTarget(PF, Axle) - Axle);
		const float Turn = V.SizeSquared() > 1.f ? FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(V.GetSafeNormal(), Axis), -1.f, 1.f))) : 0.f;
		GetCharacterMovement()->MaxWalkSpeed = FMath::Max(MinWalk, WalkSpeed * FMath::Lerp(1.f, 0.5f, FMath::Clamp(Turn / 90.f, 0.f, 1.f)));
	}
}

// Unity agent.steeringTarget: yo'lning keyingi burilish nuqtasi (crowd rejimida PathFollowing faqat oxirgi nuqtani beradi)
FVector ABozorNpc::SteeringTarget(const UPathFollowingComponent* PF, const FVector& From) const
{
	const FNavPathSharedPtr Path = PF->GetPath();
	if (!Path.IsValid() || Path->GetPathPoints().Num() < 2) return PF->GetCurrentTargetLocation();
	const TArray<FNavPathPoint>& P = Path->GetPathPoints();
	// eng yaqin bo'lak, uning oxiri; unga juda yaqin bo'lsa - keyingisi
	int32 Best = 0;
	float BestD = MAX_flt;
	for (int32 i = 0; i + 1 < P.Num(); ++i)
	{
		const float D = FMath::PointDistToSegmentSquared(Flat(From), Flat(P[i].Location), Flat(P[i + 1].Location));
		if (D < BestD)
		{
			BestD = D;
			Best = i;
		}
	}
	int32 Next = Best + 1;
	if (Next + 1 < P.Num() && FVector::DistSquared2D(From, P[Next].Location) < 30.f * 30.f) Next++;
	return P[Next].Location;
}

// g'ildirak o'qi agent nuqtasida (yer + r), dastasi ko'tarilgan; qo'llar dastaning ikki chetida
void ABozorNpc::PlaceCart(const FVector& AxleGround)
{
	const FQuat Rot = CartAxis.Rotation().Quaternion() * CartTilt;
	const FVector Pivot = AxleGround + FVector(0.f, 0.f, CartAxle.Z) - Rot.RotateVector(CartAxle);
	Cart->SetWorldTransform(FTransform(Rot * CartTurn, Pivot, FVector(CartScale)));
	if (UBozorNpcMesh* Body = Cast<UBozorNpcMesh>(GetMesh()))
	{
		const FVector Grip = Pivot + Rot.RotateVector(CartGrip);
		const FVector Side = FVector::CrossProduct(FVector::UpVector, CartAxis) * CartGripHalf;
		Body->HandTarget[0] = Grip - Side;
		Body->HandTarget[1] = Grip + Side;
	}
}

// ================= qo'l IK

void UBozorNpcMesh::FinalizeBoneTransform()
{
	if (bHandIK) ApplyHandIK();
	Super::FinalizeBoneTransform();
}

// Yangi baholangan pozada yelka-tirsak-bilak zanjiri qo'lni nishonga yetkazadi (tirsak o'z tomoniga bukiladi);
// barmoqlar va boshqa bolalar suyaklari o'z lokal holatini saqlab ergashadi
void UBozorNpcMesh::ApplyHandIK()
{
	TArray<FTransform>& Cs = GetEditableComponentSpaceTransforms();
	const USkeletalMesh* M = GetSkeletalMeshAsset();
	if (!M || Cs.Num() == 0) return;
	const FReferenceSkeleton& Ref = M->GetRefSkeleton();
	static const FName Bones[2][3] = { { TEXT("upperarm_l"), TEXT("lowerarm_l"), TEXT("hand_l") }, { TEXT("upperarm_r"), TEXT("lowerarm_r"), TEXT("hand_r") } };
	const FTransform& ToWorld = GetComponentTransform();
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const int32 U = GetBoneIndex(Bones[Side][0]), L = GetBoneIndex(Bones[Side][1]), H = GetBoneIndex(Bones[Side][2]);
		if (U == INDEX_NONE || L == INDEX_NONE || H == INDEX_NONE || H >= Cs.Num()) continue;
		const TArray<FTransform> Old = Cs;
		FTransform Ut = Cs[U], Lt = Cs[L], Ht = Cs[H];
		AnimationCore::SolveTwoBoneIK(Ut, Lt, Ht, Lt.GetLocation(), ToWorld.InverseTransformPosition(HandTarget[Side]), false, 1.0, 1.0);
		Cs[U] = Ut;
		Cs[L] = Lt;
		Cs[H] = Ht;
		TBitArray<> Changed(false, Cs.Num());
		Changed[U] = true;
		Changed[L] = true;
		Changed[H] = true;
		for (int32 i = U + 1; i < Cs.Num(); ++i)
		{
			const int32 P = Ref.GetParentIndex(i);
			if (i == L || i == H || P == INDEX_NONE || !Changed[P]) continue;
			Cs[i] = Old[i].GetRelativeTransform(Old[P]) * Cs[P];
			Changed[i] = true;
		}
	}
}

// ================= suhbatdosh
// Bir suhbatda bir vaqtda bitta odam gapiradi, qolganlari tinglaydi. Navbatni guruhning birinchi a'zosi boshqaradi.
FBozorCo ABozorNpc::TalkerLoop()
{
	if (TalkGroup >= 0) FaceTowards(LookTarget);
	co_await FBozorWait{ 0.f };                       // hamma a'zolar boshlansin
	TArray<ABozorNpc*> Members;
	if (TalkGroup >= 0)
	{
		for (TActorIterator<ABozorNpc> It(GetWorld()); It; ++It)
		{
			if (It->NpcRole == EBozorRole::Suhbatdosh && It->TalkGroup == TalkGroup) Members.Add(*It);
		}
	}
	if (Members.Num() == 0) Members.Add(this);
	Members.Sort([](const ABozorNpc& A, const ABozorNpc& B) { return A.Order < B.Order; });
	if (Members[0] != this) co_return;
	co_await FBozorWait{ FMath::FRandRange(0.f, 2.f) };
	int32 Speaker = FMath::RandRange(0, Members.Num() - 1);
	while (true)
	{
		ABozorNpc* S = Members[Speaker];
		const float D = S->Do(FMath::FRand() < 0.65f ? TEXT("Pair") : TEXT("C_Talk"), 3.f, 7.f);
		// tinglovchilar biroz kechikib: hammasi bir kadrda bir xil klipni boshlasa, bir xil qimirlardi
		for (ABozorNpc* M : Members)
		{
			if (M != S) M->DoLater(TEXT("C_Listen"), D, FMath::FRandRange(0.f, 0.8f));
		}
		co_await FBozorWait{ D };
		// gap ko'pincha boshqasiga o'tadi, ba'zan o'sha odam davom etadi
		if (Members.Num() > 1 && FMath::FRand() < 0.8f) Speaker = (Speaker + FMath::RandRange(1, Members.Num() - 1)) % Members.Num();
	}
}

// ================= yordamchilar

// ================= qo'lda yozilgan ssenariy
// Behavior Tree'dagi Sequence kabi: qadamlar ketma-ket, har biri tugaguncha (Running) davom etadi; oxirida boshiga
// qaytadi. Qaror (qayerga, nima) ssenariyda, harakat - NavMesh + crowd (game-ai: decide / move ajratilgan)
FBozorCo ABozorNpc::ScriptLoop()
{
	co_await FBozorWait{ StartDelay };
	while (Script.Num() > 0)
	{
		for (int32 i = 0; i < Script.Num(); ++i)
		{
			ScriptStep = i;
			const FBozorStep S = Script[i];
			// guruh boshlig'i: hamrohlari orqada qolgan bo'lsa (4.5 m+) ularga qarab kutadi, ko'pi bilan 8 s
			// (oila bir-birini kutadi; aks holda hamroh 14 m orqada qolib, gavjumda tiqilardi - 2026-11-09)
			for (float W = 8.f; W > 0.f; W -= 0.25f)
			{
				const ABozorNpc* Lag = nullptr;
				for (const TWeakObjectPtr<ABozorNpc>& M : Mgr->Movers)
				{
					if (M.IsValid() && M->Leader.Get() == this && FVector::DistSquared2D(M->GetActorLocation(), GetActorLocation()) > 450.f * 450.f) Lag = M.Get();
				}
				if (!Lag) break;
				if (W == 8.f)
				{
					StopMoving();
					TurnTowards(Lag->GetActorLocation() - GetActorLocation());
					DoFirst({ TEXT("C_Wait"), TEXT("P_Pause") }, 3.f, 3.f);
				}
				co_await FBozorWait{ 0.25f };
			}
			switch (S.Op)
			{
			case EBozorStepOp::GoTo:
				co_await WalkTo(S.Target, S.Speed);
				break;
			case EBozorStepOp::Anim:
			{
				const float T = Do(S.Clip, FMath::Max(S.Time, 0.5f), FMath::Max(S.Time, 0.5f));
				co_await FBozorWait{ S.Time > 0.f ? S.Time : T };
				break;
			}
			case EBozorStepOp::Face:
				co_await TurnTo((S.Other ? S.Other->GetActorLocation() : S.Target) - GetActorLocation());
				break;
			case EBozorStepOp::Greet:
				// tanish sotuvchi bilan salom-alik: unga buriladi, qo'lini ko'ksiga qo'yadi, sotuvchi javob beradi
				if (S.Other)
				{
					co_await TurnTo(S.Other->GetActorLocation() - GetActorLocation());
					S.Other->Greet();
					co_await FBozorWait{ DoFirst({ S.Clip.IsNone() ? FName(TEXT("C_Greet")) : S.Clip, TEXT("C_Greet"), TEXT("C_Talk") }, 1.5f, 2.5f) };
				}
				break;
			case EBozorStepOp::Stall:
			{
				bool bReached = false;
				co_await VisitStall(S.Index, S.Time > 0.f ? S.Time : 6.f, S.bBuy ? 1 : 0, &bReached);
				break;
			}
			case EBozorStepOp::Sit:
				co_await SitAt(S.Index, S.Time > 0.f ? S.Time : 25.f);
				break;
			case EBozorStepOp::Wait:
			default:
				if (!S.Clip.IsNone()) Do(S.Clip, S.Time, S.Time);
				else PlayMove();
				co_await FBozorWait{ FMath::Max(S.Time, 0.f) };
				break;
			}
		}
		// har aylanishda kamida bir kadr: hamma qadami bir zumda tugaydigan ssenariy o'yinni qotirmasin
		co_await FBozorWait{ 0.f };
	}
}

FBozorCo ABozorNpc::WalkTo(FVector Target, float Speed)
{
	AAIController* C = AI();
	bTurning = false;
	PlayMove();
	// ssenariy qadami o'z tezligini berishi mumkin (shoshgan yo'lovchi, sekin keksa)
	const float WalkSpd = Speed > 0.f ? FMath::Clamp(Speed, MinWalk, MaxWalk) : WalkSpeed;
	// yurib bo'lmaydi: chaqiruvchi sikl kadr kutmasdan aylanib qolmasin
	if (!C || !MoveRequest(Target, WalkSpd))
	{
		co_await FBozorWait{ 0.f };
		co_return;
	}
	float Timeout = 90.f, Still = 0.f, Slow = 0.f;
	int32 Detours = 0;
	bool bGaveUp = false;
	double CrowdOff = 0.0;
	float Best = FVector::Dist2D(Target, GetActorLocation());
	bCartTurnFailed = false;
	while (Timeout > 0.f && C->GetMoveStatus() != EPathFollowingStatus::Idle)
	{
		co_await FBozorWait{ 0.f };
		const float Dt = GetWorld()->GetDeltaSeconds();
		Timeout -= Dt;
		if (CrowdOff > 0.0 && GetWorld()->GetTimeSeconds() >= CrowdOff)
		{
			SetCrowdSteering(C, true);
			CrowdOff = 0.0;
		}
		// aravani orqaga burib bo'lmadi - boshqa manzil
		if (bCartTurnFailed)
		{
			bCartTurnFailed = false;
			break;
		}
		const float Dist = FVector::Dist2D(Target, GetActorLocation());
		if (Dist <= 50.f) CartTurnFails = 0;      // manzilga yetdi: keyingi safar yana burib ko'radi
		// tik turgan odamga tiralib qoldi (crowd uni aylanib o'tolmay tezlik 0 bo'ladi, 2026-11-09 kuzatuvi: 3-10 s):
		// manzilning o'zida odam tursa - yetib keldi deb hisoblaydi, aks holda uning yonidan aylanib o'tadi
		Slow = IsMoving() || bCartTurning ? 0.f : Slow + Dt;
		if (Slow > 0.6f)
		{
			Slow = 0.f;
			const FVector Dir = PathDir(Target);
			if (ABozorNpc* B = BlockerAhead(Dir, 110.f))
			{
				// manzilga 1.2 m qoldi, oldida begona odam: yetib keldi deb hisoblaydi (o'z hamrohi esa yo'l beradi)
				if (Dist < 120.f && B->Leader.Get() != this) break;
				FVector Det;
				if (Detours < 2 && DetourPoint(B, Dir, Det))
				{
					Detours++;
					MoveRequest(Det, WalkSpd);
					// aylanib o'tish vaqti ham tiqilish hisobiga kiradi: o'tib bo'lmasa, 3 s da voz kechadi
					for (float T = 2.f; T > 0.f && C->GetMoveStatus() != EPathFollowingStatus::Idle && FVector::DistSquared2D(GetActorLocation(), Det) > 30.f * 30.f; T -= GetWorld()->GetDeltaSeconds())
					{
						co_await FBozorWait{ 0.f };
						Still += GetWorld()->GetDeltaSeconds();
					}
					if (!MoveRequest(Target, WalkSpd))
					{
						bGaveUp = true;
						break;
					}
					continue;
				}
				// aylanib o'tadigan bo'sh joy yo'q (tor yo'lakdagi navbat): crowd'siz yo'l bo'yicha sirpanib o'tishga urinadi
				if (CrowdOff <= 0.0)
				{
					SetCrowdSteering(C, false);
					CrowdOff = GetWorld()->GetTimeSeconds() + 1.5;
				}
			}
			else if (CrowdOff <= 0.0)
			{
				// oldida odam yo'q, lekin yurolmayapti: NavMesh chetida crowd tutib qolgan
				SetCrowdSteering(C, false);
				CrowdOff = GetWorld()->GetTimeSeconds() + 1.5;
			}
		}
		// 3 s manzilga yaqinlashmadi (yo'lni odamlar to'sib turibdi): bu manzildan voz kechadi. Arava burilyapti - bu tiqilish emas
		if (Dist < Best - 30.f || bCartTurning)
		{
			Best = FMath::Min(Best, Dist);
			Still = 0.f;
		}
		else if ((Still += Dt) > 3.f)
		{
			bGaveUp = true;
			break;
		}
	}
	// voz kechildi: keyingi manzil 10 s shu tomonda tanlanmaydi (aks holda yana o'sha odamga tiraladi)
	if (bGaveUp)
	{
		AvoidDir = Flat(Target - GetActorLocation()).GetSafeNormal();
		AvoidUntil = GetWorld()->GetTimeSeconds() + 10.0;
	}
	if (CrowdOff > 0.0) SetCrowdSteering(C, true);
	StopMoving();
}

// yo'lning hozirgi yo'nalishi (keyingi burilish nuqtasiga), yo'l bo'lmasa - to'g'ri manzilga
FVector ABozorNpc::PathDir(const FVector& Goal) const
{
	const FVector Here = GetActorLocation();
	FVector To = Goal;
	if (AAIController* C = AI())
	{
		const UPathFollowingComponent* PF = C->GetPathFollowingComponent();
		if (PF && PF->GetStatus() != EPathFollowingStatus::Idle) To = SteeringTarget(PF, Here);
	}
	FVector D = Flat(To - Here);
	if (D.SizeSquared() < 1.f) D = Flat(Goal - Here);
	return D.GetSafeNormal();
}

// Dir tomonida Radius ichidagi eng yaqin odam (o'tirganlar - to'qnashuvi o'chiq - hisobga olinmaydi)
ABozorNpc* ABozorNpc::BlockerAhead(const FVector& Dir, float Radius) const
{
	ABozorNpc* Best = nullptr;
	float Bd = Radius * Radius;
	for (TActorIterator<ABozorNpc> It(GetWorld()); It; ++It)
	{
		if (*It == this || !It->GetActorEnableCollision()) continue;
		const FVector D = Flat(It->GetActorLocation() - GetActorLocation());
		const float S = D.SizeSquared();
		if (S < Bd && FVector::DotProduct(D.GetSafeNormal(), Dir) > 0.2f)
		{
			Bd = S;
			Best = *It;
		}
	}
	return Best;
}

// To'siq odamning yonidan (o'zimiz turgan tomondan, bo'lmasa narigi tomondan) o'tib ketadigan bo'sh nuqta
bool ABozorNpc::DetourPoint(const ABozorNpc* Blocker, const FVector& Dir, FVector& Out) const
{
	const FVector Bp = Blocker->GetActorLocation();
	const FVector R = FVector::CrossProduct(FVector::UpVector, Dir);
	const float Pref = FVector::DotProduct(Flat(GetActorLocation() - Bp), R) >= 0.f ? 1.f : -1.f;
	// tor joyda (rasta yonida) 95 sm yon tomonda NavMesh bo'lmasligi mumkin - yaqinrog'i ham sinaladi
	for (const float Wide : { 95.f, 72.f })
	{
		for (const float S : { Pref, -Pref })
		{
			FVector H;
			if (!ProjectToNav(Bp + R * S * Wide + Dir * 40.f, H, 30.f)) continue;
			bool bFree = true;
			for (TActorIterator<ABozorNpc> It(GetWorld()); It && bFree; ++It)
			{
				if (*It != this && FVector::DistSquared2D(It->GetActorLocation(), H) < 45.f * 45.f) bFree = false;
			}
			if (!bFree) continue;
			Out = H;
			return true;
		}
	}
	return false;
}

FBozorCo ABozorNpc::TurnTo(FVector Forward)
{
	TurnTowards(Forward);
	while (bTurning) co_await FBozorWait{ 0.f };
}

// ============================================================ avtomatik sinov

void ABozorNpcManager::LogStats(const TCHAR* Tag)
{
	int32 Chars = 0, Moving = 0, InQueue = 0, Trading = 0, AtStall = 0, Sitting = 0;
	for (TActorIterator<ABozorNpc> It(GetWorld()); It; ++It)
	{
		Chars++;
		Moving += It->IsMoving() ? 1 : 0;
		InQueue += It->Queue != INDEX_NONE ? 1 : 0;
		AtStall += It->bAtStall ? 1 : 0;
		Trading += It->NpcRole == EBozorRole::Sotuvchi && It->Deal.IsValid() ? 1 : 0;
		Sitting += It->bSitting ? 1 : 0;
	}
	// tashxis: tana qaysi tomonga qaragan (oyoq uchi: foot -> ball) - yuruvchilarda yurish tomoniga, hammolda aravaga
	auto Facing = [](ABozorNpc* N)
	{
		const USkeletalMeshComponent* M = N->GetMesh();
		FVector T = M->GetSocketLocation(TEXT("ball_l")) - M->GetSocketLocation(TEXT("foot_l")) + M->GetSocketLocation(TEXT("ball_r")) - M->GetSocketLocation(TEXT("foot_r"));
		T.Z = 0.0;
		return T.GetSafeNormal();
	};
	int32 Walkers = 0, Backward = 0;
	for (TActorIterator<ABozorNpc> It(GetWorld()); It; ++It)
	{
		if (It->bCart)
		{
			UE_LOG(LogBozorNpc, Display, TEXT("[BozorNpc] hammol %s kadr=%d orqaga=%d aylantirish=%d voz_kechish=%d tezlik=%.0f aravaga_qarash=%.2f"), *It->GetActorNameOrLabel(),
				It->CartFrames, It->CartBackFrames, It->CartTurns, It->CartGiveUps, It->GetVelocity().Size2D(), FVector::DotProduct(Facing(*It), It->GetActorForwardVector()));
		}
		else if (It->GetVelocity().Size2D() > 50.f)
		{
			Walkers++;
			Backward += FVector::DotProduct(Facing(*It), It->GetVelocity().GetSafeNormal2D()) < 0.f ? 1 : 0;
		}
	}
	UE_LOG(LogBozorNpc, Display, TEXT("[BozorNpc] yuruvchilar=%d teskari_qaragan=%d"), Walkers, Backward);
	UE_LOG(LogBozorNpc, Display, TEXT("[BozorNpc] %s t=%.0f npc=%d harakatda=%d navbatda=%d rasta_oldida=%d savdo(hozir)=%d savdo(boshlangan)=%d sotib_olgan=%d o'tirgan=%d o'tirishlar=%d fps=%.0f"),
		Tag, GetWorld()->GetTimeSeconds() - StartTime, Chars, Moving, InQueue, AtStall, Trading, TradesStarted, TradesBought, Sitting, SitCount,
		1.0 / FMath::Max(FApp::GetDeltaTime(), 1e-4));
}

void ABozorNpcManager::AutoTest(double Now)
{
	// kamera nuqtalari: kirish, yo'lak o'rtasi, rasta yonida, yuqoridan, sotuvchilar qatori, xaridorlar yaqindan
	static const FVector Locs[] = { FVector(-400, 0, 350), FVector(1200, 0, 170), FVector(800, 100, 160), FVector(-600, 0, 1300), FVector(700, 60, 165), FVector(2000, -120, 160) };
	static const FRotator Rots[] = { FRotator(-12, 0, 0), FRotator(-5, 0, 0), FRotator(-8, 90, 0), FRotator(-38, 0, 0), FRotator(-6, -90, 0), FRotator(-6, -60, 0) };
	// oxirgi kadr: aravali hammol yaqindan (yon tomondan, tepadan)
	const int32 Count = UE_ARRAY_COUNT(Locs) + 1;
	if (Now < NextShot) return;
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC) return;
	// birinchi ishga tushishda shader'lar kompilyatsiya qilinadi, NavMesh quriladi: tayyor bo'lgach 12 s o'tib suratga olinadi
	if (ReadyTime <= 0.0)
	{
		int32 Jobs = 0;
#if WITH_EDITOR
		Jobs = GShaderCompilingManager ? GShaderCompilingManager->GetNumRemainingJobs() : 0;
#endif
		UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
		const bool bNavBusy = Nav && Nav->IsNavigationBuildInProgress();
		if (Jobs > 0 || bNavBusy)
		{
			UE_LOG(LogBozorNpc, Display, TEXT("[BozorNpc] kutilmoqda: shader=%d navmesh=%d"), Jobs, bNavBusy ? 1 : 0);
			NextShot = Now + 5.0;
			return;
		}
		ReadyTime = Now;
		NextShot = Now + 12.0;
		LogStats(TEXT("tayyor"));
		return;
	}
	const int32 Round = AutoShot / Count;
	if (Round >= 2)
	{
		LogStats(TEXT("yakun"));
		UKismetSystemLibrary::QuitGame(this, PC, EQuitPreference::Quit, false);
		NextShot = DBL_MAX;
		return;
	}
	const int32 Shot = AutoShot % Count;
	if (Shot == 0) LogStats(Round == 0 ? TEXT("1-tur") : TEXT("2-tur"));
	if (!AutoCamera)
	{
		AutoCamera = GetWorld()->SpawnActor<ACameraActor>(Locs[0], Rots[0]);
		AutoCamera->GetCameraComponent()->SetFieldOfView(70.f);
		AutoCamera->GetCameraComponent()->bConstrainAspectRatio = false;
	}
	FVector CamLoc = Locs[FMath::Min(Shot, Count - 2)];
	FRotator CamRot = Rots[FMath::Min(Shot, Count - 2)];
	if (Shot == Count - 1)
	{
		for (TActorIterator<ABozorNpc> It(GetWorld()); It; ++It)
		{
			if (!It->bCart) continue;
			const FVector P = It->GetActorLocation();
			CamLoc = P - It->GetActorRightVector() * 380.f - It->GetActorForwardVector() * 120.f + FVector(0.f, 0.f, 160.f);
			CamRot = (P - FVector(0.f, 0.f, 40.f) - It->GetActorForwardVector() * 80.f - CamLoc).Rotation();
			break;
		}
	}
	AutoCamera->SetActorLocationAndRotation(CamLoc, CamRot);
	PC->SetViewTarget(AutoCamera);
	FTimerHandle H;
	const FString Name = FString::Printf(TEXT("Bozor_%d_%d"), Round + 1, Shot + 1);
	GetWorldTimerManager().SetTimer(H, [Name]() { FScreenshotRequest::RequestScreenshot(Name, false, false); }, 1.0f, false);
	AutoShot++;
	NextShot = Now + (Shot == Count - 1 ? 15.0 : 2.0);
}
