// EnemyInfoComponent.h
// Who an enemy is: tier (Standard / Elite / Boss), name, screen health bar, material drops and defeat tracking.
// Add it to BP_EnemyBase; BP_EliteBase / BP_BossBase (children) just change its settings.
//
// On death it (1) rolls the Material Drops from its rewards row straight into the player's inventory,
// (2) records Elite/Boss defeats in the save file, and (3) removes the screen health bar.
// Gold is still handled by BP_EnemyBase's GoldOnDeath for now.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DataTable.h"
#include "EnemyInfoComponent.generated.h"

class UHealthComponent;

UENUM(BlueprintType)
enum class EEnemyTier : uint8
{
	Standard UMETA(DisplayName = "Standard"),
	Elite    UMETA(DisplayName = "Elite"),
	Boss     UMETA(DisplayName = "Boss")
};

UCLASS(Blueprintable, ClassGroup = (Pathfinder), meta = (BlueprintSpawnableComponent))
class PATHFINDEROTHEREARTH_API UEnemyInfoComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEnemyInfoComponent();

	/** Unique ID used for defeat tracking (e.g. Boss_Garoran). Empty = the Blueprint's class name. */
	UFUNCTION(BlueprintPure, Category = "Enemy")
	FName GetEnemyId() const;

	UFUNCTION(BlueprintPure, Category = "Enemy")
	EEnemyTier GetTier() const { return Tier; }

	UFUNCTION(BlueprintPure, Category = "Enemy")
	FText GetDisplayName() const { return DisplayName; }

	UFUNCTION(BlueprintPure, Category = "Enemy")
	FText GetSubtitle() const { return Subtitle; }

	/** True if this enemy uses the screen-wide health bar (and should hide its small overhead bar). */
	UFUNCTION(BlueprintPure, Category = "Enemy")
	bool ShowsScreenHealthBar() const { return bShowScreenHealthBar; }

	UFUNCTION(BlueprintPure, Category = "Enemy")
	UHealthComponent* GetHealthComponent() const;

	/** Finds the info component on any actor. */
	UFUNCTION(BlueprintPure, Category = "Enemy")
	static UEnemyInfoComponent* FindEnemyInfo(const AActor* Actor);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	FName EnemyId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	EEnemyTier Tier = EEnemyTier::Standard;

	/** Shown on the screen health bar (Elite / Boss). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	FText DisplayName;

	/** Optional line under the name, e.g. "Warden of the Grove". Usually bosses only. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	FText Subtitle;

	/** Tick for bosses and elites: shows the screen-wide health bar while this enemy is alive. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	bool bShowScreenHealthBar = false;

	/** Row in DT_EnemyRewards (row struct EnemyRewardRow). Its Material Drops are rolled on death. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Rewards", meta = (RowType = "/Script/PathfinderOtherEarth.EnemyRewardRow"))
	FDataTableRowHandle RewardsRow;

private:
	UFUNCTION()
	void HandleDeath(AActor* Killer);

	void GrantMaterialDrops();
};
