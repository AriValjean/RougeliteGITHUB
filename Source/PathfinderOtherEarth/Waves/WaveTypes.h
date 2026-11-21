// WaveTypes.h
// Data types for generated enemy waves (the credit director).
// Enemy cards live in DT_EnemyCardsCPP, squads in DT_SquadsCPP.
// A card or squad is only used in the stages listed on its row.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameFramework/Pawn.h"
#include "WaveTypes.generated.h"

/** How a Combat room fills itself with enemies. One mode per room.
 *  Boss, Elite, Start, Shop and NPC rooms ignore this and always use their placed enemies.
 *  New values are only ever added at the END. */
UENUM(BlueprintType)
enum class EEncounterMode : uint8
{
	Generated UMETA(DisplayName = "Generated Waves"), // credit director spawns overlapping waves (default)
	Placed    UMETA(DisplayName = "Placed"),          // only the enemies placed in the level
	Authored  UMETA(DisplayName = "Authored")         // reserved: hand-made wave list (not built yet)
};

/** What an enemy does in a fight. Squads mix roles; EQS later uses it to pick good spawn spots. */
UENUM(BlueprintType)
enum class EEnemyRole : uint8
{
	Melee  UMETA(DisplayName = "Melee"),
	Ranged UMETA(DisplayName = "Ranged"),
	Tank   UMETA(DisplayName = "Tank"),
	Swarm  UMETA(DisplayName = "Swarm"),
	Flying UMETA(DisplayName = "Flying")
};

/** Where an enemy may spawn. Must match the type set on a Spawn Point in the level. */
UENUM(BlueprintType)
enum class ESpawnPointType : uint8
{
	Ground UMETA(DisplayName = "Ground"),
	Ledge  UMETA(DisplayName = "Ledge"),
	Air    UMETA(DisplayName = "Air")
};

/** One enemy type the director can buy. Row name = Card ID, e.g. Slime. */
USTRUCT(BlueprintType)
struct PATHFINDEROTHEREARTH_API FEnemySpawnCard : public FTableRowBase
{
	GENERATED_BODY()

	/** The enemy Blueprint to spawn (a child of BP_EnemyBase). Soft, so only the current stage's enemies get loaded. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card")
	TSoftClassPtr<APawn> EnemyClass;

	/** Credits it costs. Stronger enemies cost more. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card", meta = (ClampMin = "1"))
	int32 Cost = 10;

	/** Relative chance of being bought among the valid cards. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card", meta = (ClampMin = "0.0"))
	float Weight = 1.f;

	/** Stages (row names in DT_StagesCPP) this enemy appears in, e.g. Forest. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card")
	TArray<FName> Stages;

	/** Rooms that must be cleared in the stage before it can appear (0 = from the first room). Holds tough enemies back. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card", meta = (ClampMin = "0"))
	int32 MinRoomInStage = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card")
	EEnemyRole Role = EEnemyRole::Melee;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card")
	ESpawnPointType SpawnPointType = ESpawnPointType::Ground;
};

/** One line of a squad: which enemy card, and how many. */
USTRUCT(BlueprintType)
struct PATHFINDEROTHEREARTH_API FSquadMember
{
	GENERATED_BODY()

	/** A row in DT_EnemyCardsCPP. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Squad", meta = (RowType = "/Script/PathfinderOtherEarth.EnemySpawnCard"))
	FDataTableRowHandle Card;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Squad", meta = (ClampMin = "1"))
	int32 Count = 1;
};

/** A designed group the director buys as one package. Row name = Squad ID, e.g. SlimePack.
 *  Its cost is worked out automatically: the sum of each member card's Cost x Count. */
USTRUCT(BlueprintType)
struct PATHFINDEROTHEREARTH_API FSquadCard : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Squad")
	TArray<FSquadMember> Members;

	/** Relative chance of being bought among the valid squads. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Squad", meta = (ClampMin = "0.0"))
	float Weight = 1.f;

	/** Stages (row names in DT_StagesCPP) this squad appears in. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Squad")
	TArray<FName> Stages;

	/** Rooms that must be cleared in the stage before it can appear (0 = from the first room). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Squad", meta = (ClampMin = "0"))
	int32 MinRoomInStage = 0;
};
