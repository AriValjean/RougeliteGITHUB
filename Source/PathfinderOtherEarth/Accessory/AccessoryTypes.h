// AccessoryTypes.h
// Location: Source/PathfinderOtherEarth/Accessory/AccessoryTypes.h
//
// Data types for the accessory system.
//   FStatModifier  -> replaces Blueprint struct S_StatModifier
//   FAccessoryData -> replaces Blueprint struct S_Accessory (row struct for DT_AccessoriesCPP)

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "Items/ItemDefinition.h"        // EItemRarity
#include "Stats/PathfinderStatTypes.h"   // ECharacterStatType, EModifierMode
#include "AccessoryTypes.generated.h"

class UAccessoryEffect;

/**
 * One stat change granted by an accessory.
 * First pickup applies BaseValue; every extra stack applies PerStackValue.
 */
USTRUCT(BlueprintType)
struct PATHFINDEROTHEREARTH_API FStatModifier
{
	GENERATED_BODY()

	/** Which stat this modifies. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat Modifier")
	ECharacterStatType StatType = ECharacterStatType::MaxHealth;

	/** Flat or Percent. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat Modifier")
	EModifierMode Mode = EModifierMode::Flat;

	/** Applied the first time the accessory is picked up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat Modifier")
	float BaseValue = 0.f;

	/** Applied for each additional stack after the first. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat Modifier")
	float PerStackValue = 0.f;
};

/**
 * Row struct for the accessory data table.
 * Row Name is the canonical accessory ID (ItemID is kept so existing
 * Blueprint code that reads it keeps working).
 */
USTRUCT(BlueprintType)
struct PATHFINDEROTHEREARTH_API FAccessoryData : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Accessory")
	FName ItemID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Accessory")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Accessory", meta = (MultiLine = true))
	FText Description;

	/** Soft ref so loading the table doesn't load every icon. Use Set Brush from Soft Texture in UMG. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Accessory")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Accessory")
	EItemRarity Rarity = EItemRarity::Common;

	/** Weight used by the loot roll when picking inside a rarity tier. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Accessory|Loot", meta = (ClampMin = "0.0"))
	float DropWeight = 1.f;

	/** Optional behaviour object. Leave empty for pure-stat accessories. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Accessory|Effect")
	TSoftClassPtr<UAccessoryEffect> Effect;

	/** Max copies the player can hold. 0 = unlimited. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Accessory", meta = (ClampMin = "0"))
	int32 MaxStacks = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Accessory|Stats")
	TArray<FStatModifier> StatModifiers;

	/** Free values for the Effect blueprint to read (damage, radius, etc.). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Accessory|Effect")
	float Magnitude = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Accessory|Effect")
	float PerStack = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Accessory|Effect", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Chance = 0.f;
};
