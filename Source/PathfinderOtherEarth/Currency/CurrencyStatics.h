#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Engine/DataTable.h"
#include "CurrencyStatics.generated.h"

/**
 * Gold economy math. Gold drops and chest prices both grow from GetStageScale,
 * so income and prices always scale from the same number.
 */
UCLASS()
class PATHFINDEROTHEREARTH_API UCurrencyStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Growth multiplier for a stage. Stage 1 = 1.0, each later stage multiplies by GrowthPerStage. */
	UFUNCTION(BlueprintPure, Category="Currency|Economy")
	static float GetStageScale(int32 Stage);

	/**
	 * Rolls an enemy's gold drop from its DT_EnemyRewards row.
	 * Returns 0 if the drop-chance roll fails or the row is missing.
	 * Impure on purpose: each call rolls new random numbers, so it must run once, on an exec wire.
	 */
	UFUNCTION(BlueprintCallable, Category="Currency|Economy")
	static int32 RollGoldDrop(const FDataTableRowHandle& RewardRow, int32 Stage);

	/** Chest price for this stage: BaseCost x StageScale ^ CostExponent, rounded. */
	UFUNCTION(BlueprintPure, Category="Currency|Economy")
	static int32 GetScaledCost(int32 BaseCost, int32 Stage);
};
