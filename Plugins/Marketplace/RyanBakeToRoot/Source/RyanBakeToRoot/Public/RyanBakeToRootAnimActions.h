// Copyright Ryan Roberts 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetActionUtility.h"
#include "RyanBakeToRootAnimActions.generated.h"

class UEditorUtilityWidgetBlueprint;

UCLASS()
class RYANBAKETOROOT_API URyanBakeToRootAnimActions : public UAssetActionUtility
{
	GENERATED_BODY()

public:
	URyanBakeToRootAnimActions();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BakeToRoot|Mixamo")
	FName RootName = TEXT("root");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BakeToRoot|Mixamo")
	FName HipName = TEXT("Hips");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BakeToRoot|Mixamo")
	FVector AimAxis = FVector(0.f, 0.f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BakeToRoot|Mixamo")
	FVector UpAxis = FVector(0.f, 1.f, 0.f);

	// EUW_RyanBakeToRoot widget blueprint spawned by OpenBakeToRootWidget(). Set once on this Blueprint's Class Defaults.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BakeToRoot|PopupWidget")
	TSoftObjectPtr<UEditorUtilityWidgetBlueprint> BakeToRootWidgetAsset;

	UFUNCTION(CallInEditor, Category = "Mixamo")
	void MixamoBakeMotionToRoot_TranslateOnly();

	UFUNCTION(CallInEditor, Category = "Mixamo")
	void MixamoBakeMotionToRoot_TranslateAndYaw();
	
	UFUNCTION(CallInEditor, Category = "Mixamo")
	void MixamoBakeMotionToRoot_ToOrigin();
	
	UFUNCTION(CallInEditor, Category = "AnimSequence")
	void SyncBoneTracksToSkeleton();

	// Opens (or focuses) EUW_RyanBakeToRoot and pre-populates its skeleton field from the first selected UAnimSequence.
	UFUNCTION(CallInEditor, Category = "PopupWidgets")
	void OpenBakeToRootWidget();

	// Translate+Yaw bake using AimAxis/UpAxis as the Hip's local forward/up axis selectors
	// (rotated into world space per-frame), with Root always kept level. Called only by
	// RyanBakeToRootWidget — intentionally not CallInEditor, so it doesn't need a Blueprint
	// pairing to stay out of the right-click "Scripted Asset Actions" menu.
	void BakeMotionToRoot_TranslateAndYaw_ConfigurableAxes();

private:
	int32 GetAxisVectorType(FVector AxisVector) const;

	FTransform GetAim(FVector PointPosition, FVector AimVector, FVector UpVector, FVector InAimAxis, FVector InUpAxis) const;

	// Hip-relative, level-locked yaw: AimAxis/UpAxis select which local Hip axis is forward/up,
	// rotated into world space via CurrentHipTransform's rotation. Root's Up is always exactly
	// world +Z (never tilts with the Hip); Root's forward is derived via a Hip-relative Right
	// vector so both AimAxis and UpAxis remain meaningful despite Root staying level.
	FTransform ComputeLevelRootTransform(const FVector& PointPosition, const FTransform& CurrentHipTransform) const;

	bool TracksMatchSkeleton(const FReferenceSkeleton& RefSkeleton, TArray<FName> ExistingTrackNames);

	// Walks the parent chain from HipBoneIndex up to RootBoneIndex, returning the bone names in
	// top-down order (Root's immediate child ... Hip, inclusive). Returns false if Root and Hip
	// are the same bone, or if Hip is not a descendant of Root.
	bool GetBoneChainRootToHip(const FReferenceSkeleton& RefSkeleton, int32 RootBoneIndex, int32 HipBoneIndex, TArray<FName>& OutChainBoneNames) const;

	void SyncBoneTracksToSequence(UAnimSequence* AnimSeq);

	void BakeMotionToRoot(UAnimSequence* AnimSeq, bool bIncludeYaw, bool bToOrigin, bool bUseConfigurableHipAxes = false);
};