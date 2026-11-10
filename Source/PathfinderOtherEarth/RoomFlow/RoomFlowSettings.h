// RoomFlowSettings.h
// Project Settings > Game > Room Flow.
// Holds the room-flow data tables so no Blueprint has to hard-reference them.
// Read in C++ with GetDefault<UPathfinderRoomFlowSettings>().

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RoomFlowSettings.generated.h"

class UDataTable;
class UWorld;

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Room Flow"))
class PATHFINDEROTHEREARTH_API UPathfinderRoomFlowSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Row struct: FWorldRow. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Tables")
	TSoftObjectPtr<UDataTable> WorldsTable;

	/** Row struct: FStageRow. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Tables")
	TSoftObjectPtr<UDataTable> StagesTable;

	/** Row struct: FRoomRow. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Tables")
	TSoftObjectPtr<UDataTable> RoomsTable;

	/** World used when a run starts from camp (until World selection exists). */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Run")
	FName StartingWorld = TEXT("World1");

	/** Level opened when a run ends (death, or the last stage's boss is beaten). */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Run")
	TSoftObjectPtr<UWorld> CampLevel;

	/** Development builds only: show the current room, stage and progress on screen each time a room is entered. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Debug")
	bool bShowRoomDebugMessage = true;
};
