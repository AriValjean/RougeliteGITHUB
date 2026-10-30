#pragma once

#include "CoreMinimal.h"
#include "Fragments/InventoryItemFragment.h"
#include "InventoryFragment_Stackable.generated.h"

/**
 * Lets an item stack in one inventory slot. Replaces BP_ItemFragment_Stackable.
 * Items without this fragment stack to 1 (e.g. weapons).
 */
UCLASS(meta=(DisplayName="Stackable"))
class PATHFINDEROTHEREARTH_API UInventoryFragment_Stackable : public UInventoryItemFragment
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stacking", meta=(ClampMin="1"))
	int32 MaxStackCount = 1;
};
