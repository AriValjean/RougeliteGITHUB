// WeaponManagerComponent.cpp

#include "WeaponManagerComponent.h"

#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Core/PathfinderGameMode.h"
#include "Core/PathfinderRunSubsystem.h"
#include "Engine/GameInstance.h"
#include "WeaponBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogWeaponManager, Log, All);

UWeaponManagerComponent::UWeaponManagerComponent()
{
	// Event-driven component: nothing to do every frame.
	PrimaryComponentTick.bCanEverTick = false;
}

// ----------------------------------------------------------------------
// Player-facing
// ----------------------------------------------------------------------

bool UWeaponManagerComponent::CanEquipWeapon(FName WeaponRow, EEquipBlockReason& Reason) const
{
	FWeaponData Data;
	if (!GetWeaponData(WeaponRow, Data) || !Data.WeaponActorClass)
	{
		Reason = EEquipBlockReason::InvalidWeapon;
		return false;
	}

	if (!OwnsWeapon(WeaponRow))
	{
		Reason = EEquipBlockReason::NotOwned;
		return false;
	}

	if (Data.OwnerClass != CurrentClass)
	{
		Reason = EEquipBlockReason::WrongClass;
		return false;
	}

	if (WeaponRow == EquippedWeaponRow && IsValid(EquippedWeapon))
	{
		Reason = EEquipBlockReason::AlreadyEquipped;
		return false;
	}

	if (!LevelAllowsLoadoutChanges())
	{
		Reason = EEquipBlockReason::NotAllowedHere;
		return false;
	}

	Reason = EEquipBlockReason::None;
	return true;
}

bool UWeaponManagerComponent::TryEquipWeapon(FName WeaponRow, EEquipBlockReason& Reason)
{
	if (!CanEquipWeapon(WeaponRow, Reason))
	{
		return false;
	}
	return EquipWeapon(WeaponRow);
}

bool UWeaponManagerComponent::SetClass(ECharacterClass NewClass)
{
	const bool bChanged = (NewClass != CurrentClass);
	CurrentClass = NewClass;

	const bool bEquipped = EquipWeaponForClass(NewClass);

	if (bChanged)
	{
		OnClassChanged.Broadcast(NewClass);
	}
	return bEquipped;
}

// ----------------------------------------------------------------------
// Mechanics
// ----------------------------------------------------------------------

bool UWeaponManagerComponent::EquipWeapon(FName WeaponRow)
{
	// --- Validate ---------------------------------------------------------
	if (!OwnsWeapon(WeaponRow))
	{
		UE_LOG(LogWeaponManager, Warning, TEXT("EquipWeapon: '%s' is not owned."), *WeaponRow.ToString());
		return false;
	}

	// Already holding this exact weapon: nothing to swap.
	if (WeaponRow == EquippedWeaponRow && IsValid(EquippedWeapon))
	{
		return true;
	}

	FWeaponData Data;
	if (!GetWeaponData(WeaponRow, Data))
	{
		return false; // GetWeaponData logged why
	}

	if (!Data.WeaponActorClass)
	{
		UE_LOG(LogWeaponManager, Warning, TEXT("EquipWeapon: row '%s' has no WeaponActorClass."), *WeaponRow.ToString());
		return false;
	}

	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UWorld* World = GetWorld();
	if (!Character || !World)
	{
		UE_LOG(LogWeaponManager, Error, TEXT("EquipWeapon: owner is not a Character."));
		return false;
	}

	// --- Swap weapon ------------------------------------------------------
	DestroyEquippedWeapon();

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Character;        // Get Owner inside the weapon returns the character
	SpawnParams.Instigator = Character;   // Get Instigator returns the character (who did the damage)
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AWeaponBase* NewWeapon = World->SpawnActor<AWeaponBase>(Data.WeaponActorClass, Character->GetActorTransform(), SpawnParams);
	if (!NewWeapon)
	{
		UE_LOG(LogWeaponManager, Error, TEXT("EquipWeapon: failed to spawn %s."), *GetNameSafe(Data.WeaponActorClass));
		return false;
	}

	// Blueprint side: Initialize Weapon From Data Table + Attach To Character.
	NewWeapon->OnEquipped(Character, WeaponRow);

	// --- Record & broadcast -----------------------------------------------
	EquippedWeapon = NewWeapon;
	EquippedWeaponRow = WeaponRow;
	LastWeaponPerClass.Add(Data.OwnerClass, WeaponRow);

	OnEquippedWeaponChanged.Broadcast(WeaponRow);
	return true;
}

