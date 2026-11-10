#include "BozorLevelTools.h"

#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "NavAreas/NavArea_Null.h"
#include "NavigationSystem.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Engine/Brush.h"
#include "GameFramework/Volume.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#if WITH_EDITOR
#include "ActorFactories/ActorFactory.h"
#include "Builders/CubeBuilder.h"
#include "ObjectTools.h"
#endif

ABozorMeshGroup::ABozorMeshGroup()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent->SetMobility(EComponentMobility::Static);
}

void ABozorMeshGroup::AddInstances(UStaticMesh* Mesh, const TArray<UMaterialInterface*>& Materials, const TArray<FTransform>& Transforms, bool bCastShadow, bool bCollision)
{
	if (!Mesh || Transforms.Num() == 0) return;
	Modify();
	const FName Name = MakeUniqueObjectName(this, UInstancedStaticMeshComponent::StaticClass(), Mesh->GetFName());
	UInstancedStaticMeshComponent* C = NewObject<UInstancedStaticMeshComponent>(this, Name, RF_Transactional);
	C->SetMobility(EComponentMobility::Static);
	C->SetupAttachment(GetRootComponent());
	C->SetStaticMesh(Mesh);
	for (int32 i = 0; i < Materials.Num(); ++i)
	{
		if (Materials[i]) C->SetMaterial(i, Materials[i]);
	}
	C->SetCastShadow(bCastShadow);
	if (bCollision)
	{
		C->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	}
	else
	{
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCanEverAffectNavigation(false);
	}
	AddInstanceComponent(C);
	C->RegisterComponent();
	C->AddInstances(Transforms, false, true);
}

ABozorNavBlocker::ABozorNavBlocker()
{
	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	RootComponent = Box;
	Box->SetMobility(EComponentMobility::Static);
	Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Box->SetCollisionResponseToAllChannels(ECR_Ignore);
	Box->bDynamicObstacle = true;
	Box->SetAreaClassOverride(UNavArea_Null::StaticClass());
	Box->SetCanEverAffectNavigation(true);
	Box->ShapeColor = FColor::Red;
}

void ABozorNavBlocker::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Box->SetBoxExtent(Extent);
}

void UBozorLevelTools::SetupSkeletonRetargeting(USkeleton* Skeleton)
{
#if WITH_EDITOR
	if (!Skeleton) return;
	const FReferenceSkeleton& Ref = Skeleton->GetReferenceSkeleton();
	for (int32 i = 0; i < Ref.GetNum(); ++i)
	{
		const FName N = Ref.GetBoneName(i);
		EBoneTranslationRetargetingMode::Type Mode = EBoneTranslationRetargetingMode::Skeleton;
		if (N == TEXT("pelvis")) Mode = EBoneTranslationRetargetingMode::AnimationScaled;
		else if (Ref.GetParentIndex(i) == INDEX_NONE || N == TEXT("Root")) Mode = EBoneTranslationRetargetingMode::Animation;
		Skeleton->SetBoneTranslationRetargetingMode(i, Mode, false);
	}
	Skeleton->MarkPackageDirty();
#endif
}

void UBozorLevelTools::SetComplexCollision(UStaticMesh* Mesh)
{
#if WITH_EDITOR
	if (!Mesh) return;
	Mesh->CreateBodySetup();
	if (UBodySetup* B = Mesh->GetBodySetup())
	{
		B->Modify();
		B->CollisionTraceFlag = CTF_UseComplexAsSimple;
		B->InvalidatePhysicsData();
		B->CreatePhysicsMeshes();
	}
	Mesh->MarkPackageDirty();
#endif
}

bool UBozorLevelTools::BuildNavigation(UObject* WorldContext)
{
	UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	UNavigationSystemV1* Nav = W ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(W) : nullptr;
	if (!Nav) return false;
	// commandlet'da yangi xaritada AsyncLoadLock qoladi: qulflarni olib, sinxron quramiz
	Nav->RemoveNavigationBuildLock(0xFF, UNavigationSystemV1::ELockRemovalRebuildAction::NoRebuild);
	Nav->Build();
	for (int32 i = 0; i < 2000 && Nav->IsNavigationBuildInProgress(); ++i)
	{
		FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
		Nav->Tick(0.05f);
		FPlatformProcess::Sleep(0.01f);
	}
	return !Nav->IsNavigationBuildInProgress();
}

void UBozorLevelTools::SetAnimRootLock(UAnimSequence* Anim, bool bLock)
{
#if WITH_EDITOR
	if (!Anim) return;
	Anim->Modify();
	Anim->bForceRootLock = bLock;
	Anim->MarkPackageDirty();
#endif
}

