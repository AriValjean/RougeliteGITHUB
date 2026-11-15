// MetaTypes.h
// Data types for meta progression: currencies (DT_CurrenciesCPP) and Enchantress upgrades (DT_MetaUpgradesCPP).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "Stats/PathfinderStatTypes.h"
#include "MetaTypes.generated.h"

/** What a meta upgrade does. Code only looks at the effect, never at the row name. */
UENUM(BlueprintType)
enum class EMetaUpgradeEffect : uint8
{
	StatBonus        UMETA(DisplayName = "Stat Bonus"),
	DeathDefiance    UMETA(DisplayName = "Death Defiance"),
	FreeShopRerolls  UMETA(DisplayName = "Free Shop Rerolls")
};

/** One row per currency. Row name = currency ID (e.g. Memoria, later Gold). */
USTRUCT(BlueprintType)
struct PATHFINDEROTHEREARTH_API FCurrencyRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Currency")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Currency")
	TSoftObjectPtr<UTexture2D> Icon;

	/** True = wiped at the start of every run (Gold). False = kept forever and written to the save file (Memoria). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Currency")
	bool bResetsOnRunEnd = false;

	/** Only for currencies that reset each run: the amount every run starts with (e.g. starting Gold). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Currency", meta = (ClampMin = "0", EditCondition = "bResetsOnRunEnd"))
	int32 StartingAmount = 0;
};

/** One row per Enchantress upgrade. Row name = upgrade ID (e.g. DeathDefiance). */
USTRUCT(BlueprintType)
struct PATHFINDEROTHEREARTH_API FMetaUpgradeRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display", meta = (MultiLine = true))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	TSoftObjectPtr<UTexture2D> Icon;

	/** Lower numbers show first on the Enchantress screen. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	int32 SortOrder = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect")
	EMetaUpgradeEffect Effect = EMetaUpgradeEffect::StatBonus;

	/** Only used when Effect = Stat Bonus. Feeds the Meta bucket on the Stats page. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect", meta = (EditCondition = "Effect == EMetaUpgradeEffect::StatBonus", EditConditionHides))
	ECharacterStatType StatType = ECharacterStatType::MovementSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect", meta = (EditCondition = "Effect == EMetaUpgradeEffect::StatBonus", EditConditionHides))
	EModifierMode ModifierMode = EModifierMode::Percent;

	/** Row name in DT_CurrenciesCPP that pays for this upgrade. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ranks")
	FName CostCurrency = TEXT("Memoria");

	/** Cost of each rank, in order. The number of entries IS the max rank (3 entries = 3 ranks). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ranks")
	TArray<int32> RankCosts;

	/**
	 * TOTAL effect at each rank (not the increase per rank). Same number of entries as RankCosts.
	 * Stat Bonus + Percent: fractions (0.05 = +5%). Death Defiance: number of charges. Free Shop Rerolls: free rerolls per shop.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ranks")
	TArray<float> RankValues;
};
