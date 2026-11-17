// Copyright Ryan Roberts 2026. All Rights Reserved.

#include "RyanBakeToRootAnimActions.h"

#include "Animation/AnimSequence.h"
#include "EditorUtilityLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Editor.h"
#include "EditorUtilitySubsystem.h"
#include "EditorUtilityWidgetBlueprint.h"
#include "RyanBakeToRootWidget.h"
#include "ScopedTransaction.h"

URyanBakeToRootAnimActions::URyanBakeToRootAnimActions()
{
    SupportedClasses.Add(UAnimSequence::StaticClass());
}

int32 URyanBakeToRootAnimActions::GetAxisVectorType(FVector AxisVector) const
{
    if (AxisVector.X >  0.f) return 0; // +X
    if (AxisVector.X <  0.f) return 1; // -X
    if (AxisVector.Y >  0.f) return 2; // +Y
    if (AxisVector.Y <  0.f) return 3; // -Y
    if (AxisVector.Z >  0.f) return 4; // +Z
    if (AxisVector.Z <  0.f) return 5; // -Z
    return -1;
}

FTransform URyanBakeToRootAnimActions::GetAim(FVector PointPosition, FVector AimVector, FVector UpVector, FVector InAimAxis, FVector InUpAxis) const
{
    bool X_Set = false;
    bool Y_Set = false;
    bool Z_Set = false;
    
    FVector NormalizedAim = AimVector.GetSafeNormal();
    FVector NormalizedUp  = UpVector.GetSafeNormal();
    
    FVector NormalizedRight = FVector::CrossProduct(NormalizedAim, NormalizedUp).GetSafeNormal();
    FVector NormalizedCorrectedUp = FVector::CrossProduct(NormalizedRight, NormalizedAim).GetSafeNormal();

    int32 AimType = GetAxisVectorType(InAimAxis);
    int32 UpType  = GetAxisVectorType(InUpAxis);

    FVector X_Vector = FVector::ZeroVector;
    FVector Y_Vector = FVector::ZeroVector;
    FVector Z_Vector = FVector::ZeroVector;
    FVector Aim_Vector = FVector::ZeroVector;
    FVector Up_Vector = FVector::ZeroVector;
    FVector Right_Vector = FVector::ZeroVector;

    switch (AimType)
    {
        case 0: X_Vector = NormalizedAim; Aim_Vector = X_Vector; X_Set=true; break;
        case 1: X_Vector = NormalizedAim*-1; Aim_Vector = X_Vector; X_Set=true; break;
        case 2: Y_Vector = NormalizedAim; Aim_Vector = Y_Vector; Y_Set=true; break;
        case 3: Y_Vector = NormalizedAim*-1; Aim_Vector = Y_Vector; Y_Set=true; break;
        case 4: Z_Vector = NormalizedAim; Aim_Vector = Z_Vector; Z_Set=true; break;
        case 5: Z_Vector = NormalizedAim*-1; Aim_Vector = Z_Vector; Z_Set=true; break;
        default: break;
    }

    switch (UpType)
    {
        case 0: X_Vector = NormalizedCorrectedUp; Up_Vector = X_Vector; X_Set=true; break;
        case 1: X_Vector = NormalizedCorrectedUp*-1; Up_Vector = X_Vector; X_Set=true; break;
        case 2: Y_Vector = NormalizedCorrectedUp; Up_Vector = Y_Vector; Y_Set=true; break;
        case 3: Y_Vector = NormalizedCorrectedUp*-1; Up_Vector = Y_Vector; Y_Set=true; break;
        case 4: Z_Vector = NormalizedCorrectedUp; Up_Vector = Z_Vector; Z_Set=true; break;
        case 5: Z_Vector = NormalizedCorrectedUp*-1; Up_Vector = Z_Vector; Z_Set=true; break;
        default: break;
    }
    
    Right_Vector = FVector::CrossProduct(Aim_Vector, Up_Vector).GetSafeNormal();
    
    if (!X_Set) X_Vector = Right_Vector;
    if (!Y_Set) Y_Vector = Right_Vector;
    if (!Z_Set) Z_Vector = Right_Vector;

    //FMatrix Basis(X_Vector, Y_Vector, Z_Vector, PointPosition);
    //return FTransform(Basis);
    
    FMatrix RotMatrix = FRotationMatrix::MakeFromZY(Z_Vector, Y_Vector);
    FTransform Result;
    Result.SetLocation(PointPosition);
    Result.SetRotation(RotMatrix.ToQuat());
    Result.SetScale3D(FVector::OneVector);
    return Result;
}

