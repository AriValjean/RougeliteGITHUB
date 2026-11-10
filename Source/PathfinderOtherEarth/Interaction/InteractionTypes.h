// InteractionTypes.h
// Shared data types for the interaction system.

#pragma once

#include "CoreMinimal.h"
#include "InteractionTypes.generated.h"

/**
 * Everything the interaction component and the prompt widget need to know
 * about one interactable. Each interactable fills this in from
 * IInteractableInterface::GetInteractionOption.
 */
USTRUCT(BlueprintType)
struct PATHFINDEROTHEREARTH_API FInteractionOption
{
	GENERATED_BODY()

	/** Verb shown in the prompt, e.g. "Open (25 gold)", "Talk", "Harvest". Empty = widget default ("to interact"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	FText Prompt;

	/** 0 = instant press. Above 0 = player must hold the interact key this many seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction", meta = (ClampMin = "0.0", Units = "s"))
	float HoldDuration = 0.f;

	/** False hides the prompt and blocks interaction (opened chest, inactive portal...). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	bool bIsAvailable = true;

	bool IsSameAs(const FInteractionOption& Other) const
	{
		return Prompt.EqualTo(Other.Prompt)
			&& FMath::IsNearlyEqual(HoldDuration, Other.HoldDuration)
			&& bIsAvailable == Other.bIsAvailable;
	}
};
