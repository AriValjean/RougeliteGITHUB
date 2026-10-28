// AccessoryManagerComponent.cpp
// Location: Source/PathfinderOtherEarth/Accessory/AccessoryManagerComponent.cpp

#include "Accessory/AccessoryManagerComponent.h"
#include "Accessory/AccessoryEffect.h"
#include "Stats/StatsManagerComponent.h"
#include "Engine/DataTable.h"
#include "GameFramework/Actor.h"

DEFINE_LOG_CATEGORY_STATIC(LogAccessory, Log, All);

UAccessoryManagerComponent::UAccessoryManagerComponent()
{
	// Event-driven only; no Tick needed.
	PrimaryComponentTick.bCanEverTick = false;
}

bool UAccessoryManagerComponent::AddAccessory(FName AccessoryID)
{
	const FAccessoryData* Data = FindAccessoryRow(AccessoryID);
	if (!Data)
	{
		return false;
	}

	const int32* ExistingCount = AccessoryCounts.Find(AccessoryID);
	const bool bFirstPickup = (ExistingCount == nullptr);
	const int32 CurrentCount = bFirstPickup ? 0 : *ExistingCount;

	// Respect MaxStacks (0 = unlimited).
	if (Data->MaxStacks > 0 && CurrentCount >= Data->MaxStacks)
	{
		UE_LOG(LogAccessory, Verbose, TEXT("%s is at max stacks (%d)."), *AccessoryID.ToString(), Data->MaxStacks);
		return false;
	}

	// Compute the new count once; everything below reads this same value.
	const int32 NewCount = CurrentCount + 1;
	AccessoryCounts.Add(AccessoryID, NewCount);

	if (bFirstPickup)
	{
		if (UAccessoryEffect* Effect = CreateEffect(*Data, AccessoryID))
		{
			AccessoryEffects.Add(AccessoryID, Effect);
			Effect->OnAcquired();
		}
	}
	else if (const TObjectPtr<UAccessoryEffect>* FoundEffect = AccessoryEffects.Find(AccessoryID))
	{
		if (*FoundEffect)
		{
			(*FoundEffect)->OnStackChanged(NewCount);
		}
	}

	OnAccessoryChanged.Broadcast(AccessoryID, NewCount);
	ApplyStatModifiers(*Data, bFirstPickup);

	return true;
}

int32 UAccessoryManagerComponent::GetStackCount(FName AccessoryID) const
{
	const int32* Count = AccessoryCounts.Find(AccessoryID);
	return Count ? *Count : 0;
}

bool UAccessoryManagerComponent::HasAccessory(FName AccessoryID) const
{
	return AccessoryCounts.Contains(AccessoryID);
}

bool UAccessoryManagerComponent::GetAccessoryData(FName AccessoryID, FAccessoryData& OutData) const
{
	if (const FAccessoryData* Data = FindAccessoryRow(AccessoryID))
	{
		OutData = *Data;
		return true;
	}
	return false;
}

const FAccessoryData* UAccessoryManagerComponent::FindAccessoryRow(FName AccessoryID) const
{
	if (!AccessoryTable)
	{
		UE_LOG(LogAccessory, Warning, TEXT("%s has no AccessoryTable set."), *GetName());
		return nullptr;
	}

	// FindRow logs its own warning if the row is missing.
	return AccessoryTable->FindRow<FAccessoryData>(AccessoryID, TEXT("AccessoryManagerComponent"));
}

UAccessoryEffect* UAccessoryManagerComponent::CreateEffect(const FAccessoryData& Data, FName AccessoryID)
{
	if (Data.Effect.IsNull())
	{
		return nullptr; // Pure-stat accessory.
	}

	UClass* EffectClass = Data.Effect.LoadSynchronous();
	if (!EffectClass)
	{
		UE_LOG(LogAccessory, Warning, TEXT("Could not load effect class for %s."), *AccessoryID.ToString());
		return nullptr;
	}

	// Outer = this component, so the effect gets world context through GetWorld().
	UAccessoryEffect* Effect = NewObject<UAccessoryEffect>(this, EffectClass);
	Effect->InitializeEffect(this, AccessoryID);
	return Effect;
}

void UAccessoryManagerComponent::ApplyStatModifiers(const FAccessoryData& Data, bool bFirstPickup) const
{
	if (Data.StatModifiers.IsEmpty())
	{
		return;
	}

	AActor* Owner = GetOwner();
	UStatsManagerComponent* Stats = Owner ? Owner->FindComponentByClass<UStatsManagerComponent>() : nullptr;
	if (!Stats)
	{
		UE_LOG(LogAccessory, Warning, TEXT("%s: owner has no StatsManagerComponent."), *GetName());
		return;
	}

	for (const FStatModifier& Mod : Data.StatModifiers)
	{
		const float Amount = bFirstPickup ? Mod.BaseValue : Mod.PerStackValue;
		Stats->AddModifier(Mod.StatType, Mod.Mode, Amount);
	}
}
