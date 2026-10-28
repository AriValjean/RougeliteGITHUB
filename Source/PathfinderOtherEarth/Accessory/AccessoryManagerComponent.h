// AccessoryManagerComponent.h
// Location: Source/PathfinderOtherEarth/Accessory/AccessoryManagerComponent.h
//
// Replaces the logic of BPC_AccessoryManager. BPC_AccessoryManager becomes a
// data-only Blueprint child of this class (it just sets AccessoryTable).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Accessory/AccessoryTypes.h"
#include "AccessoryManagerComponent.generated.h"

class UAccessoryEffect;
class UDataTable;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAccessoryChanged, FName, AccessoryID, int32, NewCount);

UCLASS(Blueprintable, ClassGroup = (Pathfinder), meta = (BlueprintSpawnableComponent))
class PATHFINDEROTHEREARTH_API UAccessoryManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAccessoryManagerComponent();

	/**
	 * Gives the player one copy of an accessory.
	 * First copy: creates the effect object (if any) and applies BaseValue stats.
	 * Extra copies: notifies the effect and applies PerStackValue stats.
	 * Returns false if the ID isn't in the table or MaxStacks is reached.
	 */
	UFUNCTION(BlueprintCallable, Category = "Accessories")
	bool AddAccessory(FName AccessoryID);

	UFUNCTION(BlueprintPure, Category = "Accessories")
	int32 GetStackCount(FName AccessoryID) const;

	UFUNCTION(BlueprintPure, Category = "Accessories")
	bool HasAccessory(FName AccessoryID) const;

	/** Looks up an accessory's row. Use this in widgets instead of Get Data Table Row. */
	UFUNCTION(BlueprintCallable, Category = "Accessories")
	bool GetAccessoryData(FName AccessoryID, FAccessoryData& OutData) const;

	/** Fires after every successful AddAccessory. HUD/accessory bar binds to this. */
	UPROPERTY(BlueprintAssignable, Category = "Accessories")
	FOnAccessoryChanged OnAccessoryChanged;

protected:
	/** Must use the FAccessoryData row struct. Set in BPC_AccessoryManager's Class Defaults. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Accessories",
		meta = (RequiredAssetDataTags = "RowStructure=/Script/PathfinderOtherEarth.AccessoryData"))
	TObjectPtr<UDataTable> AccessoryTable = nullptr;

	/** AccessoryID -> how many copies the player holds this run. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Accessories|Runtime")
	TMap<FName, int32> AccessoryCounts;

	/** AccessoryID -> live effect object (only for accessories that have one). */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Accessories|Runtime")
	TMap<FName, TObjectPtr<UAccessoryEffect>> AccessoryEffects;

private:
	const FAccessoryData* FindAccessoryRow(FName AccessoryID) const;

	/** Creates the effect object for a first pickup. Returns null for pure-stat accessories. */
	UAccessoryEffect* CreateEffect(const FAccessoryData& Data, FName AccessoryID);

	/** Pushes the row's stat modifiers into the owner's UStatsManagerComponent. */
	void ApplyStatModifiers(const FAccessoryData& Data, bool bFirstPickup) const;
};