bool UWeaponManagerComponent::EquipWeaponForClass(ECharacterClass CharacterClass)
{
	// Last weapon this class used.
	const FName LastWeapon = GetLastWeaponForClass(CharacterClass);
	if (!LastWeapon.IsNone())
	{
		return EquipWeapon(LastWeapon);
	}

	// Fallback: the class's starter weapon.
	const FName Starter = GetStarterWeaponForClass(CharacterClass);
	if (Starter.IsNone())
	{
		return false; // GetStarterWeaponForClass logged why
	}
	return EquipWeapon(Starter);
}

FName UWeaponManagerComponent::GetStarterWeaponForClass(ECharacterClass CharacterClass) const
{
	if (!ClassDefinitionTable)
	{
		UE_LOG(LogWeaponManager, Error, TEXT("ClassDefinitionTable is not set on %s."), *GetNameSafe(this));
		return NAME_None;
	}

	// DT_ClassDefinitions row names match the enum names (Rogue, Ranger...).
	const FName ClassRow(*StaticEnum<ECharacterClass>()->GetNameStringByValue(static_cast<int64>(CharacterClass)));

	static const FString Context(TEXT("UWeaponManagerComponent::GetStarterWeaponForClass"));
	const FClassDefinition* ClassDef = ClassDefinitionTable->FindRow<FClassDefinition>(ClassRow, Context, false);
	if (!ClassDef)
	{
		UE_LOG(LogWeaponManager, Warning, TEXT("No class row '%s' in %s."), *ClassRow.ToString(), *GetNameSafe(ClassDefinitionTable));
		return NAME_None;
	}
	return ClassDef->StarterWeaponRow;
}

FName UWeaponManagerComponent::GetCompareWeaponRow(FName SelectedRow) const
{
	FWeaponData Selected;
	if (!GetWeaponData(SelectedRow, Selected))
	{
		return NAME_None;
	}

	// Last weapon used with the selected weapon's class; never used yet -> that class's starter weapon.
	FName Compare = GetLastWeaponForClass(Selected.OwnerClass);
	if (Compare.IsNone())
	{
		Compare = GetStarterWeaponForClass(Selected.OwnerClass);
	}

	// Comparing a weapon with itself is pointless: the card shows no differences.
	return (Compare == SelectedRow) ? NAME_None : Compare;
}

// ----------------------------------------------------------------------
// Collection
// ----------------------------------------------------------------------

void UWeaponManagerComponent::AddOwnedWeapon(FName WeaponRow)
{
	// Check first so the dispatcher only fires for weapons that are actually new.
	if (WeaponRow.IsNone() || OwnedWeapons.Contains(WeaponRow))
	{
		return;
	}

	OwnedWeapons.Add(WeaponRow);
	OnOwnedWeaponsChanged.Broadcast(WeaponRow);
}

bool UWeaponManagerComponent::OwnsWeapon(FName WeaponRow) const
{
	return OwnedWeapons.Contains(WeaponRow);
}

// ----------------------------------------------------------------------
// Persistence
// ----------------------------------------------------------------------

FWeaponManagerSaveData UWeaponManagerComponent::GetSaveData() const
{
	FWeaponManagerSaveData SaveData;
	SaveData.bHasData = true;
	SaveData.OwnedWeapons = OwnedWeapons;
	SaveData.LastWeaponPerClass = LastWeaponPerClass;
	SaveData.CurrentClass = CurrentClass;
	return SaveData;
}

void UWeaponManagerComponent::LoadSaveData(const FWeaponManagerSaveData& SaveData)
{
	if (!SaveData.bHasData)
	{
		return; // First level of a new game: keep Class Defaults.
	}

	OwnedWeapons = SaveData.OwnedWeapons;
	LastWeaponPerClass = SaveData.LastWeaponPerClass;
	CurrentClass = SaveData.CurrentClass;

	// None = "whole list changed", so bound widgets refresh.
	OnOwnedWeaponsChanged.Broadcast(NAME_None);
	OnClassChanged.Broadcast(CurrentClass);
}

// ----------------------------------------------------------------------
// Getters
// ----------------------------------------------------------------------