FTransform URyanBakeToRootAnimActions::ComputeLevelRootTransform(const FVector& PointPosition, const FTransform& CurrentHipTransform) const
{
    const FVector HipAimWorld = CurrentHipTransform.GetRotation().RotateVector(AimAxis);
    const FVector HipUpWorld = CurrentHipTransform.GetRotation().RotateVector(UpAxis);
    const FVector HipRightWorld = FVector::CrossProduct(HipAimWorld, HipUpWorld).GetSafeNormal();

    static const FVector WorldUp = FVector(0.f, 0.f, 1.f);
    const FVector LevelForward = FVector::CrossProduct(WorldUp, HipRightWorld).GetSafeNormal();

    FMatrix RotMatrix = FRotationMatrix::MakeFromZY(WorldUp, LevelForward);
    FTransform Result;
    Result.SetLocation(PointPosition);
    Result.SetRotation(RotMatrix.ToQuat());
    Result.SetScale3D(FVector::OneVector);
    return Result;
}

bool URyanBakeToRootAnimActions::TracksMatchSkeleton(const FReferenceSkeleton& RefSkeleton, TArray<FName> ExistingTrackNames)
{
    const int32 NumBones = RefSkeleton.GetRawBoneNum();
    bool bAlreadyInSync = (ExistingTrackNames.Num() == NumBones);
    if (bAlreadyInSync)
    {
        for (int32 i = 0; i < NumBones; i++)
        {
            if (ExistingTrackNames[i] != RefSkeleton.GetBoneName(i))
            {
                bAlreadyInSync = false;
                break;
            }
        }
    }
    return bAlreadyInSync;
}

bool URyanBakeToRootAnimActions::GetBoneChainRootToHip(const FReferenceSkeleton& RefSkeleton, int32 RootBoneIndex, int32 HipBoneIndex, TArray<FName>& OutChainBoneNames) const
{
    OutChainBoneNames.Reset();
    if (RootBoneIndex == INDEX_NONE || HipBoneIndex == INDEX_NONE) return false;
    if (RootBoneIndex == HipBoneIndex) return false;

    TArray<int32> ReverseChain;
    int32 Current = HipBoneIndex;
    while (Current != INDEX_NONE)
    {
        if (Current == RootBoneIndex)
        {
            for (int32 i = ReverseChain.Num() - 1; i >= 0; --i)
            {
                OutChainBoneNames.Add(RefSkeleton.GetBoneName(ReverseChain[i]));
            }
            return true;
        }
        ReverseChain.Add(Current);
        Current = RefSkeleton.GetParentIndex(Current);
    }
    return false;
}

