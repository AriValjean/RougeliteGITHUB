#include "Crafting/CraftingManagerComponent.h"
#include "Inventory/InventoryManagerComponent.h"
#include "Items/WeaponManagerComponent.h"
#include "Engine/DataTable.h"
#include "GameFramework/Actor.h"

UCraftingManagerComponent::UCraftingManagerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UCraftingManagerComponent::CanCraft(FName WeaponRow) const
{
	UWeaponManagerComponent* Weapons = GetWeapons();
	UInventoryManagerComponent* Inventory = GetInventory();
	if (!Weapons || !Inventory)
	{
		return false;
	}

	// Check 1: only one copy of each weapon can exist.
	if (Weapons->OwnsWeapon(WeaponRow))
	{
		return false;
	}

	// Check 2: the weapon must have a recipe.
	FRecipeRow Recipe;
	if (!GetRecipeForWeapon(WeaponRow, Recipe))
	{
		return false;
	}

	// Check 3: the previous-tier weapon must be owned (it is NOT consumed).
	if (!Recipe.RequiredWeaponRow.IsNone() && !Weapons->OwnsWeapon(Recipe.RequiredWeaponRow))
	{
		return false;
	}

	// Check 4: every material in the required amount.
	for (const FRecipeIngredient& Ingredient : Recipe.Materials)
	{
		if (Inventory->GetMaterialCount(Ingredient.MaterialID) < Ingredient.Count)
		{
			return false;
		}
	}

	return true;
}

bool UCraftingManagerComponent::Craft(FName WeaponRow)
{
	if (!CanCraft(WeaponRow))
	{
		return false;
	}

	FRecipeRow Recipe;
	GetRecipeForWeapon(WeaponRow, Recipe);

	UInventoryManagerComponent* Inventory = GetInventory();
	for (const FRecipeIngredient& Ingredient : Recipe.Materials)
	{
		if (!Inventory->RemoveMaterial(Ingredient.MaterialID, Ingredient.Count))
		{
			// Can only happen if a recipe lists the same material twice and the total is short.
			UE_LOG(LogTemp, Error, TEXT("Craft: could not remove %d x %s for %s"),
				Ingredient.Count, *Ingredient.MaterialID.ToString(), *WeaponRow.ToString());
			return false;
		}
	}

	GetWeapons()->AddOwnedWeapon(WeaponRow);
	return true;
}

bool UCraftingManagerComponent::GetRecipeForWeapon(FName WeaponRow, FRecipeRow& OutRecipe) const
{
	UWeaponManagerComponent* Weapons = GetWeapons();
	FWeaponData WeaponData;
	if (!Weapons || !Weapons->GetWeaponData(WeaponRow, WeaponData) || WeaponData.RecipeID.IsNone())
	{
		return false;
	}
	return GetRecipe(WeaponData.RecipeID, OutRecipe);
}

bool UCraftingManagerComponent::GetRecipe(FName RecipeID, FRecipeRow& OutRecipe) const
{
	if (!RecipeTable || RecipeID.IsNone())
	{
		return false;
	}

	const FRecipeRow* Row = RecipeTable->FindRow<FRecipeRow>(RecipeID, TEXT("GetRecipe"));
	if (!Row)
	{
		return false;
	}

	OutRecipe = *Row;
	return true;
}

bool UCraftingManagerComponent::OwnsWeapon(FName WeaponRow) const
{
	const UWeaponManagerComponent* Weapons = GetWeapons();
	return Weapons && Weapons->OwnsWeapon(WeaponRow);
}

UInventoryManagerComponent* UCraftingManagerComponent::GetInventory() const
{
	if (!CachedInventory && GetOwner())
	{
		CachedInventory = GetOwner()->FindComponentByClass<UInventoryManagerComponent>();
	}
	return CachedInventory;
}

UWeaponManagerComponent* UCraftingManagerComponent::GetWeapons() const
{
	if (!CachedWeapons && GetOwner())
	{
		CachedWeapons = GetOwner()->FindComponentByClass<UWeaponManagerComponent>();
	}
	return CachedWeapons;
}
