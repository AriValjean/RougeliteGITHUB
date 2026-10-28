// AccessoryEffect.cpp
// Location: Source/PathfinderOtherEarth/Accessory/AccessoryEffect.cpp

#include "Accessory/AccessoryEffect.h"
#include "Accessory/AccessoryManagerComponent.h"
#include "GameFramework/Actor.h"

void UAccessoryEffect::InitializeEffect(UAccessoryManagerComponent* InManager, FName InAccessoryID)
{
	OwningManager = InManager;
	AccessoryID = InAccessoryID;
}

AActor* UAccessoryEffect::GetOwningActor() const
{
	return OwningManager ? OwningManager->GetOwner() : nullptr;
}

int32 UAccessoryEffect::GetStackCount() const
{
	return OwningManager ? OwningManager->GetStackCount(AccessoryID) : 0;
}

UWorld* UAccessoryEffect::GetWorld() const
{
	// The class default object has no real outer; returning null here is
	// what lets the editor detect that this class supports world context.
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return nullptr;
	}

	if (const UObject* Outer = GetOuter())
	{
		return Outer->GetWorld();
	}

	return nullptr;
}
