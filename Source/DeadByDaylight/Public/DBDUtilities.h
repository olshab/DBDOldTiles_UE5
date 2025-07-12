#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DBDUtilities.generated.h"

class AProceduralLevelBuilder;

UCLASS(Blueprintable)
class DEADBYDAYLIGHT_API UDBDUtilities : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintPure)
	static AProceduralLevelBuilder* GetBuilder(const UObject* worldContextObject);
};
