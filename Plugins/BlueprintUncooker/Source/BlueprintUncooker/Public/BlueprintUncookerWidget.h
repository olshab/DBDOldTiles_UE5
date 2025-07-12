#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

#define LOCTEXT_NAMESPACE "BlueprintUncookerWidget"

class UUncookerSettings;

class SBlueprintUncookerWidget : public SCompoundWidget
{
public:
	SBlueprintUncookerWidget();

	UUncookerSettings* Settings;

	SLATE_BEGIN_ARGS(SBlueprintUncookerWidget)
	{
	}

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	FReply OnDetailsViewButtonClicked();
	FReply OnSelectDirectoryButtonClicked();
};

#undef LOCTEXT_NAMESPACE
