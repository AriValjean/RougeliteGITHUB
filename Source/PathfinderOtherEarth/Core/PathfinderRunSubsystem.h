// PathfinderRunSubsystem.h
// Lives as long as the Game Instance, so data here survives level travel (camp -> run -> camp).
// Components copy their state in whenever it changes (and in EndPlay), and read it back in BeginPlay.
//
// Permanent data here (weapons, class, crafting materials) is also written to disk:
// UPathfinderMetaSubsystem copies it into the MetaProgress save file and loads it back on startup.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Items/WeaponManagerComponent.h"
#include "Inventory/InventoryTypes.h"
#include "PathfinderRunSubsystem.generated.h"

UCLASS()
class PATHFINDEROTHEREARTH_API UPathfinderRunSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** Weapon collection, current class and last weapon per class. Written/read by UWeaponManagerComponent. */
	UPROPERTY(BlueprintReadWrite, Category = "Run State")
	FWeaponManagerSaveData WeaponSave;

	/** False until the inventory has stored something, so a new game keeps BPC_InventoryManager's defaults. */
	UPROPERTY(BlueprintReadOnly, Category = "Run State")
	bool bHasInventoryData = false;

	/** Crafting material stacks. Written/read by UInventoryManagerComponent. Kept on death. */
	UPROPERTY(BlueprintReadOnly, Category = "Run State")
	TArray<FInventoryEntry> InventorySave;

	/** Clears the stored weapon data so the next level uses Class Defaults (e.g. starting a brand-new game). */
	UFUNCTION(BlueprintCallable, Category = "Run State")
	void ResetWeaponSave() { WeaponSave = FWeaponManagerSaveData(); }

	/** Clears stored weapons AND inventory (used by Meta.ResetSave / a new game). */
	UFUNCTION(BlueprintCallable, Category = "Run State")
	void ResetAll()
	{
		ResetWeaponSave();
		bHasInventoryData = false;
		InventorySave.Reset();
	}
};
