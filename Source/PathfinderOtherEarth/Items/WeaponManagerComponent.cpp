// WeaponManagerComponent.cpp

#include "WeaponManagerComponent.h"

#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "WeaponBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogWeaponManager, Log, All);

UWeaponManagerComponent::UWeaponManagerComponent()
{
	// Event-driven component: nothing to do every frame.
	PrimaryComponentTick.bCanEverTick = false;
}

// ----------------------------------------------------------------------
// Equipping
// ----------------------------------------------------------------------

bool UWeaponManagerComponent::EquipWeapon(FName WeaponRow)
{
	// --- Validate ---------------------------------------------------------
	if (!WeaponDataTable)
	{
		UE_LOG(LogWeaponManager, Error, TEXT("EquipWeapon: WeaponDataTable is not set on %s."), *GetNameSafe(this));
		return false;
	}

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

	static const FString Context(TEXT("UWeaponManagerComponent::EquipWeapon"));
	const FWeaponData* Data = WeaponDataTable->FindRow<FWeaponData>(WeaponRow, Context);
	if (!Data)
	{
		UE_LOG(LogWeaponManager, Warning, TEXT("EquipWeapon: no row '%s' in %s."), *WeaponRow.ToString(), *GetNameSafe(WeaponDataTable));
		return false;
	}

	if (!Data->WeaponActorClass)
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

	AWeaponBase* NewWeapon = World->SpawnActor<AWeaponBase>(Data->WeaponActorClass, Character->GetActorTransform(), SpawnParams);
	if (!NewWeapon)
	{
		UE_LOG(LogWeaponManager, Error, TEXT("EquipWeapon: failed to spawn %s."), *GetNameSafe(Data->WeaponActorClass));
		return false;
	}

	// Blueprint side: Initialize Weapon From Data Table + Attach To Character.
	NewWeapon->OnEquipped(Character, WeaponRow);

	// --- Record & broadcast -----------------------------------------------
	EquippedWeapon = NewWeapon;
	EquippedWeaponRow = WeaponRow;
	LastWeaponPerClass.Add(Data->OwnerClass, WeaponRow);

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

	// Fallback: the class's starter weapon. DT_ClassDefinitions row names match the enum names (Rogue, Ranger...).
	if (!ClassDefinitionTable)
	{
		UE_LOG(LogWeaponManager, Error, TEXT("EquipWeaponForClass: ClassDefinitionTable is not set on %s."), *GetNameSafe(this));
		return false;
	}

	const FName ClassRow(*StaticEnum<ECharacterClass>()->GetNameStringByValue(static_cast<int64>(CharacterClass)));

	static const FString Context(TEXT("UWeaponManagerComponent::EquipWeaponForClass"));
	const FClassDefinition* ClassDef = ClassDefinitionTable->FindRow<FClassDefinition>(ClassRow, Context);
	if (!ClassDef)
	{
		UE_LOG(LogWeaponManager, Warning, TEXT("EquipWeaponForClass: no class row '%s'."), *ClassRow.ToString());
		return false;
	}

	return EquipWeapon(ClassDef->StarterWeaponRow);
}

// ----------------------------------------------------------------------
// Collection
// ----------------------------------------------------------------------

void UWeaponManagerComponent::AddOwnedWeapon(FName WeaponRow)
{
	if (WeaponRow.IsNone())
	{
		return;
	}

	// Check first so the dispatcher only fires for weapons that are actually new.
	if (OwnedWeapons.Contains(WeaponRow))
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

FName UWeaponManagerComponent::GetLastWeaponForClass(ECharacterClass CharacterClass) const
{
	const FName* Found = LastWeaponPerClass.Find(CharacterClass);
	return Found ? *Found : NAME_None;
}

// ----------------------------------------------------------------------
// Cleanup
// ----------------------------------------------------------------------

void UWeaponManagerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
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
