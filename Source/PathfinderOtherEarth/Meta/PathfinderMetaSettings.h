// PathfinderMetaSettings.h
// Project Settings > Game > Meta Progression. Points the meta subsystem at its data tables.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/DataTable.h"
#include "PathfinderMetaSettings.generated.h"

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Meta Progression"))
class PATHFINDEROTHEREARTH_API UPathfinderMetaSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** DT_CurrenciesCPP (row struct: CurrencyRow). */
	UPROPERTY(Config, EditAnywhere, Category = "Tables", meta = (RequiredAssetDataTags = "RowStructure=/Script/PathfinderOtherEarth.CurrencyRow"))
	TSoftObjectPtr<UDataTable> CurrencyTable;

	/** DT_MetaUpgradesCPP (row struct: MetaUpgradeRow). */
	UPROPERTY(Config, EditAnywhere, Category = "Tables", meta = (RequiredAssetDataTags = "RowStructure=/Script/PathfinderOtherEarth.MetaUpgradeRow"))
	TSoftObjectPtr<UDataTable> MetaUpgradeTable;

	/** Save file name for permanent progress (Saved/SaveGames/<name>.sav). Kept separate from any run save. */
	UPROPERTY(Config, EditAnywhere, Category = "Save")
	FString SaveSlotName = TEXT("MetaProgress");

	/**
	 * Editor only. Ticked = every Play In Editor session starts as a brand-new player (the PIE save is not loaded).
	 * Progress made during that session is still written to the PIE slot. Untick to keep progress between PIE sessions.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Save")
	bool bFreshSaveEachPIESession = false;

	/** Fraction of max health restored when a Death Defiance charge is used (Hades uses 50%). */
	UPROPERTY(Config, EditAnywhere, Category = "Death Defiance", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float DeathDefianceHealFraction = 0.5f;

	/** Seconds of invulnerability right after a Death Defiance revive, so the same attack can't kill again. 0 = none. */
	UPROPERTY(Config, EditAnywhere, Category = "Death Defiance", meta = (ClampMin = "0.0", ClampMax = "10.0", Units = "s"))
	float DeathDefianceInvulnerabilityTime = 1.5f;

	virtual FName GetCategoryName() const override { return TEXT("Game"); }
};
