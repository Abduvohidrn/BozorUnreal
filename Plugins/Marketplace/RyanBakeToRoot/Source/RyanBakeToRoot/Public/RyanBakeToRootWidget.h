// Copyright Ryan Roberts 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorUtilityWidget.h"
#include "RyanBakeToRootWidget.generated.h"

class USinglePropertyView;
class USkeleton;
class UComboBoxString;
class UButton;
class UEditableTextBox;

/**
 * C++ backing class for the EUW_RyanBakeToRoot Editor Utility Widget.
 * Hosts the skeleton picker (Skeleton_SinglePropertyView) and the Hip/Root
 * bone pickers (HipBone_ComboBox/RootBone_ComboBox) used by the bake-to-root workflow.
 */
UCLASS()
class RYANBAKETOROOT_API URyanBakeToRootWidget : public UEditorUtilityWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bake To Root")
	USkeleton* SelectedSkeleton = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bake To Root")
	FName SelectedHipBone;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bake To Root")
	FName SelectedRootBone;

	UFUNCTION(BlueprintCallable, Category = "Bake To Root")
	void SetSkeleton(USkeleton* InSkeleton);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	// Bound by variable name to Skeleton_SinglePropertyView on EUW_RyanBakeToRoot.
	// Widget must be marked "Is Variable" in the Designer for this bind to succeed.
	UPROPERTY(meta = (BindWidget))
	USinglePropertyView* Skeleton_SinglePropertyView = nullptr;

	// Bound by variable name to HipBone_ComboBox/RootBone_ComboBox on EUW_RyanBakeToRoot.
	// Widgets must be marked "Is Variable" in the Designer for these binds to succeed.
	UPROPERTY(meta = (BindWidget))
	UComboBoxString* HipBone_ComboBox = nullptr;

	UPROPERTY(meta = (BindWidget))
	UComboBoxString* RootBone_ComboBox = nullptr;

	// Bound by variable name to AimAxis_ComboBoxString/UpAxis_ComboBoxString on EUW_RyanBakeToRoot.
	// Widgets must be marked "Is Variable" in the Designer for these binds to succeed.
	// Read directly (not tracked via OnSelectionChanged) only when Translate+Yaw is baked.
	UPROPERTY(meta = (BindWidget))
	UComboBoxString* AimAxis_ComboBoxString = nullptr;

	UPROPERTY(meta = (BindWidget))
	UComboBoxString* UpAxis_ComboBoxString = nullptr;

	// Bound by variable name to BakeToRoot_TranslateOnly_Button/BakeToRoot_TranslateYaw_Button/BakeToRoot_ToOrigin_Button on EUW_RyanBakeToRoot.
	// Widgets must be marked "Is Variable" in the Designer for these binds to succeed.
	UPROPERTY(meta = (BindWidget))
	UButton* BakeToRoot_TranslateOnly_Button = nullptr;

	UPROPERTY(meta = (BindWidget))
	UButton* BakeToRoot_TranslateYaw_Button = nullptr;

	UPROPERTY(meta = (BindWidget))
	UButton* BakeToRoot_ToOrigin_Button = nullptr;

	// Bound by variable name to AddRootBone_Button/AddRootBone_Name_TextBox on EUW_RyanBakeToRoot.
	// Widgets must be marked "Is Variable" in the Designer for these binds to succeed.
	UPROPERTY(meta = (BindWidget))
	UButton* AddRootBone_Button = nullptr;

	UPROPERTY(meta = (BindWidget))
	UEditableTextBox* AddRootBone_Name_TextBox = nullptr;

	void RefreshSkeletonPropertyView();
	void RefreshBoneComboBoxes();

	// Resolves a bone to auto-select for a picker: tries ExactName first (case-insensitive, per
	// FName semantics), then falls back to scanning all bones for names containing any of
	// FallbackKeywords (case-insensitive). If multiple fallback candidates match, the one
	// closest to the skeleton root (fewest ancestors) wins, matching "prefer the parent"; ties
	// are broken by lowest bone index. Returns NAME_None if nothing matches.
	static FName ResolveDefaultBoneName(const FReferenceSkeleton& RefSkeleton, FName ExactName, const TArray<FString>& FallbackKeywords);

	// Inverse of the "X+"/"X-"/"Y+"/"Y-"/"Z+"/"Z-" combo box options.
	static FVector AxisStringToVector(const FString& AxisOption);

	UFUNCTION()
	void OnHipBoneSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void OnRootBoneSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void OnBakeTranslateOnlyClicked();

	UFUNCTION()
	void OnBakeTranslateYawClicked();

	UFUNCTION()
	void OnBakeToOriginClicked();

	UFUNCTION()
	void OnAddRootBoneClicked();

	// Shared validation + dispatch for all three bake buttons.
	void ExecuteBake(bool bIncludeYaw, bool bToOrigin);

	// Validation + dispatch for the AddRootBone button.
	void ExecuteAddRootBone();

	// Keeps the Hip/Root combo boxes in sync when an Undo/Redo changes the selected
	// skeleton's bone tree out from under this widget (e.g. undoing an AddRootBone call).
	void OnPostUndoRedo();
};
