#include "BozorSamaradorlik.h"

#include "BozorNpc.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"

namespace
{
	// shu masofadan yaqin NPC ko'rinmasa ham animatsiyada qoladi (yon tomonda, soya, suhbat)
	constexpr float YaqinMasofa = 900.f;
	// "yaqinda chizilgan" oynasi: soya yoki ko'zguda ko'ringan bo'lsa ham hisoblanadi
	constexpr float ChizilganOyna = 0.25f;
}

bool UBozorSamaradorlik::ShouldCreateSubsystem(UObject* Outer) const
{
	if (FParse::Param(FCommandLine::Get(), TEXT("SamaradorlikYoq"))) return false;
	const UWorld* W = Cast<UWorld>(Outer);
	return W && W->IsGameWorld() && Super::ShouldCreateSubsystem(Outer);
}

TStatId UBozorSamaradorlik::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UBozorSamaradorlik, STATGROUP_Tickables);
}

void UBozorSamaradorlik::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	Yigish();
}

void UBozorSamaradorlik::Yigish()
{
	UWorld* W = GetWorld();
	if (!W) return;
	TSet<ABozorNpc*> Bor;
	for (const FNpcHolat& H : Npclar) if (H.Npc.IsValid()) Bor.Add(H.Npc.Get());
	for (TActorIterator<ABozorNpc> It(W); It; ++It)
		if (!Bor.Contains(*It)) NpcniSozla(*It);
}

void UBozorSamaradorlik::NpcniSozla(ABozorNpc* Npc)
{
	USkeletalMeshComponent* Tana = Npc->GetMesh();
	if (!Tana) return;
	FNpcHolat H;
	H.Npc = Npc;
	H.Tana = Tana;

	TArray<USkeletalMeshComponent*> Hammasi;
	Npc->GetComponents(Hammasi);
	for (USkeletalMeshComponent* M : Hammasi)
	{
		// to'qnashuv kapsulada; mesh'dagi fizika jismlari har kadr suyaklarga moslanardi (UpdateKinematicBonesToAnim)
		M->SetGenerateOverlapEvents(false);
		M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		const bool bQism = M != Tana && M->LeaderPoseComponent.Get() == Tana;
		if (bQism)
		{
			// kiyim qismi: chegara rest-pozadan (har kadr suyaklardan hisoblanmaydi)
			M->bComponentUseFixedSkelBounds = true;
			H.Korinadi.Add(M);
		}
	}
	if (H.Korinadi.Num() == 0) H.Korinadi.Add(Tana);   // qismsiz NPC - tananing o'zi ko'rinadi
	Npclar.Add(MoveTemp(H));
}

void UBozorSamaradorlik::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UWorld* W = GetWorld();
	if (!W) return;

	// kech paydo bo'lgan NPC'lar uchun vaqti-vaqti bilan yig'amiz
	YigishT += DeltaTime;
	if (YigishT > 2.f)
	{
		YigishT = 0.f;
		Npclar.RemoveAll([](const FNpcHolat& H) { return !H.Npc.IsValid(); });
		Yigish();
	}

	FVector Kamera = FVector::ZeroVector;
	if (const APlayerController* Pc = W->GetFirstPlayerController())
	{
		FRotator R;
		Pc->GetPlayerViewPoint(Kamera, R);
	}

	Toxtaganlar = 0;
	for (FNpcHolat& H : Npclar)
	{
		USkeletalMeshComponent* Tana = H.Tana.Get();
		if (!Tana || !H.Npc.IsValid()) continue;
		bool bKorindi = false;
		for (const TWeakObjectPtr<USkeletalMeshComponent>& K : H.Korinadi)
			if (K.IsValid() && K->WasRecentlyRendered(ChizilganOyna)) { bKorindi = true; break; }
		const bool bYaqin = FVector::DistSquared(Kamera, H.Npc->GetActorLocation()) < FMath::Square(YaqinMasofa);
		const bool bToxtat = !bKorindi && !bYaqin;
		if (bToxtat != H.bToxtagan)
		{
			// tana tick'i animatsiyani hisoblaydi; to'xtatilsa qismlar oxirgi pozada qoladi (NPC yurishda davom etadi)
			Tana->SetComponentTickEnabled(!bToxtat);
			H.bToxtagan = bToxtat;
		}
		Toxtaganlar += H.bToxtagan ? 1 : 0;
	}

	LogT += DeltaTime;
	if (LogT > 5.f && FParse::Param(FCommandLine::Get(), TEXT("FpsSinov")))
	{
		LogT = 0.f;
		UE_LOG(LogTemp, Display, TEXT("Samaradorlik: %d NPC, animatsiyasi to'xtatilgan %d"), Npclar.Num(), Toxtaganlar);
	}
}
