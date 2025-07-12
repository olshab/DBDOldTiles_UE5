// Copyright Epic Games, Inc. All Rights Reserved.

#include "BlueprintMergeTool.h"
#include "BlueprintMergeToolStyle.h"
#include "BlueprintMergeToolCommands.h"
#include "LevelEditor.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "ToolMenus.h"
#include "BlueprintMergeToolWidget.h"

static const FName BlueprintMergeToolTabName("BlueprintMergeTool");

#define LOCTEXT_NAMESPACE "FBlueprintMergeToolModule"

void FBlueprintMergeToolModule::StartupModule()
{
	FBlueprintMergeToolStyle::Initialize();
	FBlueprintMergeToolStyle::ReloadTextures();

	FBlueprintMergeToolCommands::Register();
	
	PluginCommands = MakeShareable(new FUICommandList);

	PluginCommands->MapAction(
		FBlueprintMergeToolCommands::Get().OpenBlueprintMergeWindow,
		FExecuteAction::CreateRaw(this, &FBlueprintMergeToolModule::PluginButtonClicked),
		FCanExecuteAction());

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FBlueprintMergeToolModule::RegisterMenus));
	
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(BlueprintMergeToolTabName, FOnSpawnTab::CreateRaw(this, &FBlueprintMergeToolModule::OnSpawnPluginTab))
		.SetDisplayName(LOCTEXT("FBlueprintMergeToolTabTitle", "BlueprintMergeTool"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);
}

void FBlueprintMergeToolModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);

	FBlueprintMergeToolStyle::Shutdown();
	FBlueprintMergeToolCommands::Unregister();
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(BlueprintMergeToolTabName);
}

TSharedRef<SDockTab> FBlueprintMergeToolModule::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{
	return
		SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SBlueprintMergeToolWidget)
		];
}

void FBlueprintMergeToolModule::PluginButtonClicked()
{
	FGlobalTabmanager::Get()->TryInvokeTab(BlueprintMergeToolTabName);
}

void FBlueprintMergeToolModule::RegisterMenus()
{
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
	FToolMenuOwnerScoped OwnerScoped(this);

	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
		{
			FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
			Section.AddMenuEntryWithCommandList(
				FBlueprintMergeToolCommands::Get().OpenBlueprintMergeWindow,
				PluginCommands,
				FText::FromString(TEXT("Blueprint Merge Tool")),
				FText::FromString(TEXT("Click to open Blueprint Merge Tool window"))
			);
		}
	}

	{
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.User");
		{
			FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("Plugins");
			{
				FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(
					FBlueprintMergeToolCommands::Get().OpenBlueprintMergeWindow,
					FText::FromString(TEXT("Blueprint Merge Tool")),
					FText::FromString(TEXT("Click to open Blueprint Merge Tool window"))
				));
				Entry.SetCommandList(PluginCommands);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FBlueprintMergeToolModule, BlueprintMergeTool)