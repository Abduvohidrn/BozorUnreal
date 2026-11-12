#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BozorLevelTools.generated.h"

class UBoxComponent;
class UStaticMesh;
class UMaterialInterface;
class USkeleton;
class UAnimSequence;
class ABrush;
class USkeletalMesh;
class UPhysicalMaterial;

// Bozor obyektlari: bir xil mesh va materialli nusxalar bitta ISM komponentida (Python sahna quruvchisi to'ldiradi)
UCLASS()
class BOZORUNREAL_API ABozorMeshGroup : public AActor
{
	GENERATED_BODY()

public:
	ABozorMeshGroup();

	UFUNCTION(BlueprintCallable, Category = "Bozor")
	void AddInstances(UStaticMesh* Mesh, const TArray<UMaterialInterface*>& Materials, const TArray<FTransform>& Transforms, bool bCastShadow, bool bCollision);
};

// Sotuvchi zonasi: yuruvchilar kirmaydi (Unity: NavMeshBuildSource ModifierBox, area = Not Walkable)
UCLASS()
class BOZORUNREAL_API ABozorNavBlocker : public AActor
{
	GENERATED_BODY()

public:
	ABozorNavBlocker();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	FVector Extent = FVector(100.f);

	UPROPERTY(VisibleAnywhere, Category = "Bozor")
	TObjectPtr<UBoxComponent> Box;

	virtual void OnConstruction(const FTransform& Transform) override;
};

UCLASS()
class BOZORUNREAL_API UBozorLevelTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Bitta skeletdagi turli bo'yli odamlar: suyak uzunligi har kimning o'zidan (Skeleton), faqat pelvis animatsiyadan (masshtablangan)
	UFUNCTION(BlueprintCallable, Category = "Bozor")
	static void SetupSkeletonRetargeting(USkeleton* Skeleton);

	UFUNCTION(BlueprintCallable, Category = "Bozor")
	static void SetComplexCollision(UStaticMesh* Mesh);

	UFUNCTION(BlueprintCallable, Category = "Bozor", meta = (WorldContext = "WorldContext"))
	static bool BuildNavigation(UObject* WorldContext);

	UFUNCTION(BlueprintCallable, Category = "Bozor")
	static void SetAnimRootLock(UAnimSequence* Anim, bool bLock);

	// Physical Material sirt turi (SurfaceType1..62): Python enum'ida nomlanmagan turlar ko'rinmaydi, shuning uchun raqam bilan
	UFUNCTION(BlueprintCallable, Category = "Bozor")
	static void SetSurfaceType(UPhysicalMaterial* Material, int32 SurfaceType);

	// Python spawn qilgan hajmning (NavMeshBoundsVolume) brush'i bo'sh bo'ladi: Size o'lchamli kub yasaydi
	UFUNCTION(BlueprintCallable, Category = "Bozor")
	static void MakeBoxBrush(ABrush* Brush, FVector Size);

	// Retarget qilingan nusxalarni paket Blueprint'lariga ulash: Old[i] ga bo'lgan barcha havolalar New[i] ga almashadi (eski asset o'chirilmaydi)
	UFUNCTION(BlueprintCallable, Category = "Bozor")
	static int32 ReplaceReferences(const TArray<UObject*>& Old, const TArray<UObject*>& New);

	// Paket qo'lga biriktiradigan buyumlar (telefon, sigaret, ichimlik) uchun soketlar: manekendan bozor skeletiga
	UFUNCTION(BlueprintCallable, Category = "Bozor")
	static int32 CopySockets(USkeletalMesh* FromMesh, USkeleton* To);

	// Sahnadagi aktorga komponent qo'shish (masalan BP_NaturalMovement)
	UFUNCTION(BlueprintCallable, Category = "Bozor")
	static UActorComponent* AddActorComponent(AActor* Actor, TSubclassOf<UActorComponent> ComponentClass, FName Name);

	UFUNCTION(BlueprintCallable, Category = "Bozor")
	static void RerunConstruction(AActor* Actor);

	// Blueprint o'zgaruvchisini matndan o'rnatish (bo'shliqli nomlar, foydalanuvchi enum'lari uchun)
	UFUNCTION(BlueprintCallable, Category = "Bozor")
	static bool SetPropertyText(UObject* Object, FName Property, const FString& Value);

	// Blueprint'da e'lon qilingan o'zgaruvchilar (nomi, turi, qiymati) va funksiyalar/hodisalar ro'yxati - begona BP'ni o'rganish uchun
	UFUNCTION(BlueprintCallable, Category = "Bozor")
	static FString DescribeObject(UObject* Object);
};
