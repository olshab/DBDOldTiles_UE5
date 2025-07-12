#include "BlueprintUncookerWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "Widgets/Input/SButton.h"
#include "EditorStyleSet.h"
#include "Input/Reply.h"
#include "Developer/DesktopPlatform/Public/IDesktopPlatform.h"
#include "Developer/DesktopPlatform/Public/DesktopPlatformModule.h"
#include "Editor/MainFrame/Public/Interfaces/IMainFrameModule.h"

#include "UncookerSettings.h"
#include "BlueprintAssetGenerator.h"

SBlueprintUncookerWidget::SBlueprintUncookerWidget()
{
	Settings = UUncookerSettings::Get();
}

void SBlueprintUncookerWidget::Construct(const FArguments& InArgs)
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;

	TSharedRef<IDetailsView> DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	DetailsView->SetObject(Settings);

	ChildSlot
		[
			SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					DetailsView
				]

			+ SVerticalBox::Slot()
				.AutoHeight()
				.VAlign(VAlign_Top)
				.Padding(30.0f, 20.0f, 0.0f, 0.0f)
				[
					SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.HAlign(HAlign_Left)
						[
							SNew(SButton)
								.ButtonStyle(FEditorStyle::Get(), "FlatButton.Success")
								.ContentPadding(FMargin(30, 5))
								.OnClicked(this, &SBlueprintUncookerWidget::OnDetailsViewButtonClicked)
								[
									SNew(STextBlock)
										.TextStyle(FEditorStyle::Get(), "ContentBrowser.TopBar.Font")
										.Text(FText::FromString(TEXT("Generate")))
								]
						]

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.HAlign(HAlign_Left)
						.Padding(20.0f, 0.0f)
						[
							SNew(SButton)
								.ButtonStyle(FEditorStyle::Get(), "FlatButton.Primary")
								.ContentPadding(FMargin(10, 5))
								.OnClicked(this, &SBlueprintUncookerWidget::OnSelectDirectoryButtonClicked)
								[
									SNew(STextBlock)
										.TextStyle(FEditorStyle::Get(), "ContentBrowser.TopBar.Font")
										.Text(FText::FromString(TEXT("Choose Dumps Folder")))
								]
						]
				]
		];
}

FReply SBlueprintUncookerWidget::OnDetailsViewButtonClicked()
{
	Settings->SaveConfig(CPF_Config, *Settings->GetDefaultConfigFilename());

	FBlueprintAssetGenerator AssetGenerator(Settings);
	bool Success = AssetGenerator.Generate();

	if (Success)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("Successfully uncooked blueprints")));
	}
	else
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("Failed to uncook blueprints. See the logs")));
	}

	return FReply::Handled();
}

FReply SBlueprintUncookerWidget::OnSelectDirectoryButtonClicked()
{
	IMainFrameModule& MainFrame = IMainFrameModule::Get();
	TSharedPtr<SWindow> MainWindow = MainFrame.GetParentWindow();
	void* ParentWindowHandle = MainWindow->GetNativeWindow()->GetOSWindowHandle();

	FString SelectedDirectory;

	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	DesktopPlatform->OpenDirectoryDialog(ParentWindowHandle, TEXT("Select Dumps Directory"), TEXT(""), SelectedDirectory);

	Settings->DumpsDirectory = SelectedDirectory;
	Settings->SaveConfig(CPF_Config, *Settings->GetDefaultConfigFilename());

	return FReply::Handled();
}
