// WeaponBase.h
// C++ parent for BP_WeaponBase.
// C++ (UWeaponManagerComponent) spawns weapons and calls OnEquipped / OnUnequipped.
// BP_WeaponBase implements those events in Blueprint, so meshes, montages and
// attach logic stay in Blueprint where they are easy to edit.

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
};
