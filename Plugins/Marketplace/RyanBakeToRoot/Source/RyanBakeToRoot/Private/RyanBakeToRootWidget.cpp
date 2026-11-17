// Copyright Ryan Roberts 2026. All Rights Reserved.

#include "RyanBakeToRootWidget.h"

#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/SinglePropertyView.h"
#include "Editor.h"
#include "EditorUtilityLibrary.h"
#include "Engine/SkeletalMesh.h"
#include "Misc/MessageDialog.h"
#include "RyanBakeToRootAnimActions.h"
#include "RyanBakeToRootAssetActions.h"

void URyanBakeToRootWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (HipBone_ComboBox)
	{
		HipBone_ComboBox->OnSelectionChanged.AddDynamic(this, &URyanBakeToRootWidget::OnHipBoneSelectionChanged);
	}
	if (RootBone_ComboBox)
	{
		RootBone_ComboBox->OnSelectionChanged.AddDynamic(this, &URyanBakeToRootWidget::OnRootBoneSelectionChanged);
	}
	if (BakeToRoot_TranslateOnly_Button)
	{
		BakeToRoot_TranslateOnly_Button->OnClicked.AddDynamic(this, &URyanBakeToRootWidget::OnBakeTranslateOnlyClicked);
	}
	if (BakeToRoot_TranslateYaw_Button)
	{
		BakeToRoot_TranslateYaw_Button->OnClicked.AddDynamic(this, &URyanBakeToRootWidget::OnBakeTranslateYawClicked);
	}
	if (BakeToRoot_ToOrigin_Button)
	{
		BakeToRoot_ToOrigin_Button->OnClicked.AddDynamic(this, &URyanBakeToRootWidget::OnBakeToOriginClicked);
	}
	if (AddRootBone_Button)
	{
		AddRootBone_Button->OnClicked.AddDynamic(this, &URyanBakeToRootWidget::OnAddRootBoneClicked);
	}

	FEditorDelegates::PostUndoRedo.AddUObject(this, &URyanBakeToRootWidget::OnPostUndoRedo);

	RefreshSkeletonPropertyView();
	RefreshBoneComboBoxes();
}

void URyanBakeToRootWidget::NativeDestruct()
{
	FEditorDelegates::PostUndoRedo.RemoveAll(this);
	Super::NativeDestruct();
}

#if WITH_EDITOR
void URyanBakeToRootWidget::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(URyanBakeToRootWidget, SelectedSkeleton))
	{
		RefreshBoneComboBoxes();
	}
}
#endif

void URyanBakeToRootWidget::SetSkeleton(USkeleton* InSkeleton)
{
	SelectedSkeleton = InSkeleton;
	RefreshSkeletonPropertyView();
	RefreshBoneComboBoxes();
}

void URyanBakeToRootWidget::RefreshSkeletonPropertyView()
{
	if (!Skeleton_SinglePropertyView) return;

	Skeleton_SinglePropertyView->SetObject(this);
	Skeleton_SinglePropertyView->SetPropertyName(GET_MEMBER_NAME_CHECKED(URyanBakeToRootWidget, SelectedSkeleton));
	Skeleton_SinglePropertyView->SetNameOverride(FText::FromString(TEXT("Skeleton")));
}