void URyanBakeToRootAnimActions::SyncBoneTracksToSequence(UAnimSequence* AnimSeq)
{
       if (!AnimSeq) return;

    USkeleton* Skeleton = AnimSeq->GetSkeleton();
    if (!Skeleton) return;

    const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();
    int32 NumBones = RefSkeleton.GetRawBoneNum();
    int32 NumFrames = AnimSeq->GetNumberOfSampledKeys();

    IAnimationDataController& Controller = AnimSeq->GetController();
    const IAnimationDataModel* Model = Controller.GetModel();

    // Get existing track names
    TArray<FName> ExistingTrackNames;
    Model->GetBoneTrackNames(ExistingTrackNames);
    
    // Check if tracks already match skeleton in count, names and order
    if (TracksMatchSkeleton(RefSkeleton, ExistingTrackNames)) return;

    // Build set of valid skeleton bone names for quick lookup
    TSet<FName> SkeletonBoneNames;
    for (int32 i = 0; i < NumBones; i++)
    {
        SkeletonBoneNames.Add(RefSkeleton.GetBoneName(i));
    }

    // Snapshot existing track data, skipping tracks not in skeleton
    TMap<FName, TArray<FTransform>> TrackData;
    for (const FName& TrackName : ExistingTrackNames)
    {
        if (SkeletonBoneNames.Contains(TrackName))
        {
            TArray<FTransform> Transforms;
            Model->GetBoneTrackTransforms(TrackName, Transforms);
            TrackData.Add(TrackName, Transforms);
        }
        // tracks not in skeleton are simply not stored — they get dropped
    }

    // Build identity keys array for missing tracks
    TArray<FVector> IdentityPos;
    TArray<FQuat>   IdentityRot;
    TArray<FVector> IdentityScale;
    IdentityPos.Init(FVector::ZeroVector, NumFrames);
    IdentityRot.Init(FQuat::Identity, NumFrames);
    IdentityScale.Init(FVector::OneVector, NumFrames);

    /********************************************************
     * Open Bracket - Sync Bone Tracks To Skeleton
     ********************************************************/
    
    Controller.OpenBracket(FText::FromString("Sync Bone Tracks To Skeleton"));

    // Remove all existing tracks
    for (const FName& TrackName : ExistingTrackNames)
    {
        Controller.RemoveBoneTrack(TrackName);
    }

    // Re-add in skeleton hierarchy order
    for (int32 i = 0; i < NumBones; i++)
    {
        FName BoneName = RefSkeleton.GetBoneName(i);
        Controller.AddBoneCurve(BoneName);

        if (TrackData.Contains(BoneName))
        {
            // Restore existing key data
            TArray<FVector> PosKeys;
            TArray<FQuat>   RotKeys;
            TArray<FVector> ScaleKeys;

            for (const FTransform& T : TrackData[BoneName])
            {
                PosKeys.Add(T.GetLocation());
                RotKeys.Add(T.GetRotation());
                ScaleKeys.Add(T.GetScale3D());
            }

            Controller.SetBoneTrackKeys(BoneName, PosKeys, RotKeys, ScaleKeys);
        }
        else
        {
            // No existing track — write identity keys
            Controller.SetBoneTrackKeys(BoneName, IdentityPos, IdentityRot, IdentityScale);
        }
    }

    Controller.NotifyPopulated();
    Controller.CloseBracket();

    /********************************************************
     * Close Bracket - Sync Bone Tracks To Skeleton
     ********************************************************/
}

