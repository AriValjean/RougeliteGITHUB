// InteractionPromptWidget.h
// C++ parent for W_InteractionPromt. The look stays in UMG; C++ only pushes data in.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InteractionTypes.h"
#include "InteractionPromptWidget.generated.h"

UCLASS(Abstract)
class PATHFINDEROTHEREARTH_API UInteractionPromptWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Called when the focused interactable changes or its option changes. Build the prompt text here. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Interaction")
	void SetPrompt(const FInteractionOption& Option);

	/** 0..1 while holding, 0 when the hold ends or is cancelled. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Interaction")
	void SetHoldProgress(float Percent);
};
