// PathfinderLootStatics.h
// Loot helpers: WeightedPick, RollRarity, RollAccessory.
// Replaces BFL_WeightedPick entirely once callers are re-pointed.
//
// Location: Source/PathfinderOtherEarth/Loot/PathfinderLootStatics.h

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PathfinderLootTypes.h"
#include "Items/ItemDefinition.h"
#include "PathfinderLootStatics.generated.h"

class UDataTable;

UCLASS()
class PATHFINDEROTHEREARTH_API UPathfinderLootStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Sums Weights, rolls a random float in [0, Total), and returns the index of the first
	 * entry whose cumulative weight exceeds the roll. Returns -1 if Weights is empty or
	 * every entry is <= 0.
	 *
	 * Not BlueprintPure: random side effect, so it runs once per execution.
	 */
	UFUNCTION(BlueprintCallable, Category = "Loot")
	static int32 WeightedPick(const TArray<float>& Weights);

	/**
	 * Builds the row name from ChestType, reads FRarityWeights from RarityWeightTable,
	 * and rolls a rarity via WeightedPick. Returns EItemRarity::Common and logs a warning
	 * if the row isn't found.
	 *
	 * Stage is accepted but not yet consumed. When stage scaling comes back, build the row
	 * name as "<ChestType>_Stage<N>" and fall back to the plain row name if missing.
	 */
	UFUNCTION(BlueprintCallable, Category = "Loot")
	static EItemRarity RollRarity(EChestType ChestType, int32 Stage, const UDataTable* RarityWeightTable);

	/**
	 * Picks an accessory of the given rarity from AccessoryTable (FAccessoryData rows),
	 * weighted by DropWeight. Rows with DropWeight <= 0 are never picked.
	 * If that rarity tier is empty, steps down one tier at a time toward Common.
	 * Returns the row name (accessory ID), or None if nothing could be picked.
	 */
	UFUNCTION(BlueprintCallable, Category = "Loot")
	static FName RollAccessory(EItemRarity Rarity, const UDataTable* AccessoryTable);
};
