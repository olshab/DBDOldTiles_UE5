#include "BlueprintMergeToolWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "Widgets/Input/SButton.h"
#include "EditorStyleSet.h"
#include "Input/Reply.h"
#include "MergeSettings.h"
#include "MergeTool.h"

SBlueprintMergeToolWidget::SBlueprintMergeToolWidget()
{
	Settings = UMergeSettings::Get();
}

void SBlueprintMergeToolWidget::Construct(const FArguments& InArgs)
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
				.OnClicked(this, &SBlueprintMergeToolWidget::OnDetailsViewButtonClicked)
				[
					SNew(STextBlock)
					.TextStyle(FEditorStyle::Get(), "ContentBrowser.TopBar.Font")
					.Text(FText::FromString(TEXT("Generate")))
				]
			]
		]
	];
}

FReply SBlueprintMergeToolWidget::OnDetailsViewButtonClicked()
{
	Settings->SaveConfig(CPF_Config, *Settings->GetDefaultConfigFilename());

	FMergeTool MergeTool(Settings);
	bool Success = MergeTool.Execute();

	if (Success)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("Successfully generated skeleton")));
	}
	else
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("Failed to generate skeleton. See the logs")));
	}

	return FReply::Handled();
}
