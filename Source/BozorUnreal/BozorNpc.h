#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/TimerHandle.h"
#include <coroutine>
#include "BozorNpc.generated.h"

class UAnimSequence;
class UStaticMesh;
class UStaticMeshComponent;
class ABozorNpc;

// Unity BozorNpc.cs ning Unreal ko'chirmasi. Qoidalar: bozor_npc_xulq_atvori.md (2-bo'lim).
// Unity korutinlari (IEnumerator) bu yerda C++20 korutinlari: co_await Wait(s), co_await WalkTo(p) - xuddi yield return kabi.

// ================= korutin
struct FBozorCo
{
	struct promise_type
	{
		std::coroutine_handle<promise_type> Child;   // co_await qilingan ichki korutin
		float Wait = 0.f;
		uint64 WaitFrame = 0;
		bool bWaiting = false;

		FBozorCo get_return_object() { return FBozorCo(std::coroutine_handle<promise_type>::from_promise(*this)); }
		std::suspend_always initial_suspend() noexcept { return {}; }
		std::suspend_always final_suspend() noexcept { return {}; }
		void return_void() {}
		void unhandled_exception() { checkNoEntry(); }
	};
	using FHandle = std::coroutine_handle<promise_type>;

	FBozorCo() = default;
	explicit FBozorCo(FHandle InH) : H(InH) {}
	FBozorCo(FBozorCo&& O) noexcept : H(O.H) { O.H = nullptr; }
	FBozorCo& operator=(FBozorCo&& O) noexcept
	{
		if (this != &O) { Reset(); H = O.H; O.H = nullptr; }
		return *this;
	}
	FBozorCo(const FBozorCo&) = delete;
	FBozorCo& operator=(const FBozorCo&) = delete;
	~FBozorCo() { Reset(); }

	void Reset()
	{
		if (H) H.destroy();
		H = nullptr;
	}
	bool IsRunning() const { return H && !H.done(); }
	// har kadr: true - korutin tugadi
	bool Step(float Dt) { return !H || StepHandle(H, Dt); }

	// co_await boshqa korutin (Unity: yield return StartCoroutine / IEnumerator)
	bool await_ready() const noexcept { return !H || H.done(); }
	void await_suspend(FHandle Parent) noexcept { Parent.promise().Child = H; }
	void await_resume() noexcept {}

	FHandle H = nullptr;

private:
	static bool StepHandle(FHandle Co, float Dt);
};

// co_await FBozorWait{s} - WaitForSeconds; s = 0 - keyingi kadr (yield return null)
struct FBozorWait
{
	float Seconds = 0.f;
	bool await_ready() const noexcept { return false; }
	void await_suspend(FBozorCo::FHandle Co) noexcept
	{
		auto& P = Co.promise();
		P.Wait = Seconds;
		P.WaitFrame = GFrameCounter;
		P.bWaiting = true;
	}
	void await_resume() noexcept {}
};

UENUM(BlueprintType)
enum class EBozorRole : uint8
{
	Sotuvchi,
	Xaridor,
	Hamroh,
	Yolovchi,
	Hammol,
	Suhbatdosh
};

// Animatsiya kategoriyasi (V_Idle, V_Call, C_Talk ...): Unity BozorNpcManager.Action
USTRUCT(BlueprintType)
struct FBozorNpcAction
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	FName Category;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	TObjectPtr<UAnimSequence> Anim = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	TObjectPtr<UAnimSequence> AlternateAnim = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	bool bLoop = false;
};

// Rasta oldidagi navbat: Unity BozorQueue. 0-nuqta - savdolashadigan joy, keyingilari peshtaxta bo'ylab 70 sm
USTRUCT(BlueprintType)
struct FBozorQueueSpot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	TArray<FVector> Points;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	FVector Forward = FVector::ForwardVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	TObjectPtr<ABozorNpc> Vendor = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	int32 Stall = -1;

	// StartOnly: avval navbat oxiriga keladi; aks holda to'g'ri o'z joyiga
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	bool bStartOnly = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	bool bCanLeaveRandomly = true;

	// navbatda kutish (s), keyin "boshqa rastaga boraman"
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	FVector2D Patience = FVector2D(20.f, 45.f);
};

