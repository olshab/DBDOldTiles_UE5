#include "MergeTool.h"
#include "AssetRegistryModule.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "HAL/FileManagerGeneric.h"
#include "Misc/FileHelper.h"
#include "EditorAssetLibrary.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Engine/Blueprint.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/InheritableComponentHandler.h"
#include "Engine/SCS_Node.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/ChildActorComponent.h"
#include "MergeSettings.h"

bool FMergeTool::Execute()
{
	const FString FirstBlueprintPackageName = Settings->Destination.GetLongPackageName();
	const FString SecondBlueprintPackageName = Settings->Source.GetLongPackageName();

	MergeBlueprints(FirstBlueprintPackageName, SecondBlueprintPackageName);

	return true;
}

void FMergeTool::MergeBlueprints(const FString& FirstBlueprintPackageName, const FString& SecondBlueprintPackageName)
{
	if (FirstBlueprintPackageName.IsEmpty())
		return;

	const FString FirstBlueprintName = GetAssetName(FirstBlueprintPackageName);
	UPackage* FirstBlueprintPackage;
	UBlueprint* FirstBlueprint;

	FirstBlueprintPackage = GetPackage(FirstBlueprintPackageName);
	if (FirstBlueprintPackage == NULL)
	{
		UE_LOG(LogTemp, Error, TEXT("Package %s could not be loaded"), *FirstBlueprintPackageName);
		return;
	}

	FirstBlueprint = FindObject<UBlueprint>(FirstBlueprintPackage, *FirstBlueprintName);
	check(FirstBlueprint != NULL);

	// skip merging if there is merged blueprint already
	const FString MergedBlueprintPackageName = Settings->MergeAtPath + TEXT('/') + FirstBlueprintName;

	if (GetPackage(MergedBlueprintPackageName) != NULL)
	{
		UE_LOG(LogTemp, Error, TEXT("Package %s already exists"), *MergedBlueprintPackageName);
		return;
	}

	/** Duplicate first blueprint, and then copy components into it from second blueprint */
	UBlueprint* DuplicatedBlueprint = CastChecked<UBlueprint>(UEditorAssetLibrary::DuplicateAsset(FirstBlueprintPackageName, MergedBlueprintPackageName));
	checkf(DuplicatedBlueprint != NULL, TEXT("Failed to duplicate blueprint %s"), *FirstBlueprintPackageName);

	/** If the second blueprint is not specified, then just leave first BP duplicated and go on */
	if (SecondBlueprintPackageName.IsEmpty())
		return;

	USimpleConstructionScript* SCS = DuplicatedBlueprint->SimpleConstructionScript;
	USCS_Node* RootSceneNode = SCS->CreateNode(USceneComponent::StaticClass(), TEXT("COMPONENTS_FROM_ANOTHER_BP"));
	CastChecked<USceneComponent>(RootSceneNode->ComponentTemplate)->SetMobility(EComponentMobility::Static);

	// Find root component
	AddComponentToRoot(RootSceneNode, DuplicatedBlueprint);

	TArray<FString> ParentVariableNames;

	/** Get the second blueprint - the one we're going to copy from */
	const FString SecondBlueprintName = GetAssetName(SecondBlueprintPackageName);
	UPackage* SecondBlueprintPackage;
	UBlueprint* SecondBlueprint;

	SecondBlueprintPackage = GetPackage(SecondBlueprintPackageName);
	if (SecondBlueprintPackage == NULL)
	{
		UE_LOG(LogTemp, Error, TEXT("Package %s could not be loaded"), *SecondBlueprintPackageName);
		return;
	}

	SecondBlueprint = FindObject<UBlueprint>(SecondBlueprintPackage, *SecondBlueprintName);
	check(SecondBlueprint != NULL);
	/*
	if (SecondBlueprint == NULL)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to find object %s"), *SecondBlueprintName);
		return;
	}
	*/

	/** Change ChildActorClass for all ActorSpawners */
	if (Settings->bReplaceActorSpawners)
		ReplaceActorSpawners(DuplicatedBlueprint->SimpleConstructionScript);  // might crash the editor

	/** Hide all components from duplicated blueprint before copying from another BP */
	HideAllComponents(DuplicatedBlueprint->SimpleConstructionScript);

	if (Settings->bMakeMeshesTranslucent)
		ChangeMaterialsForMeshes(DuplicatedBlueprint->SimpleConstructionScript);

	for (USCS_Node* NodeToCopyFrom : SecondBlueprint->SimpleConstructionScript->GetRootNodes())
	{
		USCS_Node* ParentNode = RootSceneNode;

		if (!NodeToCopyFrom->ParentComponentOrVariableName.IsNone())
		{
			bool bHasBeenCreated = false;
			ParentNode = FindOrCreateParentNode(
				RootSceneNode,
				NodeToCopyFrom->ParentComponentOrVariableName.ToString(),
				bHasBeenCreated);
			check(ParentNode != NULL);

			/** Copy translation, rotation and scale */
			if (bHasBeenCreated)
			{
				UActorComponent* OriginalParentComponent = GetComponentBySCSName(NodeToCopyFrom->GetSCS(), NodeToCopyFrom->ParentComponentOrVariableName.ToString());
				if (OriginalParentComponent != NULL)
				{
					USceneComponent* ParentNodeScene = CastChecked<USceneComponent>(ParentNode->ComponentTemplate);
					USceneComponent* OriginalParentNodeScene = CastChecked<USceneComponent>(OriginalParentComponent);

					ParentNodeScene->SetRelativeTransform(OriginalParentNodeScene->GetRelativeTransform());
					ParentNodeScene->SetMobility(OriginalParentNodeScene->Mobility);
				}
			}
		}

		CopyComponent(NodeToCopyFrom, ParentNode);
	}

	FAssetRegistryModule::AssetCreated(DuplicatedBlueprint);
	GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(DuplicatedBlueprint);
	FKismetEditorUtilities::CompileBlueprint(DuplicatedBlueprint);
}

FString FMergeTool::GetAssetName(const FString& PackageName)
{
	FString AssetName;
	bool SplitSuccess = PackageName.Split(FString(TEXT("/")), nullptr, &AssetName, ESearchCase::CaseSensitive, ESearchDir::FromEnd);

	if (!SplitSuccess)
		UE_LOG(LogTemp, Error, TEXT("GetAssetName failed!"));

	return AssetName;
}

UPackage* FMergeTool::GetPackage(const FString& PackageName)
{
	UPackage* Package = FindPackage(nullptr, *PackageName);
	if (!Package)
	{
		Package = LoadPackage(nullptr, *PackageName, LOAD_None);
	}

	return Package;
}

void FMergeTool::AddComponentToRoot(USCS_Node* Node, UBlueprint* Blueprint)
{
	bool bRootInParentBP = false;
	USimpleConstructionScript* CurrentSCS = Blueprint->SimpleConstructionScript;

	UBlueprintGeneratedClass* ParentClass = Cast<UBlueprintGeneratedClass>(Blueprint->SimpleConstructionScript->GetParentClass());
	while (ParentClass != NULL)
	{
		CurrentSCS = ParentClass->SimpleConstructionScript;
		ParentClass = Cast<UBlueprintGeneratedClass>(CurrentSCS->GetParentClass());
		bRootInParentBP = true;
	}

	USCS_Node* RootNode = CurrentSCS->GetRootNodes()[0];
	if (bRootInParentBP)
	{
		Blueprint->SimpleConstructionScript->AddNode(Node);
		Node->SetParent(RootNode);
	}
	else
	{
		RootNode->AddChildNode(Node);
	}
}

void FMergeTool::CopyComponent(USCS_Node* NodeToCopyFrom, USCS_Node* ParentNode)
{
	USCS_Node* CopiedNode = ParentNode->GetSCS()->CreateNode(
		NodeToCopyFrom->ComponentClass,
		*(NodeToCopyFrom->GetVariableName().ToString() + Settings->SourceComponentsPostfix));
	ParentNode->AddChildNode(CopiedNode);

	UObject* DuplicatedTemplate = StaticDuplicateObject(
		NodeToCopyFrom->ComponentTemplate,
		ParentNode->GetSCS()->GetBlueprint()->GeneratedClass,
		CopiedNode->ComponentTemplate->GetFName()
	);

	UActorComponent* DuplicatedComponent = NULL;
	if (DuplicatedTemplate != NULL)
	{
		DuplicatedComponent = CastChecked<UActorComponent>(DuplicatedTemplate);
		CopiedNode->ComponentTemplate = DuplicatedComponent;
	}

	/** Check for ActorSpawner */
	if (DuplicatedComponent->ComponentHasTag(TEXT("ActorSpawner")))
	{
		if (USceneComponent* SceneComp = Cast<USceneComponent>(DuplicatedComponent))
			SceneComp->SetVisibility(false);
	}

	for (USCS_Node* ChildNodeToCopyFrom : NodeToCopyFrom->GetChildNodes())
	{
		CopyComponent(ChildNodeToCopyFrom, CopiedNode);
	}
}

USCS_Node* FMergeTool::FindOrCreateParentNode(USCS_Node* RootNode, const FString& ParentNodeName, bool& bHasBeenCreated)
{
	USCS_Node* FoundNode = NULL;

	for (USCS_Node* ChildNode : RootNode->ChildNodes)
		if (ChildNode->GetVariableName().ToString().Equals(ParentNodeName + Settings->SourceComponentsPostfix))
			FoundNode = ChildNode;

	if (FoundNode != NULL)
		return FoundNode;

	/** If parent node doesn't exist yet */
	check(RootNode != NULL);
	USCS_Node* ParentNode = RootNode->GetSCS()->CreateNode(USceneComponent::StaticClass(), *(ParentNodeName + Settings->SourceComponentsPostfix));
	RootNode->AddChildNode(ParentNode);
	bHasBeenCreated = true;

	return ParentNode;
}