void UBozorLevelTools::MakeBoxBrush(ABrush* Brush, FVector Size)
{
#if WITH_EDITOR
	if (!Brush) return;
	UCubeBuilder* B = NewObject<UCubeBuilder>(Brush);
	B->X = Size.X;
	B->Y = Size.Y;
	B->Z = Size.Z;
	// Hajm (AudioVolume, NavMeshBoundsVolume...): editor'dagidek o'z UModel'i bilan quriladi va csgPrepMovingBrush'dan o'tadi -
	// aks holda collision tanasi nol o'lchamli qoladi va EncompassesPoint faqat markazda true (AudioVolume ishlamaydi)
	if (AVolume* V = Cast<AVolume>(Brush))
	{
		UActorFactory::CreateBrushForVolumeActor(V, B);
		return;
	}
	Brush->BrushBuilder = B;
	B->Build(Brush->GetWorld(), Brush);
	Brush->PostEditChange();
#endif
}

int32 UBozorLevelTools::ReplaceReferences(const TArray<UObject*>& Old, const TArray<UObject*>& New)
{
	int32 N = 0;
#if WITH_EDITOR
	for (int32 i = 0; i < FMath::Min(Old.Num(), New.Num()); ++i)
	{
		if (!Old[i] || !New[i] || Old[i] == New[i]) continue;
		TArray<UObject*> ToReplace = { Old[i] };
		ObjectTools::ForceReplaceReferences(New[i], ToReplace);
		N++;
	}
#endif
	return N;
}

int32 UBozorLevelTools::CopySockets(USkeletalMesh* FromMesh, USkeleton* To)
{
	int32 N = 0;
#if WITH_EDITOR
	if (!FromMesh || !To) return 0;
	TArray<USkeletalMeshSocket*> Src;
	if (USkeleton* FS = FromMesh->GetSkeleton())
	{
		for (USkeletalMeshSocket* S : FS->Sockets) Src.Add(S);
	}
	for (USkeletalMeshSocket* S : FromMesh->GetMeshOnlySocketList()) Src.Add(S);
	To->Modify();
	for (USkeletalMeshSocket* S : Src)
	{
		if (!S || To->FindSocket(S->SocketName)) continue;
		To->Sockets.Add(DuplicateObject<USkeletalMeshSocket>(S, To));
		N++;
	}
	To->MarkPackageDirty();
#endif
	return N;
}

UActorComponent* UBozorLevelTools::AddActorComponent(AActor* Actor, TSubclassOf<UActorComponent> ComponentClass, FName Name)
{
	if (!Actor || !ComponentClass) return nullptr;
	Actor->Modify();
	UActorComponent* C = NewObject<UActorComponent>(Actor, ComponentClass, Name, RF_Transactional);
	if (USceneComponent* S = Cast<USceneComponent>(C))
	{
		S->SetupAttachment(Actor->GetRootComponent());
	}
	Actor->AddInstanceComponent(C);
	C->OnComponentCreated();
	C->RegisterComponent();
	return C;
}

void UBozorLevelTools::RerunConstruction(AActor* Actor)
{
#if WITH_EDITOR
	if (Actor) Actor->RerunConstructionScripts();
#endif
}

FString UBozorLevelTools::DescribeObject(UObject* Object)
{
	if (!Object) return FString();
	FString Out;
	for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
	{
		if (!It->GetOwnerClass() || It->GetOwnerClass()->IsNative()) continue;
		FString Value;
		It->ExportTextItem_InContainer(Value, Object, nullptr, Object, PPF_None);
		Out += FString::Printf(TEXT("P %s : %s = %s\n"), *It->GetName(), *It->GetCPPType(), *Value.Left(300));
	}
	for (TFieldIterator<UFunction> It(Object->GetClass()); It; ++It)
	{
		if (It->GetOwnerClass()->IsNative()) continue;
		FString Params;
		for (TFieldIterator<FProperty> P(*It); P && P->HasAnyPropertyFlags(CPF_Parm); ++P)
			Params += FString::Printf(TEXT("%s %s, "), *P->GetCPPType(), *P->GetName());
		Out += FString::Printf(TEXT("F %s(%s)\n"), *It->GetName(), *Params);
	}
	return Out;
}

bool UBozorLevelTools::SetPropertyText(UObject* Object, FName Property, const FString& Value)
{
	FProperty* P = Object ? FindFProperty<FProperty>(Object->GetClass(), Property) : nullptr;
	if (!P) return false;
	Object->Modify();
	const TCHAR* Result = P->ImportText_InContainer(*Value, Object, Object, PPF_None);
	return Result != nullptr;
}

void UBozorLevelTools::SetSurfaceType(UPhysicalMaterial* Material, int32 SurfaceType)
{
#if WITH_EDITOR
	if (!Material || SurfaceType < 0 || SurfaceType >= SurfaceType_Max) return;
	Material->Modify();
	Material->SurfaceType = static_cast<EPhysicalSurface>(SurfaceType);
	Material->MarkPackageDirty();
#endif
}
