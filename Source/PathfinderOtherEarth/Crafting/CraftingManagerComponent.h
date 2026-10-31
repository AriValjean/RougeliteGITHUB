#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Crafting/CraftingTypes.h"
#include "CraftingManagerComponent.generated.h"

class UDataTable;
class UInventoryManagerComponent;
class UWeaponManagerComponent;

/**
 * Crafting rules. BPC_CraftingManager becomes a data-only Blueprint child of this.
 *
 * Owns no data of its own: materials live in the inventory component,
 * weapon ownership lives in the weapon manager. This component only checks and moves them.
 *
 * Every function takes just the weapon row. The recipe ID is looked up from DT_WeaponData,
 * so a caller can never pass a recipe that belongs to a different weapon.
 */
UCLASS(ClassGroup=(Crafting), Blueprintable, meta=(BlueprintSpawnableComponent))
class PATHFINDEROTHEREARTH_API UCraftingManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCraftingManagerComponent();

	/**
	 * True when the player can craft this weapon right now:
	 * 1) not already owned, 2) it has a recipe, 3) the required previous weapon is owned,
	 * 4) every material is in the inventory in the required amount.
	 */
	UFUNCTION(BlueprintCallable, Category="Crafting")
	bool CanCraft(FName WeaponRow) const;

	/** Re-checks CanCraft, removes the materials, then adds the weapon to the weapon manager. */
	UFUNCTION(BlueprintCallable, Category="Crafting")
	bool Craft(FName WeaponRow);

	/** The recipe for a weapon (looked up through its Recipe ID). False if the weapon isn't craftable. */
	UFUNCTION(BlueprintCallable, Category="Crafting")
	bool GetRecipeForWeapon(FName WeaponRow, FRecipeRow& OutRecipe) const;

	/** Looks up a recipe directly by its row name in DT_RecipesCPP. */
	UFUNCTION(BlueprintCallable, Category="Crafting")
	bool GetRecipe(FName RecipeID, FRecipeRow& OutRecipe) const;

	/** Convenience forward to the weapon manager, for the owned star on crafting tiles. */
	UFUNCTION(BlueprintPure, Category="Crafting")
	bool OwnsWeapon(FName WeaponRow) const;

protected:
	/** DT_RecipesCPP. The picker only shows tables using the C++ RecipeRow struct. */
	UPROPERTY(EditDefaultsOnly, Category="Crafting",
		meta=(RequiredAssetDataTags="RowStructure=/Script/PathfinderOtherEarth.RecipeRow"))
	TObjectPtr<UDataTable> RecipeTable;

private:
	/** Sibling components on the same actor, found on first use. */
	UInventoryManagerComponent* GetInventory() const;
	UWeaponManagerComponent* GetWeapons() const;

	UPROPERTY(Transient)
	mutable TObjectPtr<UInventoryManagerComponent> CachedInventory;

	UPROPERTY(Transient)
	mutable TObjectPtr<UWeaponManagerComponent> CachedWeapons;
};
