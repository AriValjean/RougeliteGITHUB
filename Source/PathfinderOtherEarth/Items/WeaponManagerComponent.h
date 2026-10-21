// WeaponManagerComponent.h
// Owns the player's weapon collection and the currently equipped weapon actor.
// Replaces the Blueprint logic in BPC_WeaponManager (which becomes a thin Blueprint child of this class,
// used only to fill in the data tables and starting weapons in Class Defaults).
//
// Single source of truth:
//   - OwnedWeapons        : every weapon row the player owns (crafted or starter)
//   - EquippedWeapon/Row  : the weapon actor currently in the character's hands
//   - LastWeaponPerClass  : remembered so changing class re-equips that class's last weapon

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PathfinderTypes.h"
#include "WeaponManagerComponent.generated.h"

class AWeaponBase;
class UDataTable;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEquippedWeaponChanged, FName, WeaponRow);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnOwnedWeaponsChanged, FName, WeaponRow);

UCLASS(Blueprintable, ClassGroup = (Pathfinder), meta = (BlueprintSpawnableComponent))
class PATHFINDEROTHEREARTH_API UWeaponManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWeaponManagerComponent();

	// ------------------------------------------------------------------
	// Equipping
	// ------------------------------------------------------------------

	/**
	 * Destroys the current weapon actor, spawns the weapon for this DT_WeaponData row,
	 * tells it On Equipped, remembers it as its class's last weapon and fires OnEquippedWeaponChanged.
	 * Returns false (and changes nothing) if the row is missing, not owned, or has no WeaponActorClass.
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapons")
	bool EquipWeapon(FName WeaponRow);

	/**
	 * Equips the last weapon used with this class.
	 * If the class has none yet, equips the class's Starter Weapon Row from DT_ClassDefinitions.
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapons")
	bool EquipWeaponForClass(ECharacterClass CharacterClass);

	// ------------------------------------------------------------------
	// Collection
	// ------------------------------------------------------------------

	/** Adds a weapon to the collection (no duplicates) and fires OnOwnedWeaponsChanged. */
	UFUNCTION(BlueprintCallable, Category = "Weapons")
	void AddOwnedWeapon(FName WeaponRow);

	UFUNCTION(BlueprintPure, Category = "Weapons")
	bool OwnsWeapon(FName WeaponRow) const;

	// ------------------------------------------------------------------
	// Getters (read-only from outside: only this component changes its own state)
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Weapons")
	AWeaponBase* GetEquippedWeapon() const { return EquippedWeapon; }

	UFUNCTION(BlueprintPure, Category = "Weapons")
	FName GetEquippedWeaponRow() const { return EquippedWeaponRow; }

	UFUNCTION(BlueprintPure, Category = "Weapons")
	const TArray<FName>& GetOwnedWeapons() const { return OwnedWeapons; }

	/** Returns the remembered weapon row for a class, or None if that class hasn't equipped anything yet. */
	UFUNCTION(BlueprintPure, Category = "Weapons")
	FName GetLastWeaponForClass(ECharacterClass CharacterClass) const;

	// ------------------------------------------------------------------
	// Event dispatchers
	// ------------------------------------------------------------------

	/** Fired after a new weapon is equipped. Bind in the Weapons tab / HUD. */
	UPROPERTY(BlueprintAssignable, Category = "Weapons|Events")
	FOnEquippedWeaponChanged OnEquippedWeaponChanged;

	/** Fired after a weapon is added to the collection (e.g. crafted). */
	UPROPERTY(BlueprintAssignable, Category = "Weapons|Events")
	FOnOwnedWeaponsChanged OnOwnedWeaponsChanged;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** DT_WeaponData. The picker only shows tables using the C++ WeaponData row struct. */
	UPROPERTY(EditDefaultsOnly, Category = "Weapons|Data", meta = (RequiredAssetDataTags = "RowStructure=/Script/PathfinderOtherEarth.WeaponData"))
	TObjectPtr<UDataTable> WeaponDataTable;

	/** DT_ClassDefinitions. The picker only shows tables using the C++ ClassDefinition row struct. */
	UPROPERTY(EditDefaultsOnly, Category = "Weapons|Data", meta = (RequiredAssetDataTags = "RowStructure=/Script/PathfinderOtherEarth.ClassDefinition"))
	TObjectPtr<UDataTable> ClassDefinitionTable;

	/** Weapons the player owns. Fill the four class starter weapons in Class Defaults. */
	UPROPERTY(EditDefaultsOnly, Category = "Weapons")
	TArray<FName> OwnedWeapons;

private:
	/** Destroys the current weapon actor (after telling it On Unequipped). */
	void DestroyEquippedWeapon();

	UPROPERTY(Transient)
	TObjectPtr<AWeaponBase> EquippedWeapon;

	UPROPERTY()
	FName EquippedWeaponRow;

	UPROPERTY()
	TMap<ECharacterClass, FName> LastWeaponPerClass;
};
