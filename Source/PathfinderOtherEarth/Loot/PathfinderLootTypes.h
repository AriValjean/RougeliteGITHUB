// PathfinderLootTypes.h
// Drop into: Source/PathfinderOtherEarth/Loot/PathfinderLootTypes.h

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "PathfinderLootTypes.generated.h"

/** Replaces E_ChestTypes. Matches your current Basic / Enchanted naming (post Small/Large rename). */
UENUM(BlueprintType)
enum class EChestType : uint8
{
	Basic UMETA(DisplayName = "Basic"),
	Enchanted UMETA(DisplayName = "Enchanted")
};

/**
 * Replaces S_Rarity / DT_RarityWeight. Row Name = chest type ("Basic" / "Enchanted"),
 * matching your current (non-stage-scaled) DT_RarityWeight rows.
 *
 * Field named "Unique" here on purpose -- see the chat reply about the Epic/Unique
 * naming mismatch on EItemRarity before filling this table in.
 */
USTRUCT(BlueprintType)
struct FRarityWeights : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	float Common = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	float Uncommon = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	float Rare = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	float Unique = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	float Legendary = 0.f;
};