// Qo'lda yozilgan ssenariy qadami (npc_ssenariy_rejasi.md, 3-bo'lim). Ssenariy - Behavior Tree'dagi Sequence kabi:
// qadamlar ketma-ket, har biri tugaguncha davom etadi; oxirida boshiga qaytadi
UENUM(BlueprintType)
enum class EBozorStepOp : uint8
{
	GoTo,    // Target nuqtaga boradi (Speed > 0 bo'lsa shu tezlikda)
	Anim,    // Clip kategoriyasini Time soniya o'ynaydi
	Face,    // Other'ga yoki Target nuqtaga buriladi
	Greet,   // Other (sotuvchi) bilan salomlashadi
	Stall,   // Index-navbatdagi rastaga boradi: navbat, mol ko'rish (Time), bBuy - xarid yoki "qimmat" deb ketish
	Sit,     // Index-o'rindiqqa Time soniya o'tiradi
	Wait     // Time soniya kutadi (tik turish)
};

USTRUCT(BlueprintType)
struct FBozorStep
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	EBozorStepOp Op = EBozorStepOp::Wait;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	FVector Target = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	TObjectPtr<ABozorNpc> Other = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	FName Clip;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	float Time = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	float Speed = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	int32 Index = -1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	bool bBuy = true;
};

USTRUCT(BlueprintType)
struct FBozorSeat
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	FVector Position = FVector::ZeroVector;

	// o'tirgan odam qaraydigan tomon
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	FVector Forward = FVector::ForwardVector;
};

// Barcha NPC'lar uchun umumiy ma'lumot: Unity BozorNpcManager + BozorQueue (ro'yxat) + BozorCrowdAI (o'rindiqlar)
UCLASS()
class BOZORUNREAL_API ABozorNpcManager : public AActor
{
	GENERATED_BODY()

public:
	ABozorNpcManager();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	TArray<FBozorNpcAction> Actions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	TArray<FBozorQueueSpot> Queues;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	TArray<FVector> WanderPoints;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	TArray<FVector> GatePoints;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	TArray<FBozorSeat> Seats;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	TArray<TObjectPtr<UStaticMesh>> BagMeshes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	TObjectPtr<UStaticMesh> CartMesh = nullptr;

	static ABozorNpcManager* Get(const UObject* WorldContext);

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	const FBozorNpcAction* Pick(FName Category) const;
	FVector RandomWanderPoint() const;
	FVector RandomGatePoint() const;

	// navbat (Unity BozorQueue)
	int32 BestQueue(const FVector& From, int32 Except) const;
	int32 Join(int32 Q, ABozorNpc* C);
	void Leave(int32 Q, ABozorNpc* C);
	int32 IndexOf(int32 Q, const ABozorNpc* C) const;
	FVector Point(int32 Q, int32 I) const;
	FVector EntryPoint(int32 Q) const { return Point(Q, Queues[Q].Points.Num() - 1); }
	FVector LookAtLocation(int32 Q) const;
	bool NearAnyPoint(const FVector& P, float Radius) const;

	// o'rindiqlar (Unity BozorCrowdAI)
	int32 ReserveSeat(ABozorNpc* Npc, const FVector& From, float MaxDistance);
	bool ReserveSeatIndex(int32 Seat, ABozorNpc* Npc);
	void ReleaseSeat(int32 Seat, ABozorNpc* Npc);

	// yuradigan NPC'lar (sotuvchi chaqirig'i uchun)
	TArray<TWeakObjectPtr<ABozorNpc>> Movers;

	// -BozorAutoTest: holat logi va skrinshotlar, keyin chiqish
	int32 TradesStarted = 0;
	int32 TradesBought = 0;
	int32 SitCount = 0;

private:
	void EnsureNavMesh();
	void AutoTest(double Now);
	void LogStats(const TCHAR* Tag);

	TMap<FName, TArray<int32>> ByCat;
	TArray<TArray<TWeakObjectPtr<ABozorNpc>>> QueueCustomers;
	TArray<float> Popularity;
	TArray<TWeakObjectPtr<ABozorNpc>> SeatOccupant;

	bool bAutoTest = false;
	UPROPERTY(Transient)
	TObjectPtr<class ACameraActor> AutoCamera;
	int32 AutoShot = 0;
	double ReadyTime = 0.0;
	double NextShot = 0.0;
	double StartTime = 0.0;
};

// NPC tanasi: animatsiya baholangandan keyin qo'llarni nishonga yetkazadi (two-bone IK) - Unity OnAnimatorIK muqobili.
// Hammol aravaning dastasini ushlaydi
UCLASS()
class BOZORUNREAL_API UBozorNpcMesh : public USkeletalMeshComponent
{
	GENERATED_BODY()

public:
	bool bHandIK = false;
	FVector HandTarget[2];     // dunyo koordinatasida: 0 - chap, 1 - o'ng qo'l

