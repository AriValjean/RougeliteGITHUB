// RoomFlowSubsystem.cpp

#include "RoomFlow/RoomFlowSubsystem.h"
#include "RoomFlow/RoomFlowSettings.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogRoomFlow, Log, All);

namespace RoomFlow
{
	const UPathfinderRoomFlowSettings* Settings() { return GetDefault<UPathfinderRoomFlowSettings>(); }
	UDataTable* Worlds() { return Settings()->WorldsTable.LoadSynchronous(); }
	UDataTable* Stages() { return Settings()->StagesTable.LoadSynchronous(); }
	UDataTable* Rooms()  { return Settings()->RoomsTable.LoadSynchronous(); }

	/** Combat and NPC rooms are one-per-run; Start, Shop and Boss rooms can repeat. */
	bool IsNoRepeatType(ERoomType Type) { return Type == ERoomType::Combat || Type == ERoomType::NPC; }
}

// ---------------------------------------------------------------- Run start / end

bool URoomFlowSubsystem::StartRun(FName WorldId, int32 Seed)
{
	if (WorldId.IsNone())
	{
		WorldId = RoomFlow::Settings()->StartingWorld;
	}

	UDataTable* WorldsTable = RoomFlow::Worlds();
	if (!WorldsTable || !WorldsTable->FindRow<FWorldRow>(WorldId, TEXT("StartRun")))
	{
		UE_LOG(LogRoomFlow, Error, TEXT("StartRun: World '%s' not found. Check Project Settings > Room Flow > Worlds Table."), *WorldId.ToString());
		return false;
	}

	RunSeed = (Seed != 0) ? Seed : FMath::Rand();
	Stream.Initialize(RunSeed);

	bRunActive = true;
	CurrentWorld = WorldId;
	CurrentStageIndex = 0;
	RoomsCompletedThisRun = 0;
	VisitedRooms.Reset();
	ResetStageState();

	FName StartRoom;
	if (!PickRoom(GetStageIdAt(0), ERoomType::Start, TSet<FName>(), true, StartRoom))
	{
		UE_LOG(LogRoomFlow, Error, TEXT("StartRun: no Start room for stage '%s' in DT_RoomsCPP."), *GetStageIdAt(0).ToString());
		bRunActive = false;
		return false;
	}

	UE_LOG(LogRoomFlow, Log, TEXT("Run started: World '%s', seed %d."), *CurrentWorld.ToString(), RunSeed);
	return OpenRoom(StartRoom);
}

void URoomFlowSubsystem::EndRun(bool bTravelToCamp)
{
	bRunActive = false;
	CurrentWorld = NAME_None;
	CurrentStageIndex = 0;
	CurrentRoomId = NAME_None;
	CurrentRoomType = ERoomType::Start;
	RoomsCompletedThisRun = 0;
	VisitedRooms.Reset();
	ResetStageState();

	UE_LOG(LogRoomFlow, Log, TEXT("Run ended."));

	if (bTravelToCamp)
	{
		const TSoftObjectPtr<UWorld>& Camp = RoomFlow::Settings()->CampLevel;
		if (Camp.IsNull())
		{
			UE_LOG(LogRoomFlow, Error, TEXT("EndRun: Camp Level is not set in Project Settings > Room Flow."));
			return;
		}
		UGameplayStatics::OpenLevelBySoftObjectPtr(GetGameInstance(), Camp);
	}
}

void URoomFlowSubsystem::ResetStageState()
{
	RoomsCompletedThisStage = 0;
	OptionalShopsThisStage = 0;
	bCurrentRoomCleared = false;
	PortalOptions.Reset();
}

// ---------------------------------------------------------------- Room entered (debug readout)

