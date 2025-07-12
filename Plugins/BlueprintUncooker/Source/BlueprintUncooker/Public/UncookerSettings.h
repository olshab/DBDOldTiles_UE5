#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UncookerSettings.generated.h"

UCLASS(Config=Editor, DefaultConfig, meta=(DisplayName="Blueprint Generation Settings"))
class UUncookerSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UUncookerSettings();

	FORCEINLINE static UUncookerSettings* Get()
	{
		return GetMutableDefault<UUncookerSettings>();
	}

public:
	UPROPERTY(EditAnywhere, Config, meta=(DisplayName="Path To Dumped JSON Files"))
	FString DumpsDirectory;

	UPROPERTY(EditAnywhere, Config)
	FString GenerateAtPath;

	UPROPERTY(EditAnywhere, Config, meta=(AllowedClasses="MaterialInstance"))
	FSoftObjectPath ParentForCreatedMaterials;
};
