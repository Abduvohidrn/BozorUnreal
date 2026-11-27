#include "BozorSamaradorlik.h"

#include "BozorNpc.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"

#if PLATFORM_ANDROID && !UE_BUILD_SHIPPING
#include "DynamicRHI.h"
#include "HAL/PlatformTime.h"
#include "RenderTimer.h"
#endif

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
#if PLATFORM_ANDROID && !UE_BUILD_SHIPPING
	if (InWorld.GetMapName().EndsWith(TEXT("Bozor"), ESearchCase::IgnoreCase))
		MobilOlchovBoshlanishi = FPlatformTime::Seconds();
#endif
#if PLATFORM_ANDROID
	if (InWorld.GetFeatureLevel() == ERHIFeatureLevel::ES3_1)
	{
		TInlineComponentArray<UInstancedStaticMeshComponent*> Nusxalar;
		for (TActorIterator<AActor> It(&InWorld); It; ++It)
		{
			Nusxalar.Reset();
			It->GetComponents(Nusxalar);
			for (UInstancedStaticMeshComponent* Mesh : Nusxalar)
			{
				const UStaticMesh* Model = Mesh->GetStaticMesh();
				if (!Model || Mesh->Mobility != EComponentMobility::Static) continue;
				const bool bMahsulot = Model->GetPathName().StartsWith(TEXT("/Game/Bozor/Mollar/SM_Mol_"));
				const bool bMaydaOsimlik = Model->GetName().StartsWith(TEXT("SM_ATR_Ot_Yamoq"));
				if (!bMahsulot && !bMaydaOsimlik) continue;
				const int32 Masofa = bMahsulot ? 2600 : 4500;
				int32 Boshi, Oxiri;
				Mesh->GetCullDistances(Boshi, Oxiri);
				if (Oxiri <= 0 || Oxiri > Masofa)
					Mesh->SetCullDistances(Boshi > 0 ? FMath::Min(Boshi, Masofa * 4 / 5) : Masofa * 4 / 5, Masofa);
			}
		}

		for (TActorIterator<AStaticMeshActor> It(&InWorld); It; ++It)
		{
			UStaticMeshComponent* Mesh = It->GetStaticMeshComponent();
			if (!Mesh || !Mesh->GetStaticMesh() || Mesh->Mobility != EComponentMobility::Static) continue;
			const double Radius = Mesh->Bounds.SphereRadius;
			if (Radius > 80.0) continue;
			const float Masofa = Radius <= 40.0 ? 3000.f : 5000.f;
			const float Joriy = Mesh->CachedMaxDrawDistance;
			Mesh->SetCullDistance(Joriy > 0.f ? FMath::Min(Joriy, Masofa) : Masofa);
		}
	}
#endif
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
#if PLATFORM_ANDROID
	H.bAslAnimTejash = Tana->bEnableUpdateRateOptimizations;
#endif

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

#if PLATFORM_ANDROID && !UE_BUILD_SHIPPING
	if (MobilOlchovBoshlanishi > 0.0)
	{
		++MobilKadrlar;
		const double Hozir = FPlatformTime::Seconds();
		const double Oraliq = Hozir - MobilOlchovBoshlanishi;
		if (Oraliq >= 5.0)
		{
			const uint32 GPU = RHIGetGPUFrameCycles();
			const FString GPUVaqti = GPU > 0
				? FString::Printf(TEXT("%.1f ms"), FPlatformTime::ToMilliseconds(GPU))
				: TEXT("n/a");
			UE_LOG(LogTemp, Display, TEXT("[MobilFPS] %.1f FPS, Frame %.1f ms (o'rtacha), Game %.1f ms, Draw %.1f ms, RHI %.1f ms, GPU %s"),
				MobilKadrlar / Oraliq, 1000.0 * Oraliq / MobilKadrlar,
				FPlatformTime::ToMilliseconds(GGameThreadTime), FPlatformTime::ToMilliseconds(GRenderThreadTime),
				FPlatformTime::ToMilliseconds(GRHIThreadTime), *GPUVaqti);
			MobilOlchovBoshlanishi = Hozir;
			MobilKadrlar = 0;
		}
	}
#endif

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
		const double Masofa2 = FVector::DistSquared(Kamera, H.Npc->GetActorLocation());
		const bool bYaqin = Masofa2 < FMath::Square(YaqinMasofa);
#if PLATFORM_ANDROID
		if (W->GetFeatureLevel() == ERHIFeatureLevel::ES3_1)
		{
			const float UzoqMasofa = H.bUzoq ? 1500.f : 1800.f;
			const bool bUzoq = Masofa2 > FMath::Square(UzoqMasofa);
			if (bUzoq != H.bUzoq)
			{
				Tana->bEnableUpdateRateOptimizations = bUzoq || H.bAslAnimTejash;
				H.bUzoq = bUzoq;
			}
		}
#endif
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
