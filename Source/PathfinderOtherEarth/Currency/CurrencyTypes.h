#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "CurrencyTypes.generated.h"

/** One possible crafting-material drop. Goes straight into the player's inventory (with a log line). */
USTRUCT(BlueprintType)
struct PATHFINDEROTHEREARTH_API FMaterialDrop
{
	GENERATED_BODY()

	/** Row name in DT_CraftingMaterialsCPP, e.g. Mat_Mushrooms. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drop")
	FName MaterialId;

	/** 0-1. Bosses' exclusive material = 1 (guaranteed). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drop", meta=(ClampMin="0.0", ClampMax="1.0"))
	float Chance = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drop", meta=(ClampMin="1"))
	int32 MinCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drop", meta=(ClampMin="1"))
	int32 MaxCount = 1;
};

/**
 * Row struct for DT_EnemyRewards. One row per enemy type.
 * Gold values are stage 1 numbers; GetStageScale grows them every stage.
 */
USTRUCT(BlueprintType)
struct PATHFINDEROTHEREARTH_API FEnemyRewardRow : public FTableRowBase
{
	GENERATED_BODY()

	/** 0 = never drops gold, 1 = always drops gold. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gold", meta=(ClampMin="0.0", ClampMax="1.0"))
	float GoldDropChance = 1.0f;

	/** Smallest stage 1 drop. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gold", meta=(ClampMin="0"))
	int32 MinGold = 5;

	/** Largest stage 1 drop. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gold", meta=(ClampMin="0"))
	int32 MaxGold = 10;

	/** Crafting materials. Standard enemies: usually empty. Elites: a chance. Bosses: their exclusive material at 1.0. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Materials")
	TArray<FMaterialDrop> MaterialDrops;
};