void URyanBakeToRootAnimActions::BakeMotionToRoot(UAnimSequence* AnimSeq, bool bIncludeYaw, bool bToOrigin, bool bUseConfigurableHipAxes)
{
    USkeleton* Skeleton = AnimSeq ? AnimSeq->GetSkeleton() : nullptr;
    if (!Skeleton)
    {
        UE_LOG(LogTemp, Warning, TEXT("BakeMotionToRoot: %s has no Skeleton — skipped"), AnimSeq ? *AnimSeq->GetName() : TEXT("<null>"));
        return;
    }

    const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();
    const int32 RootBoneIndex = RefSkeleton.FindBoneIndex(RootName);
    const int32 HipBoneIndex  = RefSkeleton.FindBoneIndex(HipName);

    // Top-down order: [Root's immediate child, ..., Hip]
    TArray<FName> ChainBoneNames;
    if (!GetBoneChainRootToHip(RefSkeleton, RootBoneIndex, HipBoneIndex, ChainBoneNames))
    {
        UE_LOG(LogTemp, Warning, TEXT("BakeMotionToRoot: %s — '%s' is not a descendant of '%s' (or they are the same bone) — skipped"),
            *AnimSeq->GetName(), *HipName.ToString(), *RootName.ToString());
        return;
    }

    int32 NumFrames = AnimSeq->GetNumberOfSampledKeys();

    TArray<FVector> RootPosKeys, RootScaleKeys, HipPosKeys, HipScaleKeys;
    TArray<FQuat>   RootRotKeys, HipRotKeys;

    RootPosKeys.Reserve(NumFrames);
    RootRotKeys.Reserve(NumFrames);
    RootScaleKeys.Reserve(NumFrames);
    HipPosKeys.Reserve(NumFrames);
    HipRotKeys.Reserve(NumFrames);
    HipScaleKeys.Reserve(NumFrames);

    SyncBoneTracksToSequence(AnimSeq);

    // Re-fetch model after sync

    IAnimationDataController& Controller = AnimSeq->GetController();
    const IAnimationDataModel* Model = AnimSeq->GetController().GetModel();

    for (int32 Frame = 0; Frame < NumFrames; Frame++)
    {
        FTransform CurrentRootTransform = Model->GetBoneTrackTransform(RootName, Frame);

        // Walk down the chain, composing the OLD world transform through to Hip (inclusive).
        TArray<FTransform> ChainLocalTransforms;
        ChainLocalTransforms.Reserve(ChainBoneNames.Num());
        FTransform CurrentHipTransform = CurrentRootTransform;
        for (const FName& ChainBoneName : ChainBoneNames)
        {
            FTransform LocalTransform = Model->GetBoneTrackTransform(ChainBoneName, Frame);
            ChainLocalTransforms.Add(LocalTransform);
            CurrentHipTransform = LocalTransform * CurrentHipTransform;
        }

        FVector  CurrentHipLocation = CurrentHipTransform.GetLocation();
        FRotator CurrentHipRotation = CurrentHipTransform.GetRotation().Rotator();
        FRotator CurrentRootRotation = CurrentRootTransform.GetRotation().Rotator();

        FVector PointPosition = FVector(CurrentHipLocation.X, CurrentHipLocation.Y, 0.f);

        FTransform NewRootTransform;

        if (bIncludeYaw)
        {
            if (bUseConfigurableHipAxes)
            {
                NewRootTransform = ComputeLevelRootTransform(PointPosition, CurrentHipTransform);
            }
            else
            {
                FVector CurrentHipForwardVector = UKismetMathLibrary::GetUpVector(CurrentHipRotation);
                FVector CurrentRootUpVector     = UKismetMathLibrary::GetUpVector(CurrentRootRotation);

                NewRootTransform = GetAim(
                    PointPosition,
                    CurrentRootUpVector,
                    CurrentHipForwardVector,
                    FVector(0.f, 0.f, 1.f),
                    FVector(0.f, 1.f, 0.f)
                );
            }
        }
        else
        {
            // Translate only — root stays at origin rotation
            FVector RootLocation = bToOrigin ? FVector::ZeroVector : PointPosition;
            NewRootTransform.SetLocation(RootLocation);
            NewRootTransform.SetRotation(FQuat::Identity);
            NewRootTransform.SetScale3D(FVector::OneVector);
        }

        // Compose the NEW world transform of Hip's immediate parent, reusing the unchanged
        // intermediate bone locals (every chain entry except Hip itself).
        FTransform NewHipParentWorldTransform = NewRootTransform;
        for (int32 i = 0; i < ChainLocalTransforms.Num() - 1; ++i)
        {
            NewHipParentWorldTransform = ChainLocalTransforms[i] * NewHipParentWorldTransform;
        }

        FTransform NewHipTransform = UKismetMathLibrary::MakeRelativeTransform(
            CurrentHipTransform, NewHipParentWorldTransform);

        RootPosKeys.Add(NewRootTransform.GetLocation());
        RootRotKeys.Add(NewRootTransform.GetRotation());
        RootScaleKeys.Add(NewRootTransform.GetScale3D());

        HipPosKeys.Add(NewHipTransform.GetLocation());
        HipRotKeys.Add(NewHipTransform.GetRotation());
        HipScaleKeys.Add(NewHipTransform.GetScale3D());
    }

    Controller.OpenBracket(FText::FromString("Bake Motion To Root"));

    Controller.SetBoneTrackKeys(RootName, RootPosKeys, RootRotKeys, RootScaleKeys);
    Controller.SetBoneTrackKeys(HipName,  HipPosKeys,  HipRotKeys,  HipScaleKeys);

    Controller.NotifyPopulated();
    Controller.CloseBracket();
}

void URyanBakeToRootAnimActions::MixamoBakeMotionToRoot_TranslateOnly()
{
    FScopedTransaction Transaction(NSLOCTEXT("RyanBakeToRoot", "BakeMotionToRootTranslateOnly", "Bake Motion To Root (Translate Only)"));

    TArray<UObject*> SelectedAssets = UEditorUtilityLibrary::GetSelectedAssets();
    for (UObject* Asset : SelectedAssets)
    {
        UAnimSequence* AnimSeq = Cast<UAnimSequence>(Asset);
        if (!AnimSeq) continue;
        BakeMotionToRoot(AnimSeq, false, false);
    }
}

