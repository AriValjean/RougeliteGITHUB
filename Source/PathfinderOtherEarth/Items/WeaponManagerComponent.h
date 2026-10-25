// WeaponManagerComponent.h
// Owns the player's weapon collection, current class and the equipped weapon actor.
// BPC_WeaponManager is a data-only Blueprint child (data tables + starter weapons in Class Defaults).
//
// Single source of truth:
//   - OwnedWeapons        : every weapon row the player owns (crafted or starter)
//   - CurrentClass        : the player's class (class NPC calls SetClass)
//   - EquippedWeapon/Row  : the weapon actor currently in the character's hands
//   - LastWeaponPerClass  : remembered so changing class re-equips that class's last weapon
//
// Persistence: the character is recreated on every level load, so this component copies its state into
// UPathfinderRunSubsystem in EndPlay and copies it back in BeginPlay. Automatic: nothing to call before travel.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PathfinderTypes.h"
#include "WeaponManagerComponent.generated.h"

class AWeaponBase;
class UDataTable;

/** Why a player-requested equip was refused. The UI turns this into a lock icon / message. */
UENUM(BlueprintType)
enum class EEquipBlockReason : uint8
{
	None            UMETA(DisplayName = "None (can equip)"),
	InvalidWeapon   UMETA(DisplayName = "Invalid Weapon"),
	NotOwned        UMETA(DisplayName = "Not Owned"),
	WrongClass      UMETA(DisplayName = "Wrong Class"),
	NotAllowedHere  UMETA(DisplayName = "Not Allowed Here"),
	AlreadyEquipped UMETA(DisplayName = "Already Equipped")
};

/** Everything the weapon manager needs to survive a level change. Stored in the Game Instance. */
USTRUCT(BlueprintType)
struct FWeaponManagerSaveData
{
	GENERATED_BODY()

	/** False until the first save, so the first level uses Class Defaults instead of empty data. */
	UPROPERTY(BlueprintReadWrite, Category = "Weapons")
	bool bHasData = false;

	UPROPERTY(BlueprintReadWrite, Category = "Weapons")
	TArray<FName> OwnedWeapons;

	UPROPERTY(BlueprintReadWrite, Category = "Weapons")
	TMap<ECharacterClass, FName> LastWeaponPerClass;

	UPROPERTY(BlueprintReadWrite, Category = "Weapons")
	ECharacterClass CurrentClass = ECharacterClass::Rogue;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEquippedWeaponChanged, FName, WeaponRow);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnOwnedWeaponsChanged, FName, WeaponRow);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnClassChanged, ECharacterClass, NewClass);

