#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MapData.generated.h"

UCLASS(Blueprintable)
class DEADBYDAYLIGHT_API UMapData : public UDataAsset
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName ThemeName;

public:
	UMapData();
};
