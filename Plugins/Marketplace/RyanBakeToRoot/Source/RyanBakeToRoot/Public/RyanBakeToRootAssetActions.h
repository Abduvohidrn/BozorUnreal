// Copyright Ryan Roberts 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetActionUtility.h"
#include "RyanBakeToRootAssetActions.generated.h"

class USkeletalMesh;

UCLASS()
class RYANBAKETOROOT_API URyanBakeToRootAssetActions : public UAssetActionUtility
{
	GENERATED_BODY()

public:
	URyanBakeToRootAssetActions();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mixamo")
	FName RootName = TEXT("root");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mixamo")
	FName HipName = TEXT("Hips");

	UFUNCTION(CallInEditor, Category = "Mixamo")
	void AddMixamoRootBone();

	// Generic, skeleton-agnostic root-bone insertion. For each selected USkeletalMesh: finds all
	// current top-level bones (no parent in the mesh's reference skeleton) and reparents all of
	// them under a newly added bone named NewRootBoneName. Skips (logs a warning for) any mesh
	// whose bone list already contains NewRootBoneName. Reachable only via the AddRootBone widget
	// button — intentionally BlueprintCallable, not CallInEditor.
	UFUNCTION(BlueprintCallable, Category = "Bake To Root")
	void AddRootBone(FName NewRootBoneName);

private:
	static TArray<FName> GetTopLevelBoneNames(const USkeletalMesh* SkelMesh);
};