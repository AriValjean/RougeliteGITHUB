#include "Inventory/InventoryManagerComponent.h"
#include "Fragments/InventoryFragment_Stackable.h"
#include "Core/PathfinderRunSubsystem.h"
#include "Meta/PathfinderMetaSubsystem.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

UInventoryManagerComponent::UInventoryManagerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

// ---------------------------------------------------------------- Lifetime / persistence

void UInventoryManagerComponent::BeginPlay()
{
	Super::BeginPlay();

	// Restore materials from the previous level / the save file. New game: keep Class Defaults.
	if (const UPathfinderRunSubsystem* Run = GetRunSubsystem())
	{
		if (Run->bHasInventoryData)
		{
			InventoryItems = Run->InventorySave;
			OnInventoryChanged.Broadcast();
		}
	}

	if (UPathfinderMetaSubsystem* Meta = GetMetaSubsystem())
	{
		Meta->OnSaveReset.AddUniqueDynamic(this, &UInventoryManagerComponent::HandleSaveReset);
	}
}

void UInventoryManagerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UPathfinderRunSubsystem* Run = GetRunSubsystem())
	{
		Run->InventorySave = InventoryItems;
		Run->bHasInventoryData = true;
	}

	if (UPathfinderMetaSubsystem* Meta = GetMetaSubsystem())
	{
		Meta->OnSaveReset.RemoveDynamic(this, &UInventoryManagerComponent::HandleSaveReset);
	}

	Super::EndPlay(EndPlayReason);
}

void UInventoryManagerComponent::NotifyChanged()
{
	OnInventoryChanged.Broadcast();

	if (UPathfinderRunSubsystem* Run = GetRunSubsystem())
	{
		Run->InventorySave = InventoryItems;
		Run->bHasInventoryData = true;
	}
	if (UPathfinderMetaSubsystem* Meta = GetMetaSubsystem())
	{
		Meta->RequestSave();
	}
}

void UInventoryManagerComponent::HandleSaveReset()
{
	SetInventory(TArray<FInventoryEntry>());
}

UPathfinderRunSubsystem* UInventoryManagerComponent::GetRunSubsystem() const
{
	const UWorld* World = GetWorld();
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UPathfinderRunSubsystem>() : nullptr;
}

UPathfinderMetaSubsystem* UInventoryManagerComponent::GetMetaSubsystem() const
{
	const UWorld* World = GetWorld();
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UPathfinderMetaSubsystem>() : nullptr;
}

// ---------------------------------------------------------------- Items

void UInventoryManagerComponent::AddItem(TSubclassOf<UItemDefinition> ItemDefinition, int32 Count)
{
	if (!ItemDefinition || Count <= 0)
	{
		return;
	}

	const int32 MaxStack = GetMaxStackCount(ItemDefinition);
	int32 Remaining = Count;

	// 1) Top up existing stacks of this item.
	for (FInventoryEntry& Entry : InventoryItems)
	{
		if (Remaining <= 0)
		{
			break;
		}
		if (Entry.ItemDefinition == ItemDefinition && Entry.Count < MaxStack)
		{
			const int32 Added = FMath::Min(MaxStack - Entry.Count, Remaining);
			Entry.Count += Added;
			Remaining -= Added;
		}
	}

	// 2) Whatever is left goes into new slots, one full stack at a time.
	while (Remaining > 0)
	{
		const int32 Added = FMath::Min(MaxStack, Remaining);
		InventoryItems.Add(FInventoryEntry(ItemDefinition, Added));
		Remaining -= Added;
	}

	NotifyChanged();
	OnItemAdded.Broadcast(ItemDefinition, Count);
}

bool UInventoryManagerComponent::RemoveItem(TSubclassOf<UItemDefinition> ItemDefinition, int32 Amount)
{
	if (!ItemDefinition || Amount <= 0 || GetItemCount(ItemDefinition) < Amount)
	{
		return false;
	}

	int32 Remaining = Amount;

	// Walk backwards so RemoveAt doesn't skip slots.
	for (int32 Index = InventoryItems.Num() - 1; Index >= 0 && Remaining > 0; --Index)
	{
		FInventoryEntry& Entry = InventoryItems[Index];
		if (Entry.ItemDefinition != ItemDefinition || Entry.Count <= 0)
		{
			continue;
		}

		const int32 Taken = FMath::Min(Entry.Count, Remaining);
		Entry.Count -= Taken;
		Remaining -= Taken;

		if (Entry.Count <= 0)
		{
			InventoryItems.RemoveAt(Index);
		}
	}

	NotifyChanged();
	return true;
}

int32 UInventoryManagerComponent::GetItemCount(TSubclassOf<UItemDefinition> ItemDefinition) const
{
	if (!ItemDefinition)
	{
		return 0;
	}

	int32 Total = 0;
	for (const FInventoryEntry& Entry : InventoryItems)
	{
		if (Entry.ItemDefinition == ItemDefinition)
		{
			Total += Entry.Count;
		}
	}
	return Total;
}

bool UInventoryManagerComponent::AddMaterial(FName MaterialID, int32 Count)
{
	const TSubclassOf<UItemDefinition> Definition = ResolveMaterial(MaterialID);
	if (!Definition)
	{
		return false;
	}
	AddItem(Definition, Count);
	return true;
}

int32 UInventoryManagerComponent::GetMaterialCount(FName MaterialID) const
{
	return GetItemCount(ResolveMaterial(MaterialID));
}

bool UInventoryManagerComponent::RemoveMaterial(FName MaterialID, int32 Amount)
{
	return RemoveItem(ResolveMaterial(MaterialID), Amount);
}

void UInventoryManagerComponent::SetInventory(const TArray<FInventoryEntry>& NewItems)
{
	InventoryItems = NewItems;
	NotifyChanged();
}

int32 UInventoryManagerComponent::GetMaxStackCount(TSubclassOf<UItemDefinition> ItemDefinition)
{
	if (!ItemDefinition)
	{
		return 1;
	}

	const UInventoryFragment_Stackable* Stackable = Cast<UInventoryFragment_Stackable>(
		UItemDefinition::FindFragmentByClass(ItemDefinition, UInventoryFragment_Stackable::StaticClass()));

	// Never below 1, otherwise AddItem's new-slot loop could never finish.
	return Stackable ? FMath::Max(1, Stackable->MaxStackCount) : 1;
}

UInventoryManagerComponent* UInventoryManagerComponent::FindInventoryManager(const AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<UInventoryManagerComponent>() : nullptr;
}

TSubclassOf<UItemDefinition> UInventoryManagerComponent::ResolveMaterial(FName MaterialID) const
{
	if (!CraftingMaterialsTable || MaterialID.IsNone())
	{
		return nullptr;
	}

	const FCraftingMaterialRow* Row = CraftingMaterialsTable->FindRow<FCraftingMaterialRow>(MaterialID, TEXT("ResolveMaterial"));
	return Row ? Row->Definition.LoadSynchronous() : nullptr;
}