void URyanBakeToRootWidget::RefreshBoneComboBoxes()
{
	if (!HipBone_ComboBox && !RootBone_ComboBox) return;

	if (HipBone_ComboBox) HipBone_ComboBox->ClearOptions();
	if (RootBone_ComboBox) RootBone_ComboBox->ClearOptions();

	if (!SelectedSkeleton) return;

	const FReferenceSkeleton& RefSkeleton = SelectedSkeleton->GetReferenceSkeleton();
	const int32 NumBones = RefSkeleton.GetRawBoneNum();
	for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
	{
		const FString BoneName = RefSkeleton.GetBoneName(BoneIndex).ToString();
		if (HipBone_ComboBox) HipBone_ComboBox->AddOption(BoneName);
		if (RootBone_ComboBox) RootBone_ComboBox->AddOption(BoneName);
	}

	const FName ResolvedHipName = ResolveDefaultBoneName(RefSkeleton, TEXT("Hips"), { TEXT("hip"), TEXT("pelvis") });
	const FName ResolvedRootName = ResolveDefaultBoneName(RefSkeleton, TEXT("root"), { TEXT("root") });

	if (HipBone_ComboBox && !ResolvedHipName.IsNone())
	{
		HipBone_ComboBox->SetSelectedOption(ResolvedHipName.ToString());
		SelectedHipBone = ResolvedHipName;
	}
	if (RootBone_ComboBox && !ResolvedRootName.IsNone())
	{
		RootBone_ComboBox->SetSelectedOption(ResolvedRootName.ToString());
		SelectedRootBone = ResolvedRootName;
	}
}

FName URyanBakeToRootWidget::ResolveDefaultBoneName(const FReferenceSkeleton& RefSkeleton, FName ExactName, const TArray<FString>& FallbackKeywords)
{
	if (RefSkeleton.FindBoneIndex(ExactName) != INDEX_NONE)
	{
		return ExactName;
	}

	const int32 NumBones = RefSkeleton.GetRawBoneNum();
	int32 BestBoneIndex = INDEX_NONE;
	int32 BestDepth = MAX_int32;

	for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
	{
		const FString BoneName = RefSkeleton.GetBoneName(BoneIndex).ToString();
		const bool bMatches = FallbackKeywords.ContainsByPredicate([&BoneName](const FString& Keyword)
		{
			return BoneName.Contains(Keyword, ESearchCase::IgnoreCase);
		});
		if (!bMatches) continue;

		int32 Depth = 0;
		for (int32 Parent = RefSkeleton.GetParentIndex(BoneIndex); Parent != INDEX_NONE; Parent = RefSkeleton.GetParentIndex(Parent))
		{
			++Depth;
		}

		if (Depth < BestDepth)
		{
			BestDepth = Depth;
			BestBoneIndex = BoneIndex;
		}
	}

	return BestBoneIndex != INDEX_NONE ? RefSkeleton.GetBoneName(BestBoneIndex) : NAME_None;
}

void URyanBakeToRootWidget::OnHipBoneSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	SelectedHipBone = FName(*SelectedItem);
}

void URyanBakeToRootWidget::OnRootBoneSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	SelectedRootBone = FName(*SelectedItem);
}

FVector URyanBakeToRootWidget::AxisStringToVector(const FString& AxisOption)
{
	if (AxisOption == TEXT("X+")) return FVector(1.f, 0.f, 0.f);
	if (AxisOption == TEXT("X-")) return FVector(-1.f, 0.f, 0.f);
	if (AxisOption == TEXT("Y+")) return FVector(0.f, 1.f, 0.f);
	if (AxisOption == TEXT("Y-")) return FVector(0.f, -1.f, 0.f);
	if (AxisOption == TEXT("Z+")) return FVector(0.f, 0.f, 1.f);
	if (AxisOption == TEXT("Z-")) return FVector(0.f, 0.f, -1.f);
	return FVector::ZeroVector;
}

void URyanBakeToRootWidget::OnBakeTranslateOnlyClicked()
{
	ExecuteBake(/*bIncludeYaw=*/ false, /*bToOrigin=*/ false);
}

void URyanBakeToRootWidget::OnBakeTranslateYawClicked()
{
	ExecuteBake(/*bIncludeYaw=*/ true, /*bToOrigin=*/ false);
}

void URyanBakeToRootWidget::OnBakeToOriginClicked()
{
	ExecuteBake(/*bIncludeYaw=*/ false, /*bToOrigin=*/ true);
}

