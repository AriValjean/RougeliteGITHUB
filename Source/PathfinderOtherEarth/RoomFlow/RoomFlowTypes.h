// RoomFlowTypes.h
// Data types for the run's room flow: World -> Stage -> Room.
// Rows are edited in DT_WorldsCPP, DT_StagesCPP and DT_RoomsCPP.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "Waves/WaveTypes.h"
#include "RoomFlowTypes.generated.h"

class UWorld;

/** What happens when the player arrives in a room. New values are only ever added at the END (saved tables keep working). */
UENUM(BlueprintType)
enum class ERoomType : uint8
{
	Combat UMETA(DisplayName = "Combat"),
	Shop   UMETA(DisplayName = "Shop"),
	Boss   UMETA(DisplayName = "Boss"),
	Start  UMETA(DisplayName = "Start"),  // fixed first room of each stage; doesn't count toward the boss threshold
	NPC    UMETA(DisplayName = "NPC"),    // non-combat character encounter (Hades "!" door); none built yet
	Elite  UMETA(DisplayName = "Elite")   // one handcrafted level per elite (mini-boss); offered on Portal B
};

/** One World, e.g. World1: Forest -> Pantheon -> Castle -> Eden. Row name = World ID. */
USTRUCT(BlueprintType)
struct PATHFINDEROTHEREARTH_API FWorldRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World")
	FText DisplayName;

	/** Stage IDs in play order. Each must match a row name in DT_StagesCPP. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World")
	TArray<FName> Stages;

	/** False = locked until progression unlocks it (e.g. World 2). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World")
	bool bUnlockedByDefault = true;
};

/** Rules for one Stage (theme). Row name = Stage ID, e.g. Forest. */
USTRUCT(BlueprintType)
struct PATHFINDEROTHEREARTH_API FStageRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage")
	FText DisplayName;

	/** Rooms to clear in this stage before the boss portal appears. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage", meta = (ClampMin = "1"))
	int32 MinRoomsBeforeBoss = 3;

	/** Chance (0-1) that the second portal offers a shop before the boss threshold. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage|Shop", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ShopChance = 0.35f;

	/** Optional shops allowed before the threshold. The pre-boss shop is always offered and does not count. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage|Shop", meta = (ClampMin = "0"))
	int32 MaxShopsBeforeBoss = 1;

	/**
	 * Elite portals guaranteed per stage. If not reached by the last room before the boss threshold,
	 * that room's Portal B is forced to be an Elite. Offered only: Portal A always lets the player skip it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage|Elite", meta = (ClampMin = "0"))
	int32 MinElitesPerStage = 1;

	/** Most Elite portals that can be offered in this stage. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage|Elite", meta = (ClampMin = "0"))
	int32 MaxElitesPerStage = 1;

	/** Chance (0-1) that Portal B is an Elite before the guarantee kicks in (so it isn't always at the same spot). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage|Elite", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EliteChance = 0.35f;
};

/** One handcrafted room map. Row name = Room ID, e.g. Forest_Beach. */
USTRUCT(BlueprintType)
struct PATHFINDEROTHEREARTH_API FRoomRow : public FTableRowBase
{
	GENERATED_BODY()

	/** The level to open when a portal leads here. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room")
	TSoftObjectPtr<UWorld> Level;

	/** Stage this room belongs to. Must match a row name in DT_StagesCPP. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room")
	FName Stage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room")
	ERoomType RoomType = ERoomType::Combat;

	/** Relative chance of being picked among valid rooms of the same stage and type (the elite/boss pool weight). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room", meta = (ClampMin = "0.0"))
	float Weight = 1.f;

	/** Elite / Boss rooms: who you'll face, shown on the portal before you enter. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room|Encounter")
	FText EncounterName;

	/** Elite / Boss rooms: the symbol shown on the portal. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room|Encounter")
	TSoftObjectPtr<UTexture2D> EncounterIcon;

	// ---------- Waves (Combat rooms only) ----------

	/** How this Combat room fills with enemies. Other room types always use their placed enemies. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room|Waves", meta = (EditCondition = "RoomType == ERoomType::Combat", EditConditionHides))
	EEncounterMode EncounterMode = EEncounterMode::Generated;

	/** Generated Waves: how many waves before the room can clear. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room|Waves", meta = (ClampMin = "1", EditCondition = "RoomType == ERoomType::Combat && EncounterMode == EEncounterMode::Generated", EditConditionHides))
	int32 WaveCount = 3;

	/** Generated Waves: scales this room's credit budget (1 = normal, 1.5 = a harder room, 0.75 = an easier one). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room|Waves", meta = (ClampMin = "0.1", EditCondition = "RoomType == ERoomType::Combat && EncounterMode == EEncounterMode::Generated", EditConditionHides))
	float BudgetMultiplier = 1.f;
};