	virtual void FinalizeBoneTransform() override;

private:
	void ApplyHandIK();
};

UCLASS()
class BOZORUNREAL_API ABozorNpc : public ACharacter
{
	GENERATED_BODY()

public:
	ABozorNpc(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	EBozorRole NpcRole = EBozorRole::Yolovchi;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	bool bFemale = false;

	// yurish uslubi (Unity walkStyle): 0 erkak, 1 ayol, 2 keksa erkak, 3 keksa ayol; -1 - jinsdan. Sotuvchi ovozi keksaligini shundan oladi
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	int32 WalkStyle = -1;

	// sm/s
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	float WalkSpeed = 85.f;

	// Hamroh: kimga ergashadi va undan nisbatan o'rni (1, 2, 3)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	TObjectPtr<ABozorNpc> Leader = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	int32 GroupSlot = 0;

	// Suhbatdosh: bir guruh a'zolari bir xil raqamda, Order bo'yicha tartiblanadi; LookTarget - guruh markazi
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	int32 TalkGroup = -1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	int32 Order = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	FVector LookTarget = FVector::ZeroVector;

	// Hammol aravasi bormi
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	bool bCart = false;

	// Qo'lda yozilgan ssenariy (Scripts/npc_ssenariy.json -> npc_ssenariy.py). Bo'sh bo'lmasa rol sikli o'rniga shu bajariladi
	// (sotuvchi, hamroh va suhbatdosh o'z sikllarida qoladi, ular faqat o'z animatsiyalarini oladi)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	TArray<FBozorStep> Script;

	// ssenariy boshlanishidan oldingi kutish (NPC'lar bir vaqtda bir joyga yopirilmasin)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	float StartDelay = 0.f;

	// NPC'ning o'z animatsiyalari (kategoriya -> klip). Do() avval shundan, topilmasa menejerdan oladi
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	TArray<FBozorNpcAction> OwnActions;

	// kim ekani (keksa ota, ona, talaba ...) - faqat ma'lumot uchun
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	FString Persona;

	// hozir bajarilayotgan qadam (kuzatuv/tashxis uchun)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bozor")
	int32 ScriptStep = -1;

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	void LinkCharacterParts();
	virtual void Tick(float DeltaSeconds) override;

	bool IsMoving() const { return GetVelocity().SizeSquared2D() > 20.f * 20.f; }

	// sotuvchi
	TWeakObjectPtr<ABozorNpc> Deal;
	void Greet();
	bool TryBeginDeal(ABozorNpc* Customer);
	void EndDeal(float Busy = 0.f);

	// o'yinchi (bola) bilan suhbat: shu payt sotuvchi xaridorlarga xizmat qilmaydi, ular navbatda kutadi.
	// Boshla - false: sotuvchi hozir boshqa xaridor bilan band
	bool OyinchiBilanBoshla(AActor* Oyinchi);
	void OyinchiBilanTugat();
	bool OyinchiBilanmi() const { return OyinchiSuhbat.IsValid(); }
	TWeakObjectPtr<AActor> OyinchiSuhbat;

	// xaridor (boshliq): qaysi navbatda va rasta oldidami
	int32 Queue = INDEX_NONE;
	bool bAtStall = false;
	FVector StallForward = FVector::ForwardVector;
	bool bSitting = false;

	// hammol (avto-sinov uchun): kadrlar, arava orqaga yurgan kadrlar, aylantirishlar, voz kechishlar
	int32 CartFrames = 0, CartBackFrames = 0, CartTurns = 0, CartGiveUps = 0;

	float Do(FName Category, float Min = 2.f, float Max = 6.f);
	void DoLater(FName Category, float Duration, float Delay);

private:
	ABozorNpcManager* Mgr = nullptr;
	FBozorCo Main;
	bool bStarted = false;
	FName Current;
	FQuat HomeRotation = FQuat::Identity;
	float CallCooldown = 0.f;
	double BusyUntil = 0.0;
	double SettleAt = 0.0;
	FName SettleCategory;
	FTimerHandle DelayedActionTimer;
	bool bTurning = false;
	FQuat TurnGoal = FQuat::Identity;
	float MinWalk = 50.f;
	float MaxWalk = 170.f;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Bag;
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Cart;
	// hammol aravasi: kapsula (AI agenti) - aravaning g'ildirak o'qi, u yo'l bo'ylab oldinda yuradi; hammol dastadan
	// ushlab, tirkama kabi orqasidan ergashadi (UpdateCart). G'ildirak yonga sirpanmaydi: burilishda arava o'qi atrofida
	// buriladi, hammol esa tashqariga suriladi
	FQuat CartTurn = FQuat::Identity;   // modelning arava o'qlariga burilishi: uzun tomoni oldinga, dastasi orqaga
	FVector CartAxle, CartGrip;         // pivotdan g'ildirak o'qi va dasta o'rtasigacha (arava o'qlarida, sm)
	FVector CartAxis, CartHitch;        // arava yo'nalishi; dasta ostidagi yer nuqtasi
	FQuat CartTilt = FQuat::Identity;   // dasta qo'l balandligiga ko'tarilgan: arava g'ildirak o'qi atrofida egilgan
	float CartScale = 1.f, CartGripDist = 0.f, CartGripHalf = 0.f, CartReach = 0.f;
	FQuat BodyRot = FQuat::Identity;
	FVector BodyPos = FVector::ZeroVector;
	bool bCartTurning = false;          // manzil orqada: arava g'ildiraklari atrofida aylantirilyapti (agent kutadi)
	bool bCartTurnFailed = false;       // aylantirib bo'lmadi (yoy yo'lida rasta/odam): WalkTo bu manzildan voz kechadi
	float CartTurnSign = 1.f, CartTurnBest = 0.f, CartTurnStill = 0.f;
	float RequestedWalkSpeed = 0.f;
	int32 CartTurnTries = 0, CartTurnFails = 0;
	void UpdateCart(float Dt);
	FVector SteeringTarget(const class UPathFollowingComponent* PF, const FVector& From) const;
	void PlaceCart(const FVector& AxleGround);
	float CapsuleHalf() const;

	void StartRole();
	void PlayMove(bool bCancelDelayedAction = true);
	void Settle();
	void StopMoving();
	void StartWalking(const FVector& Target, float Speed);
	bool MoveRequest(const FVector& Target, float Speed);
	void TurnTowards(const FVector& Forward);
	void FaceTowards(const FVector& P) { TurnTowards(P - GetActorLocation()); }
	bool SomeoneInFront(float Radius) const;
	void TakeBag();
	void AttachCart();
	FVector NextWalkTarget() const;
	FVector WaitSpot(float Side, float Row) const;
	// tik turgan odamlar: yo'l ularni bilmaydi, shuning uchun tiralib qolganda yonidan aylanib o'tiladi
	FVector PathDir(const FVector& Goal) const;
	ABozorNpc* BlockerAhead(const FVector& Dir, float Radius) const;
	bool DetourPoint(const ABozorNpc* Blocker, const FVector& Dir, FVector& Out) const;
	// WalkTo voz kechgan yo'nalish: keyingi manzil bir muddat shu tomonda tanlanmaydi
	FVector AvoidDir = FVector::ZeroVector;
	double AvoidUntil = 0.0;
	bool ProjectToNav(const FVector& P, FVector& Out, float Extent) const;
	class AAIController* AI() const;

	FBozorCo VendorLoop();
	FBozorCo WaitOrDeal(float T);
	FBozorCo CustomerLoop();
	FBozorCo DealWith(ABozorNpc* V, int32 ForceBuy = -1);
	FBozorCo ScriptLoop();
	// navbat orqali rastaga borish va savdo (CustomerLoop'ning bitta aylanishi). Natija: true - rastaga yetdi
	FBozorCo VisitStall(int32 Q, float Browse, int32 ForceBuy, bool* bOutReached);
	FBozorCo SitAt(int32 Seat, float Time);
	const FBozorNpcAction* PickAction(FName Category) const;
	float PlayAction(const FBozorNpcAction& Action, float Min, float Max);
	// ro'yxatdagi birinchi mavjud kategoriyani o'ynaydi (yangi kategoriya hali klipsiz bo'lsa eskisiga qaytadi)
	float DoFirst(const TArray<FName>& Categories, float Min, float Max);
	FBozorCo FollowerLoop();
	FBozorCo WalkerLoop();
	FBozorCo SitDown();
	FBozorCo Slide(FVector From, FVector To, float Time);
	FBozorCo TalkerLoop();
	FBozorCo WalkTo(FVector Target, float Speed = 0.f, bool* bOutReached = nullptr);
	FBozorCo TurnTo(FVector Forward);
};