void URoomFlowSubsystem::NotifyRoomEntered()
{
	if (!bRunActive)
	{
		EnsureRunForCurrentLevel();
	}
	if (!bRunActive)
	{
		return;
	}

	int32 MinRooms = 0;
	if (UDataTable* StagesTable = RoomFlow::Stages())
	{
		if (const FStageRow* Stage = StagesTable->FindRow<FStageRow>(GetCurrentStageId(), TEXT("NotifyRoomEntered")))
		{
			MinRooms = Stage->MinRoomsBeforeBoss;
		}
	}

	const FString TypeName = StaticEnum<ERoomType>()->GetDisplayNameTextByValue(static_cast<int64>(CurrentRoomType)).ToString();

	// Which room this is on the way to the boss (Start and Boss rooms aren't counted).
	FString Progress;
	if (CurrentRoomType == ERoomType::Start)
	{
		Progress = TEXT("Start room");
	}
	else if (CurrentRoomType == ERoomType::Boss)
	{
		Progress = TEXT("Boss room");
	}
	else
	{
		Progress = FString::Printf(TEXT("Room %d of %d before boss"), RoomsCompletedThisStage + 1, MinRooms);
	}

	const FString Message = FString::Printf(TEXT("Room: %s (%s)  |  Stage %d: %s  |  %s  |  Seed %d"),
		*CurrentRoomId.ToString(), *TypeName, GetStageNumber(), *GetCurrentStageId().ToString(),
		*Progress, RunSeed);

	UE_LOG(LogRoomFlow, Log, TEXT("Entered -> %s"), *Message);

#if !UE_BUILD_SHIPPING
	if (GEngine && RoomFlow::Settings()->bShowRoomDebugMessage)
	{
		// Fixed key so each new room replaces the previous line instead of stacking.
		GEngine->AddOnScreenDebugMessage(7701, 8.f, FColor::Cyan, Message);
	}
#endif
}

// ---------------------------------------------------------------- Room cleared -> portals

void URoomFlowSubsystem::NotifyRoomCleared()
{
	if (!bRunActive)
	{
		EnsureRunForCurrentLevel();
	}
	if (!bRunActive || bCurrentRoomCleared)
	{
		return;
	}

	bCurrentRoomCleared = true;

	if (CurrentRoomType != ERoomType::Start)
	{
		++RoomsCompletedThisRun;
		if (CurrentRoomType != ERoomType::Boss)
		{
			++RoomsCompletedThisStage;
		}
	}

	RollPortals();

	UE_LOG(LogRoomFlow, Log, TEXT("Room '%s' cleared (%d this stage). %d portal option(s)."),
		*CurrentRoomId.ToString(), RoomsCompletedThisStage, PortalOptions.Num());

	OnPortalsReady.Broadcast(PortalOptions);
}

void URoomFlowSubsystem::RollPortals()
{
	PortalOptions.Reset();

	const FName StageId = GetStageIdAt(CurrentStageIndex);
	TSet<FName> Taken;

	// Adds one option of Type from this stage. Returns false if no room of that type is available.
	auto AddOption = [&](ERoomType Type, bool bAllowRepeatFallback) -> bool
	{
		FName RoomId;
		if (!PickRoom(StageId, Type, Taken, bAllowRepeatFallback, RoomId))
		{
			return false;
		}
		Taken.Add(RoomId);

		FPortalOption Option;
		Option.RoomId = RoomId;
		Option.RoomType = Type;
		PortalOptions.Add(Option);
		return true;
	};

	// Boss room: one exit, to the next stage's Start room or back to camp.
	if (CurrentRoomType == ERoomType::Boss)
	{
		FPortalOption Exit;
		if (CurrentStageIndex + 1 < GetStageCount())
		{
			FName NextStart;
			if (PickRoom(GetStageIdAt(CurrentStageIndex + 1), ERoomType::Start, TSet<FName>(), true, NextStart))
			{
				Exit.RoomId = NextStart;
				Exit.RoomType = ERoomType::Start;
				Exit.bAdvancesStage = true;
			}
			else
			{
				UE_LOG(LogRoomFlow, Error, TEXT("No Start room for stage '%s'. Ending the run instead."), *GetStageIdAt(CurrentStageIndex + 1).ToString());
				Exit.bEndsRun = true;
			}
		}
		else
		{
			Exit.bEndsRun = true;
		}
		PortalOptions.Add(Exit);
		return;
	}

	// Start room: a single Combat portal.
	if (CurrentRoomType == ERoomType::Start)
	{
		AddOption(ERoomType::Combat, true);
		return;
	}

	const FStageRow* Stage = nullptr;
	if (UDataTable* StagesTable = RoomFlow::Stages())
	{
		Stage = StagesTable->FindRow<FStageRow>(StageId, TEXT("RollPortals"));
	}
	const int32 MinRooms   = Stage ? Stage->MinRoomsBeforeBoss : 3;
	const float ShopChance = Stage ? Stage->ShopChance : 0.35f;
	const int32 MaxShops   = Stage ? Stage->MaxShopsBeforeBoss : 1;

	// Threshold reached: Boss + guaranteed last-chance Shop (not offered again from inside a shop).
	if (RoomsCompletedThisStage >= MinRooms)
	{
		AddOption(ERoomType::Boss, true);
		if (CurrentRoomType != ERoomType::Shop)
		{
			AddOption(ERoomType::Shop, true);
		}
		return;
	}

	// Below the threshold: Portal A is always Combat.
	AddOption(ERoomType::Combat, true);

	// Portal B: optional Shop if allowed and the roll passes...
	const bool bShopAllowed = OptionalShopsThisStage < MaxShops
		&& RoomsCompletedThisStage >= 1
		&& CurrentRoomType != ERoomType::Shop;

	if (bShopAllowed && Stream.FRand() < ShopChance && AddOption(ERoomType::Shop, true))
	{
		++OptionalShopsThisStage;
		return;
	}

	// ...otherwise a second, different Combat room. If none are left, Portal B stays hidden.
	AddOption(ERoomType::Combat, false);
}

