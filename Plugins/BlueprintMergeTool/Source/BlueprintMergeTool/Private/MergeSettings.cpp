#include "MergeSettings.h"

UMergeSettings::UMergeSettings()
	: Super()
{
	MergeAtPath = TEXT("/Game/MergedTiles");
	bMakeMeshesTranslucent = true;
	TranslucentMaterial = TEXT("/Game/Materials/Transparent.Transparent");
	bReplaceActorSpawners = true;
	ActorSpawnersPackagePath = TEXT("/Game/MergedTiles/ActorSpawners");
	SourceComponentsPostfix = TEXT("_MERGED");
}
