#pragma once

#include "CoreMinimal.h"

class UBlueprint;
class USCS_Node;
class UMergeSettings;

class FMergeTool
{
public:
	FMergeTool(UMergeSettings* Settings) :
		Settings(Settings)
	{ }

	/** Main Function */
	bool Execute();

private:
	void MergeBlueprints(const FString& FirstBlueprintPackageName, const FString& SecondBlueprintPackageName);

	FString GetAssetName(const FString& PackageName);

	UPackage* GetPackage(const FString& PackageName);

	void AddComponentToRoot(USCS_Node* Node, UBlueprint* Blueprint);

	void CopyComponent(USCS_Node* NodeToCopyFrom, USCS_Node* ParentNode);

	USCS_Node* FindOrCreateParentNode(USCS_Node* RootNode, const FString& ParentNodeName, bool& bHasBeenCreated);

	UActorComponent* GetComponentBySCSName(USimpleConstructionScript* SCS, const FString& SCSVariableName);

	void HideAllComponents(USimpleConstructionScript* SCS);

	void ChangeMaterialsForMeshes(USimpleConstructionScript* SCS);

	void ReplaceActorSpawners(USimpleConstructionScript* SCS);

	/** Returns PackageName of found blueprint, or empty string if not found */
	FString FindMergedActorSpawner(const FString& BlueprintAssetName);

private:
	UMergeSettings* Settings;
};
