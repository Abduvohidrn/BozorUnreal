// NPC'lar uchun CPU tejash (NPC kodiga tegmasdan, tashqaridan sozlaydi):
//  1) ko'rinmayotgan va uzoq NPC'ning yashirin "tana" mesh'i (kiyim qismlariga poza beradi) animatsiyani to'xtatadi
//  2) NPC skelet mesh'larida fizika jismlari va overlap yo'q (to'qnashuv - kapsulada)
//  3) kiyim qismlari chegarasi qat'iy (har kadr qayta hisoblanmaydi)
// O'chirish: -SamaradorlikYoq (A/B o'lchov uchun)
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BozorSamaradorlik.generated.h"

class ABozorNpc;
class USkeletalMeshComponent;

UCLASS()
class BOZORUNREAL_API UBozorSamaradorlik : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

private:
	struct FNpcHolat
	{
		TWeakObjectPtr<ABozorNpc> Npc;
		TWeakObjectPtr<USkeletalMeshComponent> Tana;              // animatsiya manbai
		TArray<TWeakObjectPtr<USkeletalMeshComponent>> Korinadi;  // ekranda chiziladiganlar (qismlar yoki tananing o'zi)
		bool bToxtagan = false;
	};

	void Yigish();
	void NpcniSozla(ABozorNpc* Npc);

	TArray<FNpcHolat> Npclar;
	float YigishT = 0.f;
	int32 Toxtaganlar = 0;
	float LogT = 0.f;
};
