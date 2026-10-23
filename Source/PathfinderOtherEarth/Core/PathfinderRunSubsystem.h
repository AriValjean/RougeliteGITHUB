// PathfinderRunSubsystem.h
// Lives as long as the Game Instance, so data here survives level travel (camp -> run -> camp).
// Components copy their state in when their level unloads and read it back when the next level starts.
// Holds the weapon manager's data now; other systems' run state moves here as they migrate to C++.
// (Permanent progression will additionally go into a SaveGame file later.)

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Items/WeaponManagerComponent.h"
#include "PathfinderRunSubsystem.generated.h"

UCLASS()
class PATHFINDEROTHEREARTH_API UPathfinderRunSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** Weapon collection, current class and last weapon per class. Written/read by UWeaponManagerComponent. */
	UPROPERTY(BlueprintReadWrite, Category = "Run State")
	FWeaponManagerSaveData WeaponSave;

	/** Clears the stored weapon data so the next level uses Class Defaults (e.g. starting a brand-new game). */
	UFUNCTION(BlueprintCallable, Category = "Run State")
	void ResetWeaponSave() { WeaponSave = FWeaponManagerSaveData(); }
};