TArray<FName> UWeaponManagerComponent::GetOwnedWeaponsSorted() const
{
	TArray<FName> Sorted = OwnedWeapons;
	if (!WeaponDataTable)
	{
		return Sorted;
	}

	static const FString Context(TEXT("UWeaponManagerComponent::GetOwnedWeaponsSorted"));

	// Stable sort keeps the owned order for weapons that compare equal (e.g. rows missing from the table).
	Sorted.StableSort([this](const FName& A, const FName& B)
	{
		const FWeaponData* DataA = WeaponDataTable->FindRow<FWeaponData>(A, Context, false);
		const FWeaponData* DataB = WeaponDataTable->FindRow<FWeaponData>(B, Context, false);
		if (!DataA || !DataB)
		{
			return DataA != nullptr; // valid rows before missing ones
		}

		// 1. Class, in enum order (Rogue, Ranger, Wizard, Swordsman).
		if (DataA->OwnerClass != DataB->OwnerClass)
		{
			return static_cast<uint8>(DataA->OwnerClass) < static_cast<uint8>(DataB->OwnerClass);
		}

		// 2. Rarity, highest first.
		if (DataA->Rarity != DataB->Rarity)
		{
			return static_cast<uint8>(DataA->Rarity) > static_cast<uint8>(DataB->Rarity);
		}

		// 3. Display name, alphabetical.
		return DataA->DisplayName.CompareTo(DataB->DisplayName) < 0;
	});

	return Sorted;
}

FName UWeaponManagerComponent::GetLastWeaponForClass(ECharacterClass CharacterClass) const
{
	const FName* Found = LastWeaponPerClass.Find(CharacterClass);
	return Found ? *Found : NAME_None;
}

bool UWeaponManagerComponent::GetWeaponData(FName WeaponRow, FWeaponData& OutData) const
{
	if (!WeaponDataTable)
	{
		UE_LOG(LogWeaponManager, Error, TEXT("WeaponDataTable is not set on %s."), *GetNameSafe(this));
		return false;
	}

	static const FString Context(TEXT("UWeaponManagerComponent::GetWeaponData"));
	const FWeaponData* Found = WeaponDataTable->FindRow<FWeaponData>(WeaponRow, Context, /*bWarnIfRowMissing*/ false);
	if (!Found)
	{
		UE_LOG(LogWeaponManager, Warning, TEXT("No row '%s' in %s."), *WeaponRow.ToString(), *GetNameSafe(WeaponDataTable));
		return false;
	}

	OutData = *Found;
	return true;
}

// ----------------------------------------------------------------------
// Rules & cleanup
// ----------------------------------------------------------------------

bool UWeaponManagerComponent::LevelAllowsLoadoutChanges() const
{
	const UWorld* World = GetWorld();
	const APathfinderGameMode* GameMode = World ? World->GetAuthGameMode<APathfinderGameMode>() : nullptr;
	if (!GameMode)
	{
		UE_LOG(LogWeaponManager, Warning, TEXT("This level's Game Mode is not a PathfinderGameMode, so loadout changes are blocked."));
		return false;
	}
	return GameMode->AllowsLoadoutChanges();
}

static UPathfinderRunSubsystem* GetRunSubsystem(const UActorComponent* Component)
{
	const UWorld* World = Component ? Component->GetWorld() : nullptr;
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UPathfinderRunSubsystem>() : nullptr;
}

void UWeaponManagerComponent::BeginPlay()
{
	Super::BeginPlay();

	// Restore what the previous level stored. First level of a new game: bHasData is false, Class Defaults stay.
	if (UPathfinderRunSubsystem* RunState = GetRunSubsystem(this))
	{
		LoadSaveData(RunState->WeaponSave);
	}
}

void UWeaponManagerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Store state for the next level (portal travel, death/respawn).
	if (UPathfinderRunSubsystem* RunState = GetRunSubsystem(this))
	{
		RunState->WeaponSave = GetSaveData();
	}

	// The weapon is a separate actor: don't leave it floating in the level if the character goes away.
	DestroyEquippedWeapon();
	Super::EndPlay(EndPlayReason);
}

void UWeaponManagerComponent::DestroyEquippedWeapon()
{
	if (IsValid(EquippedWeapon))
	{
		EquippedWeapon->OnUnequipped();
		EquippedWeapon->Destroy();
	}

	EquippedWeapon = nullptr;
	EquippedWeaponRow = NAME_None;
}
