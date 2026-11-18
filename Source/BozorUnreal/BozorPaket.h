#pragma once

#include "CoreMinimal.h"

class UStaticMesh;
class UStaticMeshComponent;

// Plastik paket (Assets/Paket -> /Game/Bozor/Paket/SM_Paket_<Qora|Oq|Sariq>): pivot - kaftdagi tutqich, paket -Z ga osiladi,
// kengligi X (~25 sm), qalinligi Y (~16 sm), tubi ~-45 sm. Ichiga mol solish - paketning bola komponentlari (bitta mol meshi,
// masalan /Game/Bozor/Mollar/SM_Mol_Sabzi_1) tubidan qatlam-qatlam teriladi; og'zidan ustkilari ko'rinadi.
namespace BozorPaket
{
	// paket tubi va ichki kengligi (paket o'qlarida, sm): osilgan paket pastga torayadi - mollar o'rtaroqda
	constexpr float Tubi = -41.f;
	constexpr float IchkiX = 6.5f;
	constexpr float IchkiY = 3.5f;

	// mahsulot nomi/id ("sabzi", "bulg'or qalampiri") -> bitta mol meshi; yo'q bo'lsa nullptr
	UStaticMesh* MolMeshi(const FString& Nomi);

	// Paket ichiga Son ta mol soladi (eski ichidagilari olib tashlanadi). Mol kattaligi haqiqiy (4..11 sm oralig'iga
	// keltiriladi); 22 sm dan katta mollar (tarvuz, qovun, karam) paketga solinmaydi. Urug' - har safar boshqacha joylashuv
	void IchigaSol(UStaticMeshComponent* Paket, UStaticMesh* Mol, int32 Son, int32 Urug);

	// paketning ichidagi mollari (bola komponentlar) - FP rejimga o'tkazish va o'chirish uchun
	TArray<UStaticMeshComponent*> Ichidagilar(const UStaticMeshComponent* Paket);
}