UCLASS(Blueprintable, ClassGroup = (Pathfinder), meta = (BlueprintSpawnableComponent))
class PATHFINDEROTHEREARTH_API UWeaponManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWeaponManagerComponent();

	// ------------------------------------------------------------------
	// Player-facing (UI calls these)
	// ------------------------------------------------------------------

	/**
	 * Can the player equip this weapon right now? Checks: valid row, owned, matches current class,
	 * level allows loadout changes (Game Mode), not already equipped.
	 * Returns true when allowed; otherwise Reason says why.
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapons")
	bool CanEquipWeapon(FName WeaponRow, EEquipBlockReason& Reason) const;

	/** CanEquipWeapon, then EquipWeapon. Use this for the Weapons tab right-click. */
	UFUNCTION(BlueprintCallable, Category = "Weapons")
	bool TryEquipWeapon(FName WeaponRow, EEquipBlockReason& Reason);

	/** Sets the current class and equips that class's last-used (or starter) weapon. Class NPC calls this. */
	UFUNCTION(BlueprintCallable, Category = "Weapons")
	bool SetClass(ECharacterClass NewClass);

	// ------------------------------------------------------------------
	// Mechanics (no loadout rules: used by SetClass, BeginPlay and loading)
	// ------------------------------------------------------------------

	/**
	 * Destroys the current weapon actor, spawns the weapon for this DT_WeaponData row,
	 * tells it On Equipped, remembers it as its class's last weapon and fires OnEquippedWeaponChanged.
	 * Only checks ownership and data. Player-initiated swaps should use TryEquipWeapon.
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapons")
	bool EquipWeapon(FName WeaponRow);

	/** Equips the last weapon used with this class, or its Starter Weapon Row from DT_ClassDefinitions. */
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
	// Persistence
	// ------------------------------------------------------------------

	/** Copy of this component's state. Store it in the Game Instance before level travel. */
	UFUNCTION(BlueprintPure, Category = "Weapons|Save")
	FWeaponManagerSaveData GetSaveData() const;

	/**
	 * Restores state from the Game Instance. Does nothing if SaveData.bHasData is false (first level).
	 * Does not equip: call SetClass(Get Current Class) afterwards.
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapons|Save")
	void LoadSaveData(const FWeaponManagerSaveData& SaveData);

	// ------------------------------------------------------------------
	// Getters
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Weapons")
	AWeaponBase* GetEquippedWeapon() const { return EquippedWeapon; }

	UFUNCTION(BlueprintPure, Category = "Weapons")
	FName GetEquippedWeaponRow() const { return EquippedWeaponRow; }

	UFUNCTION(BlueprintPure, Category = "Weapons")
	const TArray<FName>& GetOwnedWeapons() const { return OwnedWeapons; }

	/**
	 * Owned weapons in display order: by class (Rogue, Ranger, Wizard, Swordsman),
	 * then rarity high to low (Legendary first), then display name. Use this for UI lists.
	 */
	UFUNCTION(BlueprintPure, Category = "Weapons")
	TArray<FName> GetOwnedWeaponsSorted() const;

	UFUNCTION(BlueprintPure, Category = "Weapons")
	ECharacterClass GetCurrentClass() const { return CurrentClass; }

	/** Returns the remembered weapon row for a class, or None if that class hasn't equipped anything yet. */
	UFUNCTION(BlueprintPure, Category = "Weapons")
	FName GetLastWeaponForClass(ECharacterClass CharacterClass) const;

	/** The class's Starter Weapon Row from DT_ClassDefinitions, or None if missing. */
	UFUNCTION(BlueprintPure, Category = "Weapons")
	FName GetStarterWeaponForClass(ECharacterClass CharacterClass) const;

	/**
	 * Which weapon the weapon card should compare SelectedRow against:
	 * the last-used weapon of the selected weapon's class (falls back to that class's starter weapon).
	 * Returns None when there is nothing useful to compare (the selected weapon IS that weapon, or no data).
	 */
	UFUNCTION(BlueprintPure, Category = "Weapons")
	FName GetCompareWeaponRow(FName SelectedRow) const;

	/** Looks up a row in DT_WeaponData. Handy for widgets (icon, rarity, owner class). */
	UFUNCTION(BlueprintCallable, Category = "Weapons")
	bool GetWeaponData(FName WeaponRow, FWeaponData& OutData) const;

	// ------------------------------------------------------------------
	// Event dispatchers
	// ------------------------------------------------------------------

	/** Fired after a new weapon is equipped. */
	UPROPERTY(BlueprintAssignable, Category = "Weapons|Events")
	FOnEquippedWeaponChanged OnEquippedWeaponChanged;

	/** Fired after a weapon is added to the collection (e.g. crafted). None = whole list replaced (loaded). */
	UPROPERTY(BlueprintAssignable, Category = "Weapons|Events")
	FOnOwnedWeaponsChanged OnOwnedWeaponsChanged;

	/** Fired after SetClass changes the class. */
	UPROPERTY(BlueprintAssignable, Category = "Weapons|Events")
	FOnClassChanged OnClassChanged;

protected:
	/** Restores state from UPathfinderRunSubsystem (runs before the character's own BeginPlay). */
	virtual void BeginPlay() override;

	/** Stores state into UPathfinderRunSubsystem, then destroys the weapon actor. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** DT_WeaponData. The picker only shows tables using the C++ WeaponData row struct. */
	UPROPERTY(EditDefaultsOnly, Category = "Weapons|Data", meta = (RequiredAssetDataTags = "RowStructure=/Script/PathfinderOtherEarth.WeaponData"))
	TObjectPtr<UDataTable> WeaponDataTable;

	/** DT_ClassDefinitions. The picker only shows tables using the C++ ClassDefinition row struct. */
	UPROPERTY(EditDefaultsOnly, Category = "Weapons|Data", meta = (RequiredAssetDataTags = "RowStructure=/Script/PathfinderOtherEarth.ClassDefinition"))
	TObjectPtr<UDataTable> ClassDefinitionTable;

	/** Weapons the player owns at the start of a new game. Fill the four class starter weapons in Class Defaults. */
	UPROPERTY(EditDefaultsOnly, Category = "Weapons")
	TArray<FName> OwnedWeapons;

	/** Class at the start of a new game. */
	UPROPERTY(EditDefaultsOnly, Category = "Weapons")
	ECharacterClass CurrentClass = ECharacterClass::Rogue;

private:
	/** True if the current level's Game Mode allows loadout changes. */
	bool LevelAllowsLoadoutChanges() const;

	/** Destroys the current weapon actor (after telling it On Unequipped). */
	void DestroyEquippedWeapon();

	UPROPERTY(Transient)
	TObjectPtr<AWeaponBase> EquippedWeapon;

	UPROPERTY()
	FName EquippedWeaponRow;

	UPROPERTY()
	TMap<ECharacterClass, FName> LastWeaponPerClass;
};
