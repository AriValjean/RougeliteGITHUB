// PathfinderGameMode.h
// C++ parent for the project's Game Modes (BP_ThirdPersonGameMode, BP_CampGameMode).
// The Game Mode is created when a level loads and destroyed when it unloads,
// so it holds the rules of the current level.
// Future home of room/portal/stage logic (spawn portal when room is cleared, pick next room, etc.).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PathfinderGameMode.generated.h"

UCLASS(Blueprintable)
class PATHFINDEROTHEREARTH_API APathfinderGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	/** True if the player may change class/weapon in this level. */
	UFUNCTION(BlueprintPure, Category = "Rules")
	bool AllowsLoadoutChanges() const { return bAllowLoadoutChanges; }

protected:
	/**
	 * Tick in the camp Game Mode (BP_CampGameMode). Leave off for run levels.
	 * Named after the rule, not the place, so a test room or mid-run armory can allow it too.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Rules")
	bool bAllowLoadoutChanges = false;
};
