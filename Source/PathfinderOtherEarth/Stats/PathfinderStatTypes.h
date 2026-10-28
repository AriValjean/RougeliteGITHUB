// PathfinderStatTypes.h
// Core enums/structs for the Stats system. Mirrors the PathfinderTypes.h pattern
// already used by the Weapon/Items migration: data types live in C++, DataTables
// use these row structs directly, Blueprints only do content + UI.
//
// Drop into: Source/PathfinderOtherEarth/Stats/PathfinderStatTypes.h

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "PathfinderStatTypes.generated.h"

/**
 * Replaces the Blueprint enum E_StatTypes. Same 8 entries, same order, same display names.
 * (Named ECharacterStatType, not EStatType, because Engine's Stats.h already owns EStatType.)
 */
UENUM(BlueprintType)
enum class ECharacterStatType : uint8
{
	MaxHealth         UMETA(DisplayName = "Max Health"),
	Defense           UMETA(DisplayName = "Defense"),
	MovementSpeed     UMETA(DisplayName = "Movement Speed"),
	AttackDamage      UMETA(DisplayName = "Attack Damage"),
	AttackSpeed       UMETA(DisplayName = "Attack Speed"),
	CriticalRate      UMETA(DisplayName = "Critical Rate"),
	CriticalDamage    UMETA(DisplayName = "Critical Damage"),
	CooldownReduction UMETA(DisplayName = "Cooldown Reduction")
};

/** Replaces E_ModifierMode. Flat adds before the percent multiply, Percent after. */
UENUM(BlueprintType)
enum class EModifierMode : uint8
{
	Flat    UMETA(DisplayName = "Flat"),
	Percent UMETA(DisplayName = "Percent")
};

/**
 * Row struct for BOTH DT_ClassStats (rows: Rogue / Ranger / Wizard / Swordsman) and
 * DT_EnemyStatsV2 (rows: PracticeEnemy / Slime / Goblin ...). Both tables already share
 * the same 9 columns, so one struct covers both -- which table gets read is decided by
 * the DataTable picked in the component's ClassStatsRow handle, not by the struct.
 */
USTRUCT(BlueprintType)
struct FCharacterStatsRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	float BaseMaxHealth = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	float BaseDefense = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	float BaseMovementSpeed = 600.f;

	/** Sprint speed = Movement Speed total x this. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	float SprintMultiplier = 1.67f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	float BaseAttackDamage = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	float BaseAttackSpeed = 1.f;

	/** Fraction: 0.05 = 5%. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	float BaseCritRate = 0.05f;

	/** Multiplier: 1.5 = 150% damage on crit (matches ComputeHitDamage). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	float BaseCritDamage = 1.5f;

	/** Fraction: 0.1 = 10% cooldown reduction. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	float BaseCooldownReduction = 0.f;
};

/**
 * One stat's full breakdown for the Stats tab (W_StatRow):
 * "Total (Base) +(Accessory) +(Meta)"
 */
USTRUCT(BlueprintType)
struct FStatBreakdown
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	float Base = 0.f;

	/** This run's flat + percent accessory bonuses, expressed as a delta from Base. */
	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	float AccessoryBonus = 0.f;

	/** Reserved for camp meta progression. Always 0 for now. */
	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	float MetaBonus = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	float Total = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	bool bIsPercentDisplay = false;
};
