#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Items/ItemDefinition.h"
#include "InventoryTypes.generated.h"

/** Where a crafting material comes from. Replaces E_ItemSource. */
UENUM(BlueprintType)
enum class EItemSource : uint8
{
	WorldPickup UMETA(DisplayName="World Pickup"),
	BossDrop    UMETA(DisplayName="Boss Drop"),
	EliteDrop   UMETA(DisplayName="Elite Drop"),
	QuestDrop   UMETA(DisplayName="Quest Drop")
};

/** One inventory slot. Replaces S_InvetoryItems (same two fields). */
USTRUCT(BlueprintType)
struct PATHFINDEROTHEREARTH_API FInventoryEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory")
	TSubclassOf<UItemDefinition> ItemDefinition;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory", meta=(ClampMin="0"))
	int32 Count = 0;

	FInventoryEntry() {}
	FInventoryEntry(TSubclassOf<UItemDefinition> InDefinition, int32 InCount)
		: ItemDefinition(InDefinition), Count(InCount) {}
};

/**
 * Row struct for DT_CraftingMaterialsCPP. Row Name = Material ID (e.g. Mat_Mushrooms).
 * Rarity is intentionally NOT here: it lives on the item definition only.
 */
USTRUCT(BlueprintType)
struct PATHFINDEROTHEREARTH_API FCraftingMaterialRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Material")
	TSoftClassPtr<UItemDefinition> Definition;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Material")
	EItemSource Source = EItemSource::WorldPickup;
};
