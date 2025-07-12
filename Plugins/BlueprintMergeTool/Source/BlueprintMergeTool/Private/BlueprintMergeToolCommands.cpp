// Copyright Epic Games, Inc. All Rights Reserved.

#include "BlueprintMergeToolCommands.h"

#define LOCTEXT_NAMESPACE "FBlueprintMergeToolModule"

void FBlueprintMergeToolCommands::RegisterCommands()
{
	UI_COMMAND(OpenBlueprintMergeWindow, "BlueprintMergeTool", "Bring up BlueprintMergeTool window", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE
