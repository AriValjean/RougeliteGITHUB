// WaveDirectorComponent.h
// Lives on ARoomDirector (so every BP_RoomDirector has one). Runs Generated Waves in Combat rooms:
//   - Each wave buys enemies with a credit budget: squads first, then single cards to use up what's left.
//     Unspent credits carry over to the next wave.
//   - Waves overlap (bullet-hell pacing): the next wave starts once the current wave is fully spawned and
//     its living enemies are worth Next Wave Threshold or less of what the wave cost.
//   - Never more than Max Alive Enemies at once. Spawns are staggered (Spawn Interval) and telegraphed (Telegraph Time).
//   - The room can't clear until every wave has spawned and every enemy is dead.
//
// Budget per wave = Base Credits Per Wave
//                 x (1 + Stage Scaling x (overall stage - 1))
//                 x (1 + Room Scaling x rooms cleared this stage)
//                 x the room row's Budget Multiplier
//
// Cards / squads come from DT_EnemyCardsCPP / DT_SquadsCPP, filtered by the current stage.
// Rolls use the run's seeded stream (Room Flow Subsystem), so a seed reproduces the waves too.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Waves/WaveTypes.h"
#include "WaveDirectorComponent.generated.h"

class ASpawnPoint;
class UDataTable;
class UNiagaraSystem;
class URoomFlowSubsystem;
struct FStreamableHandle;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWaveStartedSignature, int32, WaveNumber, int32, WaveCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSpawnTelegraphSignature, FVector, Location, float, Duration);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAllWavesSpawnedSignature);

/** A squad that's valid in this room, with its members resolved. */
struct FWaveSquadEntry
{
	FName Id;
	TArray<TPair<FName, int32>> Members; // card ID, count
	float Weight = 1.f;
	int32 Cost = 0;
};

/** An enemy bought but not spawned yet. Members of one squad share a Group ID (and so a spawn point). */
struct FWavePendingSpawn
{
	FName CardId;
	int32 GroupId = 0;
	int32 WaveNumber = 0;
};

/** An enemy this director spawned. */
struct FWaveSpawnedEnemy
{
	TWeakObjectPtr<AActor> Actor;
	int32 WaveNumber = 0;
	int32 Cost = 0;
};

UCLASS(ClassGroup = (Room), meta = (BlueprintSpawnableComponent))
class PATHFINDEROTHEREARTH_API UWaveDirectorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWaveDirectorComponent();

	/** Called by the Room Director once placed enemies are gathered.
	 *  Starts waves only if this is a Combat room set to Generated Waves in DT_RoomsCPP. */
	void TryStartEncounter();

	/** Called by the Room Director whenever a registered enemy dies. */
	void NotifyEnemyDied();

	/** True from the start of the encounter until the last wave has fully spawned. The room can't clear while this is true. */
	UFUNCTION(BlueprintPure, Category = "Waves")
	bool IsEncounterActive() const { return bEncounterActive; }

	/** 1-based wave currently running (0 before the first wave). */
	UFUNCTION(BlueprintPure, Category = "Waves")
	int32 GetCurrentWave() const { return CurrentWave; }

	UFUNCTION(BlueprintPure, Category = "Waves")
	int32 GetWaveCount() const { return WaveCount; }

	/** Fired when a wave is bought and starts spawning. Use for a "Wave 2/3" UI or a sound sting. */
	UPROPERTY(BlueprintAssignable, Category = "Waves|Events")
	FOnWaveStartedSignature OnWaveStarted;

	/** Fired where an enemy is about to appear, Duration seconds before it does. Hook your own telegraph VFX/SFX here. */
	UPROPERTY(BlueprintAssignable, Category = "Waves|Events")
	FOnSpawnTelegraphSignature OnSpawnTelegraph;

	/** Fired once the last wave has fully spawned (the room clears when those enemies die). */
	UPROPERTY(BlueprintAssignable, Category = "Waves|Events")
	FOnAllWavesSpawnedSignature OnAllWavesSpawned;

	// ---------- Data ----------

	/** DT_EnemyCardsCPP (row type Enemy Spawn Card). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves|Data", meta = (RequiredAssetDataTags = "RowStructure=/Script/PathfinderOtherEarth.EnemySpawnCard"))
	TObjectPtr<UDataTable> EnemyCardTable;

	/** DT_SquadsCPP (row type Squad Card). Optional: without it the director only buys single cards. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves|Data", meta = (RequiredAssetDataTags = "RowStructure=/Script/PathfinderOtherEarth.SquadCard"))
	TObjectPtr<UDataTable> SquadTable;

	// ---------- Budget ----------

	/** Credits each wave gets in the first room of the first stage. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves|Budget", meta = (ClampMin = "1"))
	int32 BaseCreditsPerWave = 60;

	/** Extra budget per overall stage after the first (0.35 = +35% per stage). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves|Budget", meta = (ClampMin = "0.0"))
	float StageScaling = 0.35f;

	/** Extra budget per room already cleared in this stage (0.1 = +10% per room). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves|Budget", meta = (ClampMin = "0.0"))
	float RoomScaling = 0.1f;

	// ---------- Pacing ----------

	/** Next wave starts when the current wave's living enemies are worth this fraction of its cost or less (0.3 = 30% left). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves|Pacing", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float NextWaveThreshold = 0.3f;

	/** Waves never start closer together than this (seconds), so they can't all dump at once. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves|Pacing", meta = (ClampMin = "0.0"))
	float MinTimeBetweenWaves = 2.f;

	/** Most enemies alive (plus telegraphing) at once. Extra spawns wait until there's room. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves|Pacing", meta = (ClampMin = "1"))
	int32 MaxAliveEnemies = 20;

	/** Seconds between individual spawns, so a wave pours in instead of popping in on one frame. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves|Pacing", meta = (ClampMin = "0.01"))
	float SpawnInterval = 0.15f;

	// ---------- Telegraph ----------

	/** Seconds the warning shows before the enemy appears. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves|Telegraph", meta = (ClampMin = "0.0"))
	float TelegraphTime = 1.f;

	/** Optional Niagara effect played at each spawn spot during the telegraph (e.g. a glowing ground circle). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves|Telegraph")
	TObjectPtr<UNiagaraSystem> TelegraphEffect;

	/** Development builds: draw a red debug sphere at each telegraph (placeholder until the VFX exists). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves|Telegraph")
	bool bDrawDebugTelegraph = true;

	// ---------- Spawn points ----------

	/** Spawn points closer than this (cm) to the player are skipped when any farther point exists. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves|Spawn Points", meta = (ClampMin = "0.0"))
	float MinPlayerDistance = 800.f;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	URoomFlowSubsystem* GetRoomFlow() const;
	FRandomStream& GetStream();

	void BuildPools(FName StageId, int32 RoomInStage);
	void OnClassesLoaded();

	void StartNextWave();
	int32 BuyWave(int32 WaveNumber);

	void ProcessQueue();
	void FinishSpawn(FName CardId, int32 WaveNumber, FVector Location);

	/** Picks the spawn point for a group. The one function the EQS upgrade will replace. */
	ASpawnPoint* ChooseSpawnPoint(ESpawnPointType Type);

	void CheckProgress();
	void EvaluateNextWave();
	void FinishEncounter();

	int32 GetAliveCount();
	int32 GetAliveCreditsInWave(int32 WaveNumber);
	static bool IsEnemyAlive(const AActor* Enemy);

	bool bEncounterActive = false;
	bool bClassesLoaded = false;

	int32 WaveCount = 0;
	int32 CurrentWave = 0;
	int32 CurrentWaveSpent = 0;
	int32 CarriedCredits = 0;
	int32 Telegraphing = 0;
	int32 NextGroupId = 0;
	float WaveBudget = 0.f;
	float LastWaveStartTime = 0.f;

	/** Every card this room may spawn (singles + squad members). */
	TMap<FName, FEnemySpawnCard> Cards;

	/** Cards the director may buy on their own in this room. */
	TArray<FName> SingleCards;

	TArray<FWaveSquadEntry> Squads;
	TArray<FWavePendingSpawn> Queue;
	TArray<FWaveSpawnedEnemy> Spawned;

	TArray<TWeakObjectPtr<ASpawnPoint>> SpawnPoints;
	TMap<int32, TWeakObjectPtr<ASpawnPoint>> GroupPoints;

	FTimerHandle QueueTimer;
	FTimerHandle NextWaveTimer;

	/** Keeps the enemy classes loaded while the room is running. */
	TSharedPtr<FStreamableHandle> LoadHandle;

	/** Only used if there's no Room Flow Subsystem. */
	FRandomStream FallbackStream;
};
