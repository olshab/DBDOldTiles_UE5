#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralLevelBuilder.generated.h"

class UMapData;

UCLASS(Blueprintable)
class DEADBYDAYLIGHT_API AProceduralLevelBuilder : public AActor
{
	GENERATED_BODY()
	
private:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(AllowPrivateAccess=true))
	UMapData* _mapData;
};
