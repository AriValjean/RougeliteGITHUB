// PathfinderMetaSave.h
// The save file for permanent progress. Only plain data lives here; the Meta subsystem copies in and out.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Items/WeaponManagerComponent.h"
#include "Inventory/InventoryTypes.h"
#include "PathfinderMetaSave.generated.h"

UCLASS()
class PATHFINDEROTHEREARTH_API UPathfinderMetaSave : public USaveGame
{
	GENERATED_BODY()

public:
	/** 1 = Memoria + upgrades. 2 = + weapons, class, materials. 3 = + elite/boss defeats. Older saves still load. */
	UPROPERTY()
	int32 SaveVersion = 3;

	/** Only currencies that do NOT reset each run (e.g. Memoria). */
	UPROPERTY()
	TMap<FName, int32> Currencies;

	/** Upgrade row name -> rank bought (0 = not bought). */
	UPROPERTY()
	TMap<FName, int32> UpgradeRanks;

	/** Owned weapons, current class, last weapon per class. bHasData false = use Class Defaults. */
	UPROPERTY()
	FWeaponManagerSaveData Weapons;

	/** False = no inventory saved yet (use defaults). */
	UPROPERTY()
	bool bHasInventory = false;

	/** Crafting material stacks. Item definitions are stored by asset path. */
	UPROPERTY()
	TArray<FInventoryEntry> Inventory;

	/** Elite / Boss enemy ID -> times defeated. */
	UPROPERTY()
	TMap<FName, int32> EnemyDefeats;
};
