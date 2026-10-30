#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "PathfinderEconomySettings.generated.h"

/**
 * Gold economy tuning. Edit in Project Settings > Game > Pathfinder Economy.
 * Saved to Config/DefaultGame.ini, so every system reads the same numbers.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Pathfinder Economy"))
class PATHFINDEROTHEREARTH_API UPathfinderEconomySettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** How much gold drops and chest prices grow per stage completed (RoR2 uses 1.15). */
	UPROPERTY(Config, EditAnywhere, Category="Stage Scaling", meta=(ClampMin="1.0"))
	float GrowthPerStage = 1.15f;

	/**
	 * Extra curve on chest prices. Above 1 means prices grow a bit faster than gold drops,
	 * so gold stays tight later in the run (RoR2 uses 1.25). Set to 1 for prices that grow
	 * exactly as fast as drops.
	 */
	UPROPERTY(Config, EditAnywhere, Category="Stage Scaling", meta=(ClampMin="1.0"))
	float CostExponent = 1.25f;
};
