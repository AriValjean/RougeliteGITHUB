#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "CraftingTypes.generated.h"

/** One material requirement. Replaces S_RecipleIngredient. */
USTRUCT(BlueprintType)
struct PATHFINDEROTHEREARTH_API FRecipeIngredient
{
	GENERATED_BODY()

	/** Row name in DT_CraftingMaterialsCPP (e.g. Mat_Mushrooms). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recipe", meta=(DisplayName="Material ID"))
	FName MaterialID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recipe", meta=(ClampMin="1"))
	int32 Count = 1;
};

/**
 * Row struct for DT_RecipesCPP. Replaces S_Recipe.
 * Row Name = the Recipe ID set on the weapon's DT_WeaponData row (e.g. R.001).
 */
USTRUCT(BlueprintType)
struct PATHFINDEROTHEREARTH_API FRecipeRow : public FTableRowBase
{
	GENERATED_BODY()

	/** DT_WeaponData row the player must already own (the previous tier). None = no weapon needed. Not consumed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recipe")
	FName RequiredWeaponRow;

	/** Materials consumed when crafting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recipe")
	TArray<FRecipeIngredient> Materials;
};
