#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "MergeSettings.generated.h"

UCLASS(Config = Editor, DefaultConfig, meta = (DisplayName = "Blueprint Merge Settings"))
class UMergeSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UMergeSettings();

	FORCEINLINE static UMergeSettings* Get()
	{
		return GetMutableDefault<UMergeSettings>();
	}

public:
	UPROPERTY(EditAnywhere, Config)
	FString MergeAtPath;

	UPROPERTY(EditAnywhere, Config, meta = (AllowedClasses = "Blueprint"))
	FSoftObjectPath Destination;

	UPROPERTY(EditAnywhere, Config, meta = (AllowedClasses = "Blueprint"))
	FSoftObjectPath Source;

	UPROPERTY(EditAnywhere, Config, Category = MaterialOverride)
	bool bMakeMeshesTranslucent;

	UPROPERTY(EditAnywhere, Config, Category = MaterialOverride, meta = (AllowedClasses = "Material", EditCondition = "bMakeMeshesTranslucent"))
	FSoftObjectPath TranslucentMaterial;

	UPROPERTY(EditAnywhere, Config, Category = ActorSpawners)
	bool bReplaceActorSpawners;

	UPROPERTY(EditAnywhere, Config, Category = ActorSpawners, meta = (DisplayName = "Folder To Take ActorSpawners From", EditCondition = "bReplaceActorSpawners"))
	FString ActorSpawnersPackagePath;

	UPROPERTY(EditAnywhere, Config, AdvancedDisplay)
	FString SourceComponentsPostfix;
};