UActorComponent* FMergeTool::GetComponentBySCSName(USimpleConstructionScript* SCS, const FString& SCSVariableName)
{
	USCS_Node* FoundNode;

	FoundNode = SCS->FindSCSNode(*SCSVariableName);
	if (FoundNode != NULL)
		return FoundNode->ComponentTemplate;

	/** If that node in parent class */
	UBlueprintGeneratedClass* ParentClass = nullptr;
	USimpleConstructionScript* CurrentSCS = SCS;
	do
	{
		ParentClass = Cast<UBlueprintGeneratedClass>(CurrentSCS->GetParentClass());
		if (!ParentClass)
		{
			UE_LOG(LogTemp, Error, TEXT("Can't find inherited node %s (cast to parent BP class failed)"), *SCSVariableName);
			return NULL;
		}
		USimpleConstructionScript* ParentSCS = ParentClass->SimpleConstructionScript;
		CurrentSCS = ParentSCS;

		FoundNode = CurrentSCS->FindSCSNode(*SCSVariableName);
		if (FoundNode != NULL)
		{
			UInheritableComponentHandler* ComponentHandler = SCS->GetBlueprint()->InheritableComponentHandler;
			FComponentKey ComponentKey(FoundNode);

			UActorComponent* ComponentTemplate = ComponentHandler->GetOverridenComponentTemplate(ComponentKey);
			if (!ComponentTemplate)
				ComponentTemplate = ComponentHandler->CreateOverridenComponentTemplate(ComponentKey);

			return ComponentTemplate;
		}

	} while (ParentClass);

	return NULL;
}

void FMergeTool::HideAllComponents(USimpleConstructionScript* SCS)
{
	/** For components that are not inherited */
	for (USCS_Node* Node : SCS->GetAllNodes())
		if (USceneComponent* SceneComp = Cast<USceneComponent>(Node->ComponentTemplate))
			SceneComp->SetHiddenInGame(true);

	UInheritableComponentHandler* ComponentHandler = SCS->GetBlueprint()->GetInheritableComponentHandler(true);

	USimpleConstructionScript* CurrentSCS = NULL;
	UBlueprintGeneratedClass* ParentClass = Cast<UBlueprintGeneratedClass>(SCS->GetParentClass());

	while (ParentClass != NULL)
	{
		CurrentSCS = ParentClass->SimpleConstructionScript;

		for (USCS_Node* Node : CurrentSCS->GetAllNodes())
		{
			FComponentKey ComponentKey(Node);
			UActorComponent* ComponentTemplate = ComponentHandler->GetOverridenComponentTemplate(ComponentKey);
			if (ComponentTemplate == NULL)
				ComponentTemplate = ComponentHandler->CreateOverridenComponentTemplate(ComponentKey);

			if (USceneComponent* SceneComp = Cast<USceneComponent>(ComponentTemplate))
				SceneComp->SetHiddenInGame(true);
		}
		ParentClass = Cast<UBlueprintGeneratedClass>(CurrentSCS->GetParentClass());
	}
}

void FMergeTool::ChangeMaterialsForMeshes(USimpleConstructionScript* SCS)
{
	UMaterialInterface* Material = Cast<UMaterialInterface>(Settings->TranslucentMaterial.TryLoad());
	
	if (Material == NULL)
		return;

	/** For components that are not inherited */
	for (USCS_Node* Node : SCS->GetAllNodes())
	{
		if (UStaticMeshComponent* MeshComp = Cast<UStaticMeshComponent>(Node->ComponentTemplate))
		{
			int32 MaterialsNum = MeshComp->GetMaterials().Num();

			for (int32 i = 0; i < MaterialsNum; i++)
				MeshComp->SetMaterial(i, Material);
		}
	}
}

