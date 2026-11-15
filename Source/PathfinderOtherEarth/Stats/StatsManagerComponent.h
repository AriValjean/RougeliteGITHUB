// StatsManagerComponent.h
// Replaces the logic in BPC_StatsManager. BPC_StatsManager becomes a data-only child of
// this class (same as BPC_WeaponManager -> UWeaponManagerComponent).
//
// Players and enemies use the SAME class: the player's ClassStatsRow points at
// DT_ClassStatsCPP, an enemy's points at DT_EnemyStatsCPP.
//
// Total = (Base + Accessory Flat + Meta Flat) x (1 + Accessory Percent + Meta Percent)
// Meta bonuses come from the Enchantress upgrades (UPathfinderMetaSubsystem) and only apply
// when Apply Meta Bonuses is ticked (player only — enemies leave it off).
//
// Drop into: Source/PathfinderOtherEarth/Stats/StatsManagerComponent.h

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PathfinderStatTypes.h"
#include "StatsManagerComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnStatsChanged);

UCLASS(Blueprintable, ClassGroup = (Pathfinder), meta = (BlueprintSpawnableComponent))
class PATHFINDEROTHEREARTH_API UStatsManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UStatsManagerComponent();

	/**
	 * Which DataTable + row to load base stats from (DT_ClassStatsCPP for players,
	 * DT_EnemyStatsCPP for enemies). Same name as the old Blueprint variable on purpose.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	FDataTableRowHandle ClassStatsRow;

	/** Tick on the PLAYER's stats component only. Adds Enchantress (meta) upgrades into the totals and the Meta bucket. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	bool bApplyMetaBonuses = false;

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "Stats")
	FOnStatsChanged OnStatsChanged;

	/** Loaded from the row's Sprint Multiplier column by InitializeStats. Sprint speed = Movement Speed total x this. */
	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	float SprintMultiplier = 1.f;

	/** Loads base stats from ClassStatsRow and broadcasts OnStatsChanged. */
	UFUNCTION(BlueprintCallable, Category = "Stats")
	void InitializeStats();

	/** Adds a run modifier (accessory, level-up...) into the Flat or Percent map, then broadcasts OnStatsChanged. */
	UFUNCTION(BlueprintCallable, Category = "Stats")
	void AddModifier(ECharacterStatType StatType, EModifierMode Mode, float Amount);

	/** (Base + Flat) x (1 + Percent), including meta bonuses when enabled. Percent is a fraction, so 0.1 = +10%. */
	UFUNCTION(BlueprintPure, Category = "Stats")
	void GetStatTotal(ECharacterStatType Stat, float& Total) const;

	/** Base / Accessory / Meta / Total breakdown for the Stats tab. */
	UFUNCTION(BlueprintPure, Category = "Stats")
	FStatBreakdown GetStatBreakdown(ECharacterStatType StatType) const;

	/** Clears this run's Flat/Percent bonuses. Base stats and meta bonuses untouched. */
	UFUNCTION(BlueprintCallable, Category = "Stats")
	void ResetRunBonuses();

	/** True for Critical Rate, Critical Damage, Cooldown Reduction. */
	UFUNCTION(BlueprintPure, Category = "Stats")
	static bool IsPercentStat(ECharacterStatType StatType);

	/** C++ convenience wrapper around GetStatTotal. */
	float GetTotal(ECharacterStatType Stat) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	float GetBaseStat(ECharacterStatType StatType) const;

	/** Total without meta bonuses: (Base + Flat) x (1 + Percent). */
	float GetTotalWithoutMeta(ECharacterStatType Stat) const;

	/** Meta (Enchantress) bonus for one stat + mode, or 0 when Apply Meta Bonuses is off. */
	float GetMetaBonus(ECharacterStatType Stat, EModifierMode Mode) const;

	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	TMap<ECharacterStatType, float> BaseStats;

	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	TMap<ECharacterStatType, float> FlatBonuses;

	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	TMap<ECharacterStatType, float> PercentBonuses;

private:
	/** Meta upgrade bought or refunded -> totals changed. */
	UFUNCTION()
	void HandleMetaUpgradeChanged(FName UpgradeId, int32 NewRank);

	class UPathfinderMetaSubsystem* GetMetaSubsystem() const;
};