bool URoomFlowSubsystem::PickRoom(FName StageId, ERoomType Type, const TSet<FName>& Exclude, bool bAllowRepeatFallback, FName& OutRoomId)
{
	UDataTable* RoomsTable = RoomFlow::Rooms();
	if (!RoomsTable)
	{
		UE_LOG(LogRoomFlow, Error, TEXT("Rooms Table is not set in Project Settings > Room Flow."));
		return false;
	}

	const bool bNoRepeat = RoomFlow::IsNoRepeatType(Type);

	// Pass 0 skips visited rooms; pass 1 (fallback) allows a repeat so the run never gets stuck.
	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		const bool bSkipVisited = bNoRepeat && Pass == 0;

		TArray<FName> Ids;
		TArray<float> Weights;
		float TotalWeight = 0.f;

		RoomsTable->ForeachRow<FRoomRow>(TEXT("PickRoom"), [&](const FName& Key, const FRoomRow& Row)
		{
			if (Row.Stage != StageId || Row.RoomType != Type || Row.Weight <= 0.f) return;
			if (Exclude.Contains(Key)) return;
			if (bSkipVisited && VisitedRooms.Contains(Key)) return;

			Ids.Add(Key);
			Weights.Add(Row.Weight);
			TotalWeight += Row.Weight;
		});

		if (Ids.Num() > 0)
		{
			float Roll = Stream.FRandRange(0.f, TotalWeight);
			for (int32 i = 0; i < Ids.Num(); ++i)
			{
				Roll -= Weights[i];
				if (Roll <= 0.f)
				{
					OutRoomId = Ids[i];
					return true;
				}
			}
			OutRoomId = Ids.Last();
			return true;
		}

		if (!bSkipVisited || !bAllowRepeatFallback)
		{
			break;
		}

		UE_LOG(LogRoomFlow, Warning, TEXT("No unused %s rooms left in stage '%s'; allowing a repeat. Add more rooms to DT_RoomsCPP."),
			*UEnum::GetValueAsString(Type), *StageId.ToString());
	}

	return false;
}

// ---------------------------------------------------------------- Travel

bool URoomFlowSubsystem::TravelToPortal(const FPortalOption& Option)
{
	if (!bRunActive)
	{
		UE_LOG(LogRoomFlow, Warning, TEXT("TravelToPortal called with no active run."));
		return false;
	}

	if (Option.bEndsRun)
	{
		EndRun(true);
		return true;
	}

	if (Option.bAdvancesStage)
	{
		++CurrentStageIndex;
		ResetStageState();
		UE_LOG(LogRoomFlow, Log, TEXT("Advancing to stage %d ('%s')."), GetStageNumber(), *GetCurrentStageId().ToString());
	}

	return OpenRoom(Option.RoomId);
}

bool URoomFlowSubsystem::OpenRoom(FName RoomId)
{
	UDataTable* RoomsTable = RoomFlow::Rooms();
	const FRoomRow* Row = RoomsTable ? RoomsTable->FindRow<FRoomRow>(RoomId, TEXT("OpenRoom")) : nullptr;
	if (!Row || Row->Level.IsNull())
	{
		UE_LOG(LogRoomFlow, Error, TEXT("OpenRoom: room '%s' is missing or has no Level set in DT_RoomsCPP."), *RoomId.ToString());
		return false;
	}

	CurrentRoomId = RoomId;
	CurrentRoomType = Row->RoomType;
	bCurrentRoomCleared = false;
	PortalOptions.Reset();

	if (RoomFlow::IsNoRepeatType(Row->RoomType))
	{
		VisitedRooms.Add(RoomId);
	}

	UE_LOG(LogRoomFlow, Log, TEXT("Opening room '%s' (%s)."), *RoomId.ToString(), *UEnum::GetValueAsString(Row->RoomType));
	UGameplayStatics::OpenLevelBySoftObjectPtr(GetGameInstance(), Row->Level);
	return true;
}

// ---------------------------------------------------------------- Lookups

