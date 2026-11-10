// RoomFlowSubsystem.h
// Owns the run's room progression: World -> Stage -> Room.
// Lives as long as the Game Instance, so it survives every level change.
//
// Flow:
//   StartRun()          -> opens the first stage's Start room
//   NotifyRoomCleared() -> called by the room when it's cleared; rolls the portal options
//   GetPortalOptions()  -> what each portal should offer (index 0 = Portal A, 1 = Portal B)
//   TravelToPortal()    -> called by the portal the player uses; opens the next room
//   EndRun()            -> resets and returns to camp
//
// Rules (see the Room Flow Design doc):
//   Start room            -> one Combat portal
//   Below the threshold   -> Portal A Combat; Portal B Shop (chance, limited) or a second Combat
//   Threshold reached     -> Portal A Boss; Portal B Shop (guaranteed, unless you're in a shop)
//   Boss room             -> one portal to the next stage's Start room, or back to camp after the last stage
//   Combat and NPC rooms never repeat in a run; Start, Shop and Boss rooms may.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RoomFlow/RoomFlowTypes.h"
#include "RoomFlowSubsystem.generated.h"

class UDataTable;

/** One choice a portal can offer. */
USTRUCT(BlueprintType)
struct PATHFINDEROTHEREARTH_API FPortalOption
{
	GENERATED_BODY()

	/** Row name in DT_RoomsCPP. None when bEndsRun is true. */
	UPROPERTY(BlueprintReadOnly, Category = "Room Flow")
	FName RoomId;

	UPROPERTY(BlueprintReadOnly, Category = "Room Flow")
	ERoomType RoomType = ERoomType::Combat;

	/** True on the boss room's exit: using it moves to the next stage. */
	UPROPERTY(BlueprintReadOnly, Category = "Room Flow")
	bool bAdvancesStage = false;

	/** True on the last boss's exit: using it ends the run and returns to camp. */
	UPROPERTY(BlueprintReadOnly, Category = "Room Flow")
	bool bEndsRun = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPortalsReady, const TArray<FPortalOption>&, Options);

UCLASS()
class PATHFINDEROTHEREARTH_API URoomFlowSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** Starts a new run in WorldId (None = Starting World from Project Settings) and opens its first Start room.
	 *  Seed 0 = random seed. Returns false if the tables are missing data (see Output Log, LogRoomFlow). */
	UFUNCTION(BlueprintCallable, Category = "Room Flow")
	bool StartRun(FName WorldId = NAME_None, int32 Seed = 0);

	/** Called by the room's Room Director on BeginPlay. Starts a debug run if you pressed Play inside a room level,
	 *  and (development builds) prints the current room, stage and progress on screen and in the Output Log. */
	UFUNCTION(BlueprintCallable, Category = "Room Flow")
	void NotifyRoomEntered();

	/** Call when the current room is cleared. Counts the room, rolls the portal options and fires OnPortalsReady.
	 *  Safe to call more than once; only the first call per room does anything.
	 *  If no run is active (you pressed Play inside a room level), a debug run is started from that level. */
	UFUNCTION(BlueprintCallable, Category = "Room Flow")
	void NotifyRoomCleared();

	/** The options rolled for the current room. Empty until the room is cleared. */
	UFUNCTION(BlueprintPure, Category = "Room Flow")
	const TArray<FPortalOption>& GetPortalOptions() const { return PortalOptions; }

	/** Opens the room behind a portal option. Save run state BEFORE calling this. */
	UFUNCTION(BlueprintCallable, Category = "Room Flow")
	bool TravelToPortal(const FPortalOption& Option);

	/** Ends the run and resets all room-flow state. Opens the Camp Level from Project Settings if bTravelToCamp. */
	UFUNCTION(BlueprintCallable, Category = "Room Flow")
	void EndRun(bool bTravelToCamp = true);

	/** 1-based stage number within the current World (Forest = 1). Use this for stage scaling. */
	UFUNCTION(BlueprintPure, Category = "Room Flow")
	int32 GetStageNumber() const { return CurrentStageIndex + 1; }

	/** Stage number counted across Worlds in DT_WorldsCPP order (World 2's first stage = 5 if World 1 has 4).
	 *  Use this for difficulty and shop odds so later Worlds keep getting harder. */
	UFUNCTION(BlueprintPure, Category = "Room Flow")
	int32 GetOverallStageNumber() const;

	/** The run's seeded random stream. C++ systems (shop, waves) roll from this so a seed reproduces the run. */
	FRandomStream& GetRandomStream() { return Stream; }

	/** Stage row name, e.g. Forest. None if no run is active. */
	UFUNCTION(BlueprintPure, Category = "Room Flow")
	FName GetCurrentStageId() const;

	UFUNCTION(BlueprintPure, Category = "Room Flow")
	bool IsRunActive() const { return bRunActive; }

	UFUNCTION(BlueprintPure, Category = "Room Flow")
	bool IsCurrentRoomCleared() const { return bCurrentRoomCleared; }

	/** Fired by NotifyRoomCleared once the options are rolled. Portals listen to this. */
	UPROPERTY(BlueprintAssignable, Category = "Room Flow")
	FOnPortalsReady OnPortalsReady;

	// ---- Read-only state (visible in Blueprint for debugging) ----

	UPROPERTY(BlueprintReadOnly, Category = "Room Flow|State")
	FName CurrentWorld;

	UPROPERTY(BlueprintReadOnly, Category = "Room Flow|State")
	int32 CurrentStageIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Room Flow|State")
	FName CurrentRoomId;

	UPROPERTY(BlueprintReadOnly, Category = "Room Flow|State")
	ERoomType CurrentRoomType = ERoomType::Start;

	/** Rooms cleared in this stage (Start and Boss rooms don't count). Compared against MinRoomsBeforeBoss. */
	UPROPERTY(BlueprintReadOnly, Category = "Room Flow|State")
	int32 RoomsCompletedThisStage = 0;

	/** Rooms cleared this run, all stages (Start rooms don't count). For difficulty scaling later. */
	UPROPERTY(BlueprintReadOnly, Category = "Room Flow|State")
	int32 RoomsCompletedThisRun = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Room Flow|State")
	int32 RunSeed = 0;

private:
	void RollPortals();
	bool PickRoom(FName StageId, ERoomType Type, const TSet<FName>& Exclude, bool bAllowRepeatFallback, FName& OutRoomId);
	bool OpenRoom(FName RoomId);
	void ResetStageState();
	void EnsureRunForCurrentLevel();

	FName GetStageIdAt(int32 Index) const;
	int32 GetStageCount() const;

	bool bRunActive = false;
	bool bCurrentRoomCleared = false;
	int32 OptionalShopsThisStage = 0;

	/** Combat and NPC rooms already used this run. */
	TSet<FName> VisitedRooms;

	TArray<FPortalOption> PortalOptions;

	/** Every roll in the run comes from this, so a seed reproduces the run. */
	FRandomStream Stream;
};