void URyanBakeToRootWidget::ExecuteBake(bool bIncludeYaw, bool bToOrigin)
{
	if (SelectedHipBone.IsNone() || SelectedRootBone.IsNone() || SelectedHipBone == SelectedRootBone)
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			FText::FromString(TEXT("Select a valid Hip Bone and Root Bone (distinct from each other) before baking.")));
		return;
	}

	FVector AimAxisVector = FVector::ZeroVector;
	FVector UpAxisVector = FVector::ZeroVector;

	if (bIncludeYaw)
	{
		const FString AimOption = AimAxis_ComboBoxString ? AimAxis_ComboBoxString->GetSelectedOption() : FString();
		const FString UpOption = UpAxis_ComboBoxString ? UpAxis_ComboBoxString->GetSelectedOption() : FString();

		if (AimOption.IsEmpty() || UpOption.IsEmpty())
		{
			FMessageDialog::Open(EAppMsgType::Ok,
				FText::FromString(TEXT("Select an Aim Axis and Up Axis before baking.")));
			return;
		}

		AimAxisVector = AxisStringToVector(AimOption);
		UpAxisVector = AxisStringToVector(UpOption);

		auto AxisLetter = [](const FVector& V) { return !FMath::IsNearlyZero(V.X) ? 0 : (!FMath::IsNearlyZero(V.Y) ? 1 : 2); };
		if (AxisLetter(AimAxisVector) == AxisLetter(UpAxisVector))
		{
			FMessageDialog::Open(EAppMsgType::Ok,
				FText::FromString(TEXT("Select an Aim Axis and Up Axis on different axes (X/Y/Z) before baking.")));
			return;
		}
	}

	TArray<UObject*> SelectedAssets = UEditorUtilityLibrary::GetSelectedAssets();
	bool bHasAnimSequence = false;
	for (UObject* Asset : SelectedAssets)
	{
		if (Asset && Asset->IsA<UAnimSequence>())
		{
			bHasAnimSequence = true;
			break;
		}
	}
	if (!bHasAnimSequence)
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			FText::FromString(TEXT("Select one or more Animation Sequences in the Content Browser before baking.")));
		return;
	}

	URyanBakeToRootAnimActions* Baker = NewObject<URyanBakeToRootAnimActions>();
	Baker->RootName = SelectedRootBone;
	Baker->HipName = SelectedHipBone;

	if (bIncludeYaw)
	{
		Baker->AimAxis = AimAxisVector;
		Baker->UpAxis = UpAxisVector;
	}

	if (bToOrigin)
	{
		Baker->MixamoBakeMotionToRoot_ToOrigin();
	}
	else if (bIncludeYaw)
	{
		Baker->BakeMotionToRoot_TranslateAndYaw_ConfigurableAxes();
	}
	else
	{
		Baker->MixamoBakeMotionToRoot_TranslateOnly();
	}
}

void URyanBakeToRootWidget::OnAddRootBoneClicked()
{
	ExecuteAddRootBone();
}

void URyanBakeToRootWidget::ExecuteAddRootBone()
{
	const FString TrimmedName = AddRootBone_Name_TextBox
		? AddRootBone_Name_TextBox->GetText().ToString().TrimStartAndEnd()
		: FString();

	if (TrimmedName.IsEmpty())
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			FText::FromString(TEXT("Enter a name for the new root bone before continuing.")));
		return;
	}

	TArray<UObject*> SelectedAssets = UEditorUtilityLibrary::GetSelectedAssets();
	bool bHasSkeletalMesh = false;
	for (UObject* Asset : SelectedAssets)
	{
		if (Asset && Asset->IsA<USkeletalMesh>())
		{
			bHasSkeletalMesh = true;
			break;
		}
	}
	if (!bHasSkeletalMesh)
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			FText::FromString(TEXT("Select one or more Skeletal Meshes in the Content Browser before continuing.")));
		return;
	}

	URyanBakeToRootAssetActions* AssetActions = NewObject<URyanBakeToRootAssetActions>();
	AssetActions->AddRootBone(FName(*TrimmedName));

	RefreshBoneComboBoxes();
}

void URyanBakeToRootWidget::OnPostUndoRedo()
{
	RefreshBoneComboBoxes();
}
