// Copyright Epic Games, Inc. All Rights Reserved.

#include "BlueprintUncookerStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Framework/Application/SlateApplication.h"
#include "Slate/SlateGameResources.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyleMacros.h"

#define RootToContentDir Style->RootToContentDir

TSharedPtr<FSlateStyleSet> FBlueprintUncookerStyle::StyleInstance = nullptr;

void FBlueprintUncookerStyle::Initialize()
{
	if (!StyleInstance.IsValid())
	{
		StyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

void FBlueprintUncookerStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
	ensure(StyleInstance.IsUnique());
	StyleInstance.Reset();
}

FName FBlueprintUncookerStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("BlueprintUncookerStyle"));
	return StyleSetName;
}

const FVector2D Icon16x16(16.0f, 16.0f);
const FVector2D Icon20x20(20.0f, 20.0f);
const FVector2D Icon190x190(190.0f, 190.0f);

TSharedRef< FSlateStyleSet > FBlueprintUncookerStyle::Create()
{
	TSharedRef<FSlateStyleSet> Style = MakeShareable(new FSlateStyleSet("BlueprintUncookerStyle"));
	Style->SetContentRoot(IPluginManager::Get().FindPlugin("BlueprintUncooker")->GetBaseDir() / TEXT("Resources"));

	Style->Set("BlueprintUncooker.OpenBlueprintUncookerWindow", new IMAGE_BRUSH(TEXT("PluginIcon"), Icon190x190));

	return Style;
}

void FBlueprintUncookerStyle::ReloadTextures()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
	}
}

const ISlateStyle& FBlueprintUncookerStyle::Get()
{
	return *StyleInstance;
}
