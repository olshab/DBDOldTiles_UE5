#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "DBDGameState.generated.h"

UCLASS(Blueprintable)
class DEADBYDAYLIGHT_API ADBDGameState : public AGameState
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure)
	bool IsLevelReadyToPlay() const;
};
