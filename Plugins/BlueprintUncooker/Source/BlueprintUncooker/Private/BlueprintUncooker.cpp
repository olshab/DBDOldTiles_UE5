// Copyright Epic Games, Inc. All Rights Reserved.

#include "BlueprintUncooker.h"
#include "BlueprintUncookerStyle.h"
#include "BlueprintUncookerCommands.h"
#include "LevelEditor.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "ToolMenus.h"

#include "BlueprintUncookerWidget.h"

static const FName BlueprintUncookerTabName("Blueprint Uncooker");

#define LOCTEXT_NAMESPACE "FBlueprintUncookerModule"

void FBlueprintUncookerModule::StartupModule()
{
	FBlueprintUncookerStyle::Initialize();
	FBlueprintUncookerStyle::ReloadTextures();

	FBlueprintUncookerCommands::Register();
	
	PluginCommands = MakeShareable(new FUICommandList);

	PluginCommands->MapAction(
		FBlueprintUncookerCommands::Get().OpenBlueprintUncookerWindow,
		FExecuteAction::CreateRaw(this, &FBlueprintUncookerModule::PluginButtonClicked),
		FCanExecuteAction());

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FBlueprintUncookerModule::RegisterMenus));
	
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(BlueprintUncookerTabName, FOnSpawnTab::CreateRaw(this, &FBlueprintUncookerModule::OnSpawnPluginTab))
		.SetDisplayName(LOCTEXT("FBlueprintUncookerTabTitle", "BlueprintUncooker"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);
}

void FBlueprintUncookerModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	UToolMenus::UnRegisterStartupCallback(this);

	UToolMenus::UnregisterOwner(this);

	FBlueprintUncookerStyle::Shutdown();

	FBlueprintUncookerCommands::Unregister();

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(BlueprintUncookerTabName);
}

TSharedRef<SDockTab> FBlueprintUncookerModule::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{
	return
		SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SBlueprintUncookerWidget)
		];
}

void FBlueprintUncookerModule::PluginButtonClicked()
{
	FGlobalTabmanager::Get()->TryInvokeTab(BlueprintUncookerTabName);
}

void FBlueprintUncookerModule::RegisterMenus()
{
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
	FToolMenuOwnerScoped OwnerScoped(this);

	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
		{
			FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
			Section.AddMenuEntryWithCommandList(
				FBlueprintUncookerCommands::Get().OpenBlueprintUncookerWindow,
				PluginCommands,
				FText::FromString(TEXT("Blueprint Uncooker Tool")),
				FText::FromString(TEXT("Click to open Blueprint Uncooker window"))
			);
		}
	}

	{
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.User");
		{
			FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("Plugins");
			{
				FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(
					FBlueprintUncookerCommands::Get().OpenBlueprintUncookerWindow,
					FText::FromString(TEXT("BlueprintUncooker")),
					FText::FromString(TEXT("Click to open Blueprint Uncooker window"))
				));
				Entry.SetCommandList(PluginCommands);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FBlueprintUncookerModule, BlueprintUncooker)
