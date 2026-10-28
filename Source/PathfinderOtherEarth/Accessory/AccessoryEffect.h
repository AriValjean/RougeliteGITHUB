// AccessoryEffect.h
// Location: Source/PathfinderOtherEarth/Accessory/AccessoryEffect.h
//
// C++ base for BP_AccesoryEffect. Blueprint children implement the behaviour
// by overriding OnAcquired / OnStackChanged.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "AccessoryEffect.generated.h"

class UAccessoryManagerComponent;

UCLASS(Abstract, Blueprintable, BlueprintType)
class PATHFINDEROTHEREARTH_API UAccessoryEffect : public UObject
{
	GENERATED_BODY()

public:
	/** Called by the manager right after construction, before OnAcquired. */
	void InitializeEffect(UAccessoryManagerComponent* InManager, FName InAccessoryID);

	/** First time the player picks up this accessory. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Accessory Effect")
	void OnAcquired();

	/** Player picked up another copy. NewStackCount includes the new copy. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Accessory Effect")
	void OnStackChanged(int32 NewStackCount);

	UFUNCTION(BlueprintPure, Category = "Accessory Effect")
	UAccessoryManagerComponent* GetAccessoryManager() const { return OwningManager; }

	/** The actor that owns the accessory manager (the player). */
	UFUNCTION(BlueprintPure, Category = "Accessory Effect")
	AActor* GetOwningActor() const;

	UFUNCTION(BlueprintPure, Category = "Accessory Effect")
	FName GetAccessoryID() const { return AccessoryID; }

	/** Current stack count of this accessory. */
	UFUNCTION(BlueprintPure, Category = "Accessory Effect")
	int32 GetStackCount() const;

	/** Gives Blueprint children world context (timers, spawning, traces). */
	virtual UWorld* GetWorld() const override;

private:
	UPROPERTY()
	TObjectPtr<UAccessoryManagerComponent> OwningManager = nullptr;

	UPROPERTY()
	FName AccessoryID;
};
