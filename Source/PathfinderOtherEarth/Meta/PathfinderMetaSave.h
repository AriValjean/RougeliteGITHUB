// PathfinderMetaSave.h
// The save file for permanent (meta) progress. Only plain data lives here; the subsystem copies in and out.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "PathfinderMetaSave.generated.h"

UCLASS()
class PATHFINDEROTHEREARTH_API UPathfinderMetaSave : public USaveGame
{
	GENERATED_BODY()

public:
	/** Bump this if the save layout ever changes, so old saves can be upgraded on load. */
	UPROPERTY()
	int32 SaveVersion = 1;

	/** Only currencies that do NOT reset each run (e.g. Memoria). */
	UPROPERTY()
	TMap<FName, int32> Currencies;

	/** Upgrade row name -> rank bought (0 = not bought). */
	UPROPERTY()
	TMap<FName, int32> UpgradeRanks;
};