void FMergeTool::ReplaceActorSpawners(USimpleConstructionScript* SCS)
{
	/** Replace ActorSpawners for SCS_Nodes in current blueprint */
	for (USCS_Node* Node : SCS->GetAllNodes())
	{
		if (Node->ComponentTemplate->ComponentHasTag(TEXT("ActorSpawner")))
			if (UChildActorComponent* ChildActor = Cast<UChildActorComponent>(Node->ComponentTemplate))
			{
				TSubclassOf<AActor> ChildActorClass = ChildActor->GetChildActorClass();
				if (ChildActorClass == NULL)
				{
					UE_LOG(LogTemp, Warning, TEXT("ActorSpawner component %s doesn't have ChildActorClass"), *Node->GetVariableName().ToString());
					continue;
				}

				FString ActorClassName = ChildActorClass.Get()->GetName();
				if (Cast<UBlueprintGeneratedClass>(ChildActorClass.Get()))
					check(ActorClassName.RemoveFromEnd(TEXT("_C")));

				// const FString ActorSpawnerPackageName = Settings->ActorSpawnersPackagePath + TEXT('/') + AssetName;
				const FString ActorSpawnerPackageName = FindMergedActorSpawner(ActorClassName);
				if (ActorSpawnerPackageName.IsEmpty())
					continue;

				UPackage* Package = GetPackage(*ActorSpawnerPackageName);
				if (Package == NULL)
				{
					UE_LOG(LogTemp, Error, TEXT("Failed to find %s"), *ActorSpawnerPackageName);
					continue;
				}

				UClass* BlueprintGeneratedClass = FindObjectFast<UClass>(Package, *(GetAssetName(ActorSpawnerPackageName) + TEXT("_C")));
				if (BlueprintGeneratedClass == NULL)
				{
					UE_LOG(LogTemp, Error, TEXT("Failed to find UBlueprintGeneratedClass %s"), *(GetAssetName(ActorSpawnerPackageName) + TEXT("_C")));
					continue;
				}

				ChildActor->SetChildActorClass(TSubclassOf<AActor>(BlueprintGeneratedClass));
			}
	}

	/** Replace ActorSpawners for SCS_Nodes inherited from parent blueprint */
	UInheritableComponentHandler* ComponentHandler = SCS->GetBlueprint()->GetInheritableComponentHandler(true);

	USimpleConstructionScript* CurrentSCS = NULL;
	UBlueprintGeneratedClass* ParentClass = Cast<UBlueprintGeneratedClass>(SCS->GetParentClass());

	while (ParentClass != NULL)
	{
		CurrentSCS = ParentClass->SimpleConstructionScript;

		for (USCS_Node* Node : CurrentSCS->GetAllNodes())
		{
			if (!Node->ComponentTemplate->ComponentHasTag(TEXT("ActorSpawner")))
				continue;

			FComponentKey ComponentKey(Node);
			UActorComponent* ComponentTemplate = ComponentHandler->GetOverridenComponentTemplate(ComponentKey);
			if (ComponentTemplate == NULL)
				ComponentTemplate = ComponentHandler->CreateOverridenComponentTemplate(ComponentKey);


			if (UChildActorComponent* ChildActor = Cast<UChildActorComponent>(ComponentTemplate))
			{
				TSubclassOf<AActor> ChildActorClass = ChildActor->GetChildActorClass();
				if (ChildActorClass == NULL)
				{
					UE_LOG(LogTemp, Warning, TEXT("ActorSpawner component %s doesn't have ChildActorClass"), *Node->GetVariableName().ToString());
					continue;
				}

				FString ActorClassName = ChildActorClass.Get()->GetName();
				if (Cast<UBlueprintGeneratedClass>(ChildActorClass.Get()))
					check(ActorClassName.RemoveFromEnd(TEXT("_C")));

				// const FString ActorSpawnerPackageName = Settings->ActorSpawnersPackagePath + TEXT('/') + AssetName;
				const FString ActorSpawnerPackageName = FindMergedActorSpawner(ActorClassName);
				if (ActorSpawnerPackageName.IsEmpty())
					continue;

				UPackage* Package = GetPackage(*ActorSpawnerPackageName);
				if (Package == NULL)
				{
					UE_LOG(LogTemp, Error, TEXT("Failed to find %s"), *ActorSpawnerPackageName);
					continue;
				}

				UClass* BlueprintGeneratedClass = FindObjectFast<UClass>(Package, *(GetAssetName(ActorSpawnerPackageName) + TEXT("_C")));
				if (BlueprintGeneratedClass == NULL)
				{
					UE_LOG(LogTemp, Error, TEXT("Failed to find UBlueprintGeneratedClass %s"), *(GetAssetName(ActorSpawnerPackageName) + TEXT("_C")));
					continue;
				}

				ChildActor->SetChildActorClass(TSubclassOf<AActor>(BlueprintGeneratedClass));
			}

		}
		ParentClass = Cast<UBlueprintGeneratedClass>(CurrentSCS->GetParentClass());
	}
}

FString FMergeTool::FindMergedActorSpawner(const FString& BlueprintAssetName)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	TArray<FAssetData> Assets;

	FARFilter Filter;
	Filter.PackagePaths.Add(*Settings->ActorSpawnersPackagePath);
	Filter.bRecursivePaths = true;
	AssetRegistryModule.Get().GetAssets(Filter, Assets);

	for (const FAssetData& AssetData : Assets)
		if (BlueprintAssetName.Equals(AssetData.AssetName.ToString()))
			return AssetData.PackageName.ToString();

	return FString();
}