void URyanBakeToRootAnimActions::MixamoBakeMotionToRoot_TranslateAndYaw()
{
    FScopedTransaction Transaction(NSLOCTEXT("RyanBakeToRoot", "BakeMotionToRootTranslateAndYaw", "Bake Motion To Root (Translate + Yaw)"));

    TArray<UObject*> SelectedAssets = UEditorUtilityLibrary::GetSelectedAssets();
    for (UObject* Asset : SelectedAssets)
    {
        UAnimSequence* AnimSeq = Cast<UAnimSequence>(Asset);
        if (!AnimSeq) continue;
        BakeMotionToRoot(AnimSeq, true, false);
    }
}

void URyanBakeToRootAnimActions::BakeMotionToRoot_TranslateAndYaw_ConfigurableAxes()
{
    FScopedTransaction Transaction(NSLOCTEXT("RyanBakeToRoot", "BakeMotionToRootTranslateAndYawConfigurableAxes", "Bake Motion To Root (Translate + Yaw)"));

    TArray<UObject*> SelectedAssets = UEditorUtilityLibrary::GetSelectedAssets();
    for (UObject* Asset : SelectedAssets)
    {
        UAnimSequence* AnimSeq = Cast<UAnimSequence>(Asset);
        if (!AnimSeq) continue;
        BakeMotionToRoot(AnimSeq, true, false, true);
    }
}

void URyanBakeToRootAnimActions::MixamoBakeMotionToRoot_ToOrigin()
{
    FScopedTransaction Transaction(NSLOCTEXT("RyanBakeToRoot", "BakeMotionToRootToOrigin", "Bake Motion To Root (To Origin)"));

    TArray<UObject*> SelectedAssets = UEditorUtilityLibrary::GetSelectedAssets();
    for (UObject* Asset : SelectedAssets)
    {
        UAnimSequence* AnimSeq = Cast<UAnimSequence>(Asset);
        if (!AnimSeq) continue;
        BakeMotionToRoot(AnimSeq, false, true);
    }
}

void URyanBakeToRootAnimActions::SyncBoneTracksToSkeleton()
{
    FScopedTransaction Transaction(NSLOCTEXT("RyanBakeToRoot", "SyncBoneTracksToSkeleton", "Sync Bone Tracks To Skeleton"));

    TArray<UObject*> SelectedAssets = UEditorUtilityLibrary::GetSelectedAssets();

    for (UObject* Asset : SelectedAssets)
    {
        UAnimSequence* AnimSeq = Cast<UAnimSequence>(Asset);
        if (!AnimSeq) continue;

        SyncBoneTracksToSequence(AnimSeq);
    }
}

void URyanBakeToRootAnimActions::OpenBakeToRootWidget()
{
    TArray<UObject*> SelectedAssets = UEditorUtilityLibrary::GetSelectedAssets();

    UAnimSequence* SelectedAnimSequence = nullptr;
    for (UObject* Asset : SelectedAssets)
    {
        SelectedAnimSequence = Cast<UAnimSequence>(Asset);
        if (SelectedAnimSequence) break;
    }

    UEditorUtilityWidgetBlueprint* WidgetBlueprint = BakeToRootWidgetAsset.LoadSynchronous();
    if (!WidgetBlueprint) return;

    UEditorUtilitySubsystem* Subsystem = GEditor ? GEditor->GetEditorSubsystem<UEditorUtilitySubsystem>() : nullptr;
    if (!Subsystem) return;

    UEditorUtilityWidget* SpawnedWidget = Subsystem->SpawnAndRegisterTab(WidgetBlueprint);
    if (URyanBakeToRootWidget* BakeToRootWidget = Cast<URyanBakeToRootWidget>(SpawnedWidget))
    {
        if (SelectedAnimSequence)
        {
            BakeToRootWidget->SetSkeleton(SelectedAnimSequence->GetSkeleton());
        }
    }
}
