#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "CurrencyTypes.generated.h"

/**
 * Row struct for DT_EnemyRewards. One row per enemy type.
 * Values are stage 1 numbers; GetStageScale grows them every stage.
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
};
