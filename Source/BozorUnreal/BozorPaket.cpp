#include "BozorPaket.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Math/RandomStream.h"

namespace
{
	const FName PaketMoliTeg(TEXT("PaketMoli"));
}

UStaticMesh* BozorPaket::MolMeshi(const FString& Nomi)
{
	// "bulg'or qalampiri" -> Qalampir, "ko'kat" -> Kokat: oxirgi so'z, tutuq belgilarsiz, bosh harfi katta
	FString S = Nomi.ToLower();
	S.ReplaceInline(TEXT("'"), TEXT(""));
	S.ReplaceInline(TEXT("‘"), TEXT(""));
	S.ReplaceInline(TEXT("’"), TEXT(""));
	S.ReplaceInline(TEXT("ʻ"), TEXT(""));
	TArray<FString> Sozlar;
	S.ParseIntoArrayWS(Sozlar);
	if (Sozlar.Num() == 0) return nullptr;
	// "bulgor qalampiri" -> qalampir (egalik qo'shimchasi -i)
	FString Soz = Sozlar.Last();
	if (Sozlar.Num() > 1 && Soz.EndsWith(TEXT("i"))) Soz.LeftChopInline(1);
	Soz[0] = FChar::ToUpper(Soz[0]);
	const FString Yol = FString::Printf(TEXT("/Game/Bozor/Mollar/SM_Mol_%s_1.SM_Mol_%s_1"), *Soz, *Soz);
	return LoadObject<UStaticMesh>(nullptr, *Yol, nullptr, LOAD_NoWarn | LOAD_Quiet);
}

TArray<UStaticMeshComponent*> BozorPaket::Ichidagilar(const UStaticMeshComponent* Paket)
{
	TArray<UStaticMeshComponent*> Out;
	if (!Paket) return Out;
	TArray<USceneComponent*> Bolalar;
	Paket->GetChildrenComponents(false, Bolalar);
	for (USceneComponent* B : Bolalar)
		if (UStaticMeshComponent* M = Cast<UStaticMeshComponent>(B); M && M->ComponentHasTag(PaketMoliTeg)) Out.Add(M);
	return Out;
}

void BozorPaket::IchigaSol(UStaticMeshComponent* Paket, UStaticMesh* Mol, int32 Son, int32 Urug)
{
	if (!Paket) return;
	for (UStaticMeshComponent* M : Ichidagilar(Paket)) M->DestroyComponent();
	if (!Mol || Son <= 0) return;
	const FBox B = Mol->GetBoundingBox();
	const float Katta = B.GetSize().GetMax();
	if (Katta < 0.5f || Katta > 22.f) return;   // tarvuz, qovun, karam paketga sig'maydi
	const float Olcham = FMath::Clamp(Katta, 4.f, 8.f);
	const float Masshtab = Olcham / Katta;
	FRandomStream R(Urug);
	// bir qatlamda nechta: paket tubi ~13 x 7 sm
	const int32 Ustun = FMath::Max(1, FMath::FloorToInt(2.f * IchkiX / Olcham));
	const int32 Qator = FMath::Max(1, FMath::FloorToInt(2.f * IchkiY / Olcham));
	const int32 Qatlamda = Ustun * Qator;
	// osilgan paket yuqoriga torayadi (tutqichlar kaftda yig'ilgan): tubidagi 3 qatlam (8 sm li mol bilan og'izdan ~20 sm pastgacha) - plyonkadan chiqib qolmasin
	Son = FMath::Min(Son, Qatlamda * 3);
	for (int32 i = 0; i < Son; ++i)
	{
		const int32 Q = i / Qatlamda, J = i % Qatlamda;
		const float X = Ustun > 1 ? FMath::Lerp(-IchkiX + Olcham * 0.5f, IchkiX - Olcham * 0.5f, float(J % Ustun) / (Ustun - 1)) : 0.f;
		const float Y = Qator > 1 ? FMath::Lerp(-IchkiY + Olcham * 0.5f, IchkiY - Olcham * 0.5f, float(J / Ustun) / (Qator - 1)) : 0.f;
		const FVector Joy(X + R.FRandRange(-1.f, 1.f), Y + R.FRandRange(-0.8f, 0.8f), Tubi + Olcham * (0.5f + 0.8f * Q));
		const FRotator Bur(R.FRandRange(-30.f, 30.f), R.FRandRange(0.f, 360.f), R.FRandRange(-30.f, 30.f));
		UStaticMeshComponent* M = NewObject<UStaticMeshComponent>(Paket->GetOwner() ? static_cast<UObject*>(Paket->GetOwner()) : Paket);
		M->ComponentTags.Add(PaketMoliTeg);
		M->SetStaticMesh(Mol);
		M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		M->SetCastShadow(false);   // paket ichida - soyasi plyonkadan chiqib qolmasin
		M->SetMobility(EComponentMobility::Movable);
		M->SetupAttachment(Paket);
		// mol meshining pivoti markazida bo'lmasligi mumkin: markazi Joy'ga tushsin
		const FVector Markaz = Bur.RotateVector(B.GetCenter() * Masshtab);
		M->SetRelativeTransform(FTransform(Bur, Joy - Markaz, FVector(Masshtab)));
		M->RegisterComponent();
	}
}
