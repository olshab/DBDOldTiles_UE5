// Copyright Epic Games, Inc. All Rights Reserved.

#include "BlueprintUncookerCommands.h"

#define LOCTEXT_NAMESPACE "FBlueprintUncookerModule"

void FBlueprintUncookerCommands::RegisterCommands()
{
	UI_COMMAND(OpenBlueprintUncookerWindow, "BlueprintUncooker", "Bring up BlueprintUncooker window", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE
