#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

#define LOCTEXT_NAMESPACE "BlueprintUncookerWidget"

class UMergeSettings;

class SBlueprintMergeToolWidget : public SCompoundWidget
{
public:
	SBlueprintMergeToolWidget();

	UMergeSettings* Settings;

	SLATE_BEGIN_ARGS(SBlueprintMergeToolWidget)
		{}

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	FReply OnDetailsViewButtonClicked();
};

#undef LOCTEXT_NAMESPACE
