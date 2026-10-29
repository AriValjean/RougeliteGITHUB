// WeaponBase.h
// C++ parent for BP_WeaponBase (and through it every class weapon: BP_RogueWeaponBase, BP_WizardBase...).
//
// C++ owns the weapon "contract" (what every weapon can do) and the shared math (Compute Hit Damage).
// Blueprints own the look and feel: meshes, sockets, montages, FX and sounds live in the class weapon
// Blueprints, which override Draw / Holster / Fire / Swing.
//
// Drop into: Source/PathfinderOtherEarth/Items/WeaponBase.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WeaponBase.generated.h"

class ACharacter;

UCLASS(Abstract, Blueprintable)
class PATHFINDEROTHEREARTH_API AWeaponBase : public AActor
{
	GENERATED_BODY()

public:
	AWeaponBase();

	// ------------------------------------------------------------------
	// Lifetime (called by UWeaponManagerComponent)
	// ------------------------------------------------------------------

	/**
	 * Called by the weapon manager right after this weapon is spawned.
	 * Implement in BP_WeaponBase: call Initialize Weapon From Data Table, then Attach To Character.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Weapon", meta = (DisplayName = "On Equipped"))
	void OnEquipped(ACharacter* Character, FName WeaponRow);

	/**
	 * Called by the weapon manager just before this weapon is destroyed (weapon swap, character removed).
	 * Optional: use it to stop FX, sounds or timers. Leave unimplemented if there is nothing to clean up.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Weapon", meta = (DisplayName = "On Unequipped"))
	void OnUnequipped();

	// ------------------------------------------------------------------
	// Actions (every class weapon overrides these in its own Blueprint)
	//
	// BlueprintNativeEvent = callable from anywhere, overridable in Blueprint, and the empty
	// _Implementation below is the default. Using NativeEvent instead of ImplementableEvent
	// means a future C++ weapon class could also override them.
	// ------------------------------------------------------------------

	/** Move the weapon meshes from the holster sockets to the hands. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Weapon|Actions")
	void Draw();

	/** Move the weapon meshes from the hands back to the holster sockets. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Weapon|Actions")
	void Holster();

	/** Primary attack (left click): the Ranged attack slot. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Weapon|Actions")
	void Fire();

	/** Secondary attack (right click): the Melee attack slot. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Weapon|Actions")
	void Swing();

	// ------------------------------------------------------------------
	// Damage
	// ------------------------------------------------------------------

	/**
	 * Rolls a crit and returns the damage for one hit.
	 *   Crit chance     = owner's Critical Rate stat + Slot Crit Rate (clamped 0..1)
	 *   Crit multiplier = owner's Critical Damage stat (base 1.5) + Slot Crit Damage
	 *   Final Damage    = Base Damage x (crit ? Crit multiplier : 1)
	 *
	 * Deliberately NOT const and NOT pure: it rolls a random number, and pure nodes re-run
	 * every time a pin reads them, which would roll a different crit for each wire.
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Damage", meta = (ReturnDisplayName = "Final Damage"))
	float ComputeHitDamage(float BaseDamage, float SlotCritRate, float SlotCritDamage, bool& bIsCritical);

protected:
	virtual void Draw_Implementation() {}
	virtual void Holster_Implementation() {}
	virtual void Fire_Implementation() {}
	virtual void Swing_Implementation() {}
};