FName URoomFlowSubsystem::GetCurrentStageId() const
{
	return bRunActive ? GetStageIdAt(CurrentStageIndex) : NAME_None;
}

int32 URoomFlowSubsystem::GetOverallStageNumber() const
{
	int32 StagesBefore = 0;
	if (UDataTable* WorldsTable = RoomFlow::Worlds())
	{
		// Rows are visited in table order, so earlier Worlds come first.
		bool bFound = false;
		WorldsTable->ForeachRow<FWorldRow>(TEXT("GetOverallStageNumber"), [&](const FName& Key, const FWorldRow& Row)
		{
			if (bFound) return;
			if (Key == CurrentWorld) { bFound = true; return; }
			StagesBefore += Row.Stages.Num();
		});
		if (!bFound)
		{
			StagesBefore = 0;
		}
	}
	return StagesBefore + GetStageNumber();
}

FName URoomFlowSubsystem::GetStageIdAt(int32 Index) const
{
	UDataTable* WorldsTable = RoomFlow::Worlds();
	const FWorldRow* World = WorldsTable ? WorldsTable->FindRow<FWorldRow>(CurrentWorld, TEXT("GetStageIdAt")) : nullptr;
	return (World && World->Stages.IsValidIndex(Index)) ? World->Stages[Index] : NAME_None;
}

int32 URoomFlowSubsystem::GetStageCount() const
{
	UDataTable* WorldsTable = RoomFlow::Worlds();
	const FWorldRow* World = WorldsTable ? WorldsTable->FindRow<FWorldRow>(CurrentWorld, TEXT("GetStageCount")) : nullptr;
	return World ? World->Stages.Num() : 0;
}

// ---------------------------------------------------------------- PIE convenience

void URoomFlowSubsystem::EnsureRunForCurrentLevel()
{
	// Lets you press Play directly inside a room level for testing: finds that level in DT_RoomsCPP
	// and starts a debug run from it, as if you had arrived there normally.
	UGameInstance* GI = GetGameInstance();
	UWorld* World = GI ? GI->GetWorld() : nullptr;
	UDataTable* RoomsTable = RoomFlow::Rooms();
	UDataTable* WorldsTable = RoomFlow::Worlds();
	if (!World || !RoomsTable || !WorldsTable)
	{
		return;
	}

	const FString CurrentPackage = UWorld::RemovePIEPrefix(World->GetPackage()->GetName());

	FName FoundRoom;
	const FRoomRow* FoundRow = nullptr;
	RoomsTable->ForeachRow<FRoomRow>(TEXT("EnsureRun"), [&](const FName& Key, const FRoomRow& Row)
	{
		if (!FoundRow && Row.Level.ToSoftObjectPath().GetLongPackageName() == CurrentPackage)
		{
			FoundRoom = Key;
			FoundRow = &Row;
		}
	});

	if (!FoundRow)
	{
		UE_LOG(LogRoomFlow, Warning, TEXT("No active run and level '%s' is not in DT_RoomsCPP."), *CurrentPackage);
		return;
	}

	// Find which World contains this room's stage.
	FName FoundWorld;
	int32 FoundStageIndex = INDEX_NONE;
	WorldsTable->ForeachRow<FWorldRow>(TEXT("EnsureRun"), [&](const FName& Key, const FWorldRow& Row)
	{
		const int32 Index = Row.Stages.IndexOfByKey(FoundRow->Stage);
		if (FoundStageIndex == INDEX_NONE && Index != INDEX_NONE)
		{
			FoundWorld = Key;
			FoundStageIndex = Index;
		}
	});

	if (FoundStageIndex == INDEX_NONE)
	{
		UE_LOG(LogRoomFlow, Warning, TEXT("Room '%s' uses stage '%s', which no World lists."), *FoundRoom.ToString(), *FoundRow->Stage.ToString());
		return;
	}

	RunSeed = FMath::Rand();
	Stream.Initialize(RunSeed);
	bRunActive = true;
	CurrentWorld = FoundWorld;
	CurrentStageIndex = FoundStageIndex;
	RoomsCompletedThisRun = 0;
	VisitedRooms.Reset();
	ResetStageState();

	CurrentRoomId = FoundRoom;
	CurrentRoomType = FoundRow->RoomType;
	if (RoomFlow::IsNoRepeatType(CurrentRoomType))
	{
		VisitedRooms.Add(FoundRoom);
	}

	UE_LOG(LogRoomFlow, Log, TEXT("Debug run started from level '%s' (room '%s', stage '%s')."),
		*CurrentPackage, *FoundRoom.ToString(), *FoundRow->Stage.ToString());
}
