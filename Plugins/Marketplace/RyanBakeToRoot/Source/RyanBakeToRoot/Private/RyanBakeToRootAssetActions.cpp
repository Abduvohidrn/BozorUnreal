// Copyright Ryan Roberts 2026. All Rights Reserved.

#include "RyanBakeToRootAssetActions.h"
#include "Engine/SkeletalMesh.h"
#include "SkeletonModifier.h"
#include "EditorUtilityLibrary.h"
#include "ScopedTransaction.h"

URyanBakeToRootAssetActions::URyanBakeToRootAssetActions()
{
	SupportedClasses.Add(USkeletalMesh::StaticClass());
}

void URyanBakeToRootAssetActions::AddMixamoRootBone()
{
	// Generalization: now reparents ALL current top-level bones under RootName, not just
	// HipName specifically. Real Mixamo skeletons have exactly one top-level bone ("Hips"),
	// so behavior is unchanged in practice.
	AddRootBone(RootName);
}

TArray<FName> URyanBakeToRootAssetActions::GetTopLevelBoneNames(const USkeletalMesh* SkelMesh)
{
	TArray<FName> TopLevelBoneNames;
	if (!SkelMesh) return TopLevelBoneNames;

	const FReferenceSkeleton& RefSkeleton = SkelMesh->GetRefSkeleton();
	const int32 NumBones = RefSkeleton.GetRawBoneNum();
	for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
	{
		if (RefSkeleton.GetParentIndex(BoneIndex) == INDEX_NONE)
		{
			TopLevelBoneNames.Add(RefSkeleton.GetBoneName(BoneIndex));
		}
	}
	return TopLevelBoneNames;
}

void URyanBakeToRootAssetActions::AddRootBone(FName NewRootBoneName)
{
	const FString TrimmedName = NewRootBoneName.ToString().TrimStartAndEnd();
	if (TrimmedName.IsEmpty() || NewRootBoneName.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("AddRootBone: new root bone name is empty — aborted"));
		return;
	}
	const FName TrimmedRootBoneName(*TrimmedName);

	// USkeletonModifier::CommitSkeletonToSkeletalMesh() calls Modify() but relies on an
	// externally-active transaction to record anything — provide one here so Undo works
	// regardless of caller (CallInEditor menu invocations already get one from Blutility,
	// but nesting is safe and this makes the widget-driven path work too).
	FScopedTransaction Transaction(NSLOCTEXT("RyanBakeToRoot", "AddRootBone", "Add Root Bone"));

	TArray<UObject*> SelectedAssets = UEditorUtilityLibrary::GetSelectedAssets();

	for (UObject* Asset : SelectedAssets)
	{
		USkeletalMesh* SkelMesh = Cast<USkeletalMesh>(Asset);
		if (!SkelMesh) continue;

		// Discover top-level bones from the current ref skeleton BEFORE any modifier edits —
		// otherwise the newly-added root bone would trivially be the only unparented bone.
		TArray<FName> TopLevelBoneNames = GetTopLevelBoneNames(SkelMesh);
		if (TopLevelBoneNames.Num() == 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("AddRootBone: %s has no bones — skipped"), *SkelMesh->GetName());
			continue;
		}

		USkeletonModifier* Modifier = NewObject<USkeletonModifier>();
		Modifier->SetSkeletalMesh(SkelMesh);

		TArray<FName> BoneNames = Modifier->GetAllBoneNames();

		if (BoneNames.Contains(TrimmedRootBoneName))
		{
			// root already exists - skip
			UE_LOG(LogTemp, Warning,
				TEXT("%s already exists in %s"),
				*TrimmedRootBoneName.ToString(),
				*SkelMesh->GetName());
			continue;
		}

		// Add root bone at origin with no parent
		FTransform Identity = FTransform::Identity;
		Modifier->AddBone(TrimmedRootBoneName, NAME_None, Identity);

		// Reparent every current top-level bone under the new root
		for (const FName& TopLevelBoneName : TopLevelBoneNames)
		{
			Modifier->ParentBone(TopLevelBoneName, TrimmedRootBoneName);
		}

		Modifier->CommitSkeletonToSkeletalMesh();
	}
}
