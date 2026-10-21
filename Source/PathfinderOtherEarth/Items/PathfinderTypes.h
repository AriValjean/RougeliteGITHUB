// PathfinderTypes.h
// Core gameplay data types shared by C++ and Blueprints.
// Replaces the Blueprint assets: E_Classes + E_WeaponClass (merged), E_AttackDelivery,
// S_AttackSlot, S_PassiveRef, S_WeaponData, S_ClassDefinition.
// DisplayName metadata matches the old Blueprint field names so pins read the same in the editor.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "ItemDefinition.h" // EItemRarity. If ItemDefinition.h is in a subfolder, change this path (e.g. "Inventory/ItemDefinition.h").
#include "PathfinderTypes.generated.h"

class UAnimMontage;
class USoundBase;
class UNiagaraSystem;
class UStaticMesh;
class UTexture2D;
class AWeaponBase;

// ---------------------------------------------------------------------------
// Enums
// ---------------------------------------------------------------------------

/** Playable classes. One enum for both the character's class and a weapon's owner class. */
UENUM(BlueprintType)
enum class ECharacterClass : uint8
{
	None      UMETA(DisplayName = "None"),
	Rogue     UMETA(DisplayName = "Rogue"),
	Ranger    UMETA(DisplayName = "Ranger"),
	Wizard    UMETA(DisplayName = "Wizard"),
	Swordsman UMETA(DisplayName = "Swordsman")
};

/** How an attack slot delivers its hit. */
UENUM(BlueprintType)
enum class EAttackDelivery : uint8
{
	Projectile  UMETA(DisplayName = "Projectile"),
	WeaponSweep UMETA(DisplayName = "WeaponSweep"),
	AreaBurst   UMETA(DisplayName = "AreaBurst")
};

// ---------------------------------------------------------------------------
// Attack slot (one for ranged, one for melee on every weapon)
// ---------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct FAttackSlot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
	EAttackDelivery Delivery = EAttackDelivery::Projectile;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
	float Damage = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack", meta = (DisplayName = "Attack Speed"))
	float AttackSpeed = 0.f;

	/** Soft refs: loaded on equip in Apply Weapon Visuals. Index 0 = ranged montage; melee uses one per combo hit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack", meta = (DisplayName = "Attack Montage"))
	TArray<TSoftObjectPtr<UAnimMontage>> AttackMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack", meta = (DisplayName = "Trace Range"))
	float TraceRange = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack", meta = (DisplayName = "Projectile Class"))
	TSoftClassPtr<AActor> ProjectileClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack", meta = (DisplayName = "Burst Radius"))
	float BurstRadius = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack", meta = (DisplayName = "Burst Angle"))
	float BurstAngle = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
	float Knockback = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feedback", meta = (DisplayName = "Attack Sound"))
	TSoftObjectPtr<USoundBase> AttackSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feedback", meta = (DisplayName = "Muzzle FX"))
	TSoftObjectPtr<UNiagaraSystem> MuzzleFX;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feedback", meta = (DisplayName = "Muzzle Offset"))
	FVector MuzzleOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack", meta = (DisplayName = "Combo Count"))
	int32 ComboCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals", meta = (DisplayName = "Weapon Mesh"))
	TSoftObjectPtr<UStaticMesh> WeaponMesh;

	/** Added to the character's Crit Rate stat (fraction, 0.1 = 10%). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crit", meta = (DisplayName = "Crit Rate"))
	float CritRate = 0.f;

	/** Added to the character's Crit Damage multiplier (fraction). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crit", meta = (DisplayName = "Crit damage modifier"))
	float CritDamageModifier = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals", meta = (DisplayName = "Draw Socket"))
	FName DrawSocket;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals", meta = (DisplayName = "Holster Socket"))
	FName HolsterSocket;
};

// ---------------------------------------------------------------------------
// Passive effect reference
// ---------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct FPassiveRef
{
	GENERATED_BODY()

	/** Optional effect Blueprint for passives with unique behaviour. None = no special behaviour. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Passive", meta = (DisplayName = "Effect Class"))
	TSoftClassPtr<UObject> EffectClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Passive")
	float Magnitude = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Passive", meta = (MultiLine = true))
	FText Description;
};

// ---------------------------------------------------------------------------
// DT_WeaponData row (Row Name = weapon ID)
// ---------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct FWeaponData : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon", meta = (DisplayName = "Owner Class"))
	ECharacterClass OwnerClass = ECharacterClass::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon", meta = (DisplayName = "Display Name"))
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	EItemRarity Rarity = EItemRarity::Common;

	/** Row name in DT_Recipes. None = not craftable (starter weapon). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon", meta = (DisplayName = "Recipe ID"))
	FName RecipeID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon", meta = (MultiLine = true))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon", meta = (DisplayName = "PassiveEffect"))
	FPassiveRef PassiveEffect;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon", meta = (DisplayName = "Ranged Slot"))
	FAttackSlot RangedSlot;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon", meta = (DisplayName = "Melee Slot"))
	FAttackSlot MeleeSlot;

	/** Which weapon Blueprint EquipWeapon spawns for this row. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon", meta = (DisplayName = "WeaponActorClass"))
	TSubclassOf<AWeaponBase> WeaponActorClass;
};

// ---------------------------------------------------------------------------
// DT_ClassDefinitions row (Row Name = class name, e.g. "Rogue")
// ---------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct FClassDefinition : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Class")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Class", meta = (MultiLine = true))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Class")
	TSoftObjectPtr<UTexture2D> Icon;

	/** Row name in DT_ClassStats. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Class")
	FName BaseStatsRow;

	/** Row name in DT_WeaponData equipped when this class has no last-used weapon. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Class")
	FName StarterWeaponRow;
};
