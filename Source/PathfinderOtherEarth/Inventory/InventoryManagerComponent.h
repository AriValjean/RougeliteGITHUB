#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Inventory/InventoryTypes.h"
#include "InventoryManagerComponent.generated.h"

class UDataTable;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryChangedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnItemAddedSignature, TSubclassOf<UItemDefinition>, ItemDefinition, int32, Count);

/**
 * Owns the player's item stacks (crafting materials). BPC_InventoryManager is a data-only Blueprint child of this.
 *
 * Items only change through the functions below, so OnInventoryChanged always fires.
 *
 * Persistence: crafting materials are kept on death and when the game closes. Every change is copied into
 * UPathfinderRunSubsystem (survives level travel) and the Meta subsystem writes it to the save file.
 * BeginPlay restores it, so nothing needs to be saved or loaded from Blueprint.
 */
UCLASS(ClassGroup=(Inventory), Blueprintable, meta=(BlueprintSpawnableComponent))
class PATHFINDEROTHEREARTH_API UInventoryManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInventoryManagerComponent();

	/** Fires whenever slots are added, removed or their counts change. */
	UPROPERTY(BlueprintAssignable, Category="Inventory")
	FOnInventoryChangedSignature OnInventoryChanged;

	/** Fires when items are ADDED (drops, harvesting, debug) - not when restored or removed. Drives the "X obtained" log line. */
	UPROPERTY(BlueprintAssignable, Category="Inventory")
	FOnItemAddedSignature OnItemAdded;

	/** All slots. Read-only from Blueprint: change it through AddItem / RemoveItem / SetInventory. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Inventory")
	TArray<FInventoryEntry> InventoryItems;

	/** Adds items: tops up existing stacks first, then opens new slots of up to MaxStackCount each. */
	UFUNCTION(BlueprintCallable, Category="Inventory")
	void AddItem(TSubclassOf<UItemDefinition> ItemDefinition, int32 Count);

	/**
	 * Removes items, newest slots first. All-or-nothing: returns false and removes nothing
	 * if the inventory holds fewer than Amount.
	 */
	UFUNCTION(BlueprintCallable, Category="Inventory")
	bool RemoveItem(TSubclassOf<UItemDefinition> ItemDefinition, int32 Amount);

	/** Total count of an item across all slots. */
	UFUNCTION(BlueprintPure, Category="Inventory")
	int32 GetItemCount(TSubclassOf<UItemDefinition> ItemDefinition) const;

	/** Adds a crafting material by its DT_CraftingMaterialsCPP row name (e.g. Mat_Mushrooms). False if the row is unknown. */
	UFUNCTION(BlueprintCallable, Category="Inventory|Materials")
	bool AddMaterial(FName MaterialID, int32 Count);

	/** Count of a crafting material by its DT_CraftingMaterialsCPP row name (e.g. Mat_Mushrooms). */
	UFUNCTION(BlueprintPure, Category="Inventory|Materials")
	int32 GetMaterialCount(FName MaterialID) const;

	/** Removes a crafting material by row name. All-or-nothing, like RemoveItem. */
	UFUNCTION(BlueprintCallable, Category="Inventory|Materials")
	bool RemoveMaterial(FName MaterialID, int32 Amount);

	/** Replaces every slot at once. */
	UFUNCTION(BlueprintCallable, Category="Inventory")
	void SetInventory(const TArray<FInventoryEntry>& NewItems);

	/** Max stack size from the item's Stackable fragment. Items without one stack to 1. */
	UFUNCTION(BlueprintPure, Category="Inventory")
	static int32 GetMaxStackCount(TSubclassOf<UItemDefinition> ItemDefinition);

	/** Finds the inventory component on any actor (e.g. Get Player Character). */
	UFUNCTION(BlueprintPure, Category="Inventory")
	static UInventoryManagerComponent* FindInventoryManager(const AActor* Actor);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Material ID lookup table. Set to DT_CraftingMaterialsCPP in BPC_InventoryManager's Class Defaults. */
	UPROPERTY(EditDefaultsOnly, Category="Inventory|Materials",
		meta=(RequiredAssetDataTags="RowStructure=/Script/PathfinderOtherEarth.CraftingMaterialRow"))
	TObjectPtr<UDataTable> CraftingMaterialsTable;

private:
	/** Material row name -> item definition class. Null if the row or class is missing. */
	TSubclassOf<UItemDefinition> ResolveMaterial(FName MaterialID) const;

	/** Broadcasts On Inventory Changed, copies the slots into the Run subsystem and requests a save. */
	void NotifyChanged();

	/** Meta.ResetSave / new game: empty inventory. */
	UFUNCTION()
	void HandleSaveReset();

	class UPathfinderRunSubsystem* GetRunSubsystem() const;
	class UPathfinderMetaSubsystem* GetMetaSubsystem() const;
};